/*
 * This file is part of jauto-profiler
 * Copyright (c) 2026 Bill Kozak
 *
 * This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "prof-server.h"

#include "util/ev-queue.h"
#include "prof-server-msg.h"
#include "prof-env.h"
#include "user-if.h"
#include "ps-uif-handler.h"
#include "prof-server-ev.h"
#include "uif-response.h"
#include "class-info.h"
#include "class-info-list.h"
#include "bytecode.h"
#include "jni/jni-system.h"
#include "jni/jni-profiler.h"
#include "jni/jni-util.h"
#include "jni/bc-instrument.h"
#include "ps-thread-state.h"
#include "instrument-handler.h"
#include "prof-err-log.h"
#include "util/log.h"

#include <jvmti.h>
#include <jni.h>
#include <pthread.h>
#include <semaphore.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#define LOG_TAG "prof-server"

struct prof_server {
	struct ev_queue *ev_q;
	struct user_if *uif;
	jvmtiEnv *jvmti;
	sem_t shutdown_sem;
	pthread_mutex_t resume_mutex;
	pthread_cond_t resume_cond;
	int paused;
	struct instr_ctx instr_ctx;
	int thread_running;
	jthread agent_thread;
	struct ps_thread_pause *thread_pause;
	struct prof_err_log err_log;
};


static void handle_usr_rq_loaded_classes(
	struct prof_server *ps,
	struct ps_msg *msg
) {
	struct user_if_client *client = msg->body.usr_rq_loaded_classes.client;

	free(msg);

	uif_respond_loaded_classes(
		ps->uif, client, &ps->instr_ctx.loaded_classes
	);
	uif_client_release(client);
}

static void handle_usr_rq_class_methods(
	struct prof_server *ps,
	struct ps_msg *msg
) {
	const char *class_name = msg->body.usr_rq_class_methods.class_name;
	struct class_info *ci = ci_list_find_by_name(
		&ps->instr_ctx.loaded_classes, class_name
	);

	uif_respond_class_methods(
		ps->uif, msg->body.usr_rq_class_methods.client, ci
	);
	ps_usr_rq_class_methods_dealloc(msg);
}

static void handle_class_loaded(
	struct prof_server *ps,
	JNIEnv *jni_env,
	struct ps_msg *msg
) {
	char *name = msg->body.class_loaded.name;
	unsigned char *bytecode = msg->body.class_loaded.bytecode;
	size_t bytecode_len = msg->body.class_loaded.bytecode_len;
	jobject loader = msg->body.class_loaded.loader;
	struct class_info_list *loaded_classes = &ps->instr_ctx.loaded_classes;
	struct method_list methods;
	struct class_info *ci;


	if (method_list_init(&methods, 0) != 0) {
		ps_send_class_ev_dealloc(jni_env, msg);
		return;
	}

	/* transfer loader ownership from msg to ci */
	msg->body.class_loaded.loader = NULL;

	bc_extract_methods(bytecode, bytecode_len, &methods);

	ci = ci_alloc(name, bytecode, bytecode_len, &methods, loader);
	if (ci == NULL) {
		jni_safe_free_global_obj(jni_env, loader);
		method_list_deep_destroy(&methods);
		goto exit;
	}

	int add_result = ci_list_add(loaded_classes, jni_env, ci);
	if (add_result == CI_LIST_ADD_ERR) {
		ci_free(ci, jni_env);
		goto exit;
	} else if (add_result == CI_LIST_ADD_DUPLICATE) {
		ih_reapply_instruments(
			&ps->instr_ctx, jni_env, ps->jvmti,
			&ps->err_log, ci
		);
	}

	ih_apply_deferred_instrumentations(
		&ps->instr_ctx, jni_env, ps->jvmti, &ps->err_log, ci
	);

exit:
	ps_send_class_ev_dealloc(jni_env, msg);
}

static void handle_usr_rq_instrument_method(
	struct prof_server *ps,
	JNIEnv *jni_env,
	struct ps_msg *msg
) {
	struct user_if_client *client = msg->body.usr_rq_instr_method.client;
	const char *class_name = msg->body.usr_rq_instr_method.class_name;
	const char *method_sig = msg->body.usr_rq_instr_method.method_sig;
	uint64_t instrument_id = 0;

	enum instrument_resp_status status = ih_instrument_method(
		&ps->instr_ctx, jni_env, ps->jvmti, class_name, method_sig,
		&instrument_id
	);

	uif_respond_instrument(ps->uif, client, status, instrument_id);
	ps_usr_rq_instrument_method_dealloc(msg);
}

void ps_handle_retransform(
	struct prof_server *ps,
	jvmtiEnv *jvmti,
	const char *name,
	jint *new_class_data_len,
	unsigned char **new_class_data
) {
	unsigned char *buf;
	jvmtiError err;

	if (ps->instr_ctx.pending.class_name == NULL) {
		return;
	}
	if (strcmp(name, (const char *)ps->instr_ctx.pending.class_name) != 0) {
		return;
	}

	err = (*jvmti)->Allocate(
		jvmti, ps->instr_ctx.pending.bytecode_len, &buf
	);
	if (err != JVMTI_ERROR_NONE) {
		LOG_ERROR("Allocate failed in retransform");
		return;
	}

	LOG_DEBUG(
		"retransform hook fired for %s, bytecode_len=%zu",
		name, ps->instr_ctx.pending.bytecode_len
	);

	memcpy(
		buf,
		(const void *)ps->instr_ctx.pending.bytecode,
		ps->instr_ctx.pending.bytecode_len
	);
	*new_class_data = buf;
	*new_class_data_len = (jint)ps->instr_ctx.pending.bytecode_len;
}

static void handle_usr_rq_get_stats(
	struct prof_server *ps,
	JNIEnv *jni_env,
	struct ps_msg *msg
) {
	struct user_if_client *client = msg->body.usr_rq_get_stats.client;
	uint8_t *stats_buf = NULL;
	size_t stats_len = 0;

	free(msg);

	if (jni_get_profiler_stats(jni_env, &stats_buf, &stats_len) != 0) {
		LOG_ERROR("jni_get_profiler_stats failed");
		uif_client_release(client);
		return;
	}

	uif_respond_get_stats(ps->uif, client, stats_buf, stats_len);
	free(stats_buf);
	uif_client_release(client);
}

static void handle_usr_rq_deinstrument_method(
	struct prof_server *ps,
	JNIEnv *jni_env,
	struct ps_msg *msg
) {
	const char *class_name =
		msg->body.usr_rq_deinstrument_method.class_name;
	const char *method_sig =
		msg->body.usr_rq_deinstrument_method.method_sig;
	struct user_if_client *client =
		msg->body.usr_rq_deinstrument_method.client;

	enum deinstrument_resp_status status = ih_deinstrument_method(
		&ps->instr_ctx, jni_env, ps->jvmti, class_name, method_sig
	);

	uif_respond_deinstrument(ps->uif, client, status);
	ps_usr_rq_deinstrument_method_dealloc(msg);
}

static void handle_usr_rq_deinstrument_by_id(
	struct prof_server *ps,
	JNIEnv *jni_env,
	struct ps_msg *msg
) {
	uint64_t instrument_id =
		msg->body.usr_rq_deinstrument_by_id.instrument_id;
	struct user_if_client *client =
		msg->body.usr_rq_deinstrument_by_id.client;

	free(msg);

	enum deinstrument_resp_status status = ih_deinstrument_by_id(
		&ps->instr_ctx, jni_env, ps->jvmti, instrument_id
	);

	uif_respond_deinstrument_by_id(ps->uif, client, status);
	uif_client_release(client);
}

static void handle_usr_rq_get_async_errors(
	struct prof_server *ps,
	struct ps_msg *msg
) {
	struct user_if_client *client =
		msg->body.usr_rq_get_async_errors.client;

	free(msg);

	uif_respond_get_async_errors(ps->uif, client, &ps->err_log);
	uif_client_release(client);
}

static void handle_usr_rq_list_instrumented(
	struct prof_server *ps,
	struct ps_msg *msg
) {
	struct user_if_client *client =
		msg->body.usr_rq_list_instrumented.client;

	free(msg);

	uif_respond_list_instrumented(
		ps->uif, client, ps->instr_ctx.master_instruments
	);
	uif_client_release(client);
}

static void handle_usr_rq_pause_threads(
	struct prof_server *ps,
	JNIEnv *jni_env,
	struct ps_msg *msg
) {
	struct user_if_client *client = msg->body.usr_rq_pause_threads.client;
	enum pause_threads_resp_status status;

	free(msg);

	switch (ps_thread_pause_suspend(
		ps->thread_pause, ps->jvmti, jni_env, ps->agent_thread
	)) {
	case PS_SUSPEND_OK:
		status = PAUSE_THREADS_RP_OK;
		break;
	case PS_SUSPEND_RACE:
		status = PAUSE_THREADS_RP_RACE_FAILURE;
		break;
	default:
		status = PAUSE_THREADS_RP_ERROR;
		break;
	}

	uif_respond_pause_threads(ps->uif, client, status);
	uif_client_release(client);
}

static void handle_usr_rq_instrument_line(
	struct prof_server *ps,
	JNIEnv *jni_env,
	struct ps_msg *msg
) {
	struct user_if_client *client =
		msg->body.usr_rq_instrument_line.client;
	const struct pstring *entry_class =
		msg->body.usr_rq_instrument_line.entry_class;
	const struct pstring *exit_class =
		msg->body.usr_rq_instrument_line.exit_class;
	uint32_t entry_line = msg->body.usr_rq_instrument_line.entry_line;
	uint32_t exit_line  = msg->body.usr_rq_instrument_line.exit_line;
	uint64_t instrument_id = 0;

	enum instrument_line_resp_status status = ih_instrument_line(
		&ps->instr_ctx, jni_env, ps->jvmti,
		entry_class, exit_class, entry_line, exit_line,
		&instrument_id
	);

	uif_respond_instrument_line(ps->uif, client, status, instrument_id);
	ps_usr_rq_instrument_line_dealloc(msg);
}

static int resume_vm(struct prof_server *ps)
{
	int was_paused;

	pthread_mutex_lock(&ps->resume_mutex);
	was_paused = ps->paused;
	if (ps->paused) {
		ps->paused = 0;
		pthread_cond_signal(&ps->resume_cond);
	}
	pthread_mutex_unlock(&ps->resume_mutex);

	return was_paused;
}

static void handle_shutdown_request(
	struct prof_server *ps,
	JNIEnv *jni_env,
	struct ps_msg *msg
) {
	int exit_code = msg->body.shutdown_request.exit_code;
	char errmsg[PSM_SHUTDOWN_REQUEST_MSG_MAX];

	memcpy(
		errmsg,
		msg->body.shutdown_request.msg,
		PSM_SHUTDOWN_REQUEST_MSG_MAX
	);
	free(msg);

	if (errmsg[0] != '\0') {
		LOG_WARN("shutdown requested: %s", errmsg);
	}

	/*
	 * When paused, the main thread is blocked in ps_wait_for_resume()
	 * on resume_cond. System.exit() would trigger Agent_OnUnload ->
	 * ps_destroy, which destroys that cond and then calls free(ps),
	 * leaving the thread permanently blocked in a futex on freed memory.
	 * The JVM then cannot complete halt() and the process hangs.
	 *
	 * Use _exit() instead: it bypasses the Java shutdown sequence,
	 * terminates all threads via exit_group, and lets the OS reclaim
	 * resources. No user code runs.
	 */
	pthread_mutex_lock(&ps->resume_mutex);
	int is_paused = ps->paused;
	pthread_mutex_unlock(&ps->resume_mutex);

	if (is_paused) {
		_exit(exit_code);
	}

	jni_system_exit(jni_env, exit_code);
}

static void handle_usr_rq_resume(
	struct prof_server *ps,
	JNIEnv *jni_env,
	struct ps_msg *msg
) {
	struct user_if_client *client = msg->body.usr_rq_resume.client;
	enum resume_resp_status status;

	free(msg);

	if (resume_vm(ps)) {
		status = RESUME_RP_UNBLOCKED;
	} else if (ps->thread_pause->suspended_count > 0) {
		if (ps_thread_pause_resume(
			ps->thread_pause, ps->jvmti, jni_env
		) == 0) {
			status = RESUME_RP_UNBLOCKED;
		} else {
			status = RESUME_RP_ERROR;
		}
	} else {
		status = RESUME_RP_NOCHANGE;
	}

	uif_respond_resume(ps->uif, client, status);
	uif_client_release(client);
}

static int dispatch(struct prof_server *ps, JNIEnv *jni_env, void *raw)
{
	struct ps_msg *msg = (struct ps_msg *)raw;
	int ret = 1;

	switch (msg->type) {
	case CLASS_LOADED:
		handle_class_loaded(ps, jni_env, msg);
		break;
	case USR_RQ_LOADED_CLASSES:
		handle_usr_rq_loaded_classes(ps, msg);
		break;
	case USR_RQ_CLASS_METHODS:
		handle_usr_rq_class_methods(ps, msg);
		break;
	case USR_RQ_INSTRUMENT_METHOD:
		handle_usr_rq_instrument_method(ps, jni_env, msg);
		break;
	case USR_RQ_GET_STATS:
		handle_usr_rq_get_stats(ps, jni_env, msg);
		break;
	case USR_RQ_DEINSTRUMENT_METHOD:
		handle_usr_rq_deinstrument_method(ps, jni_env, msg);
		break;
	case USR_RQ_RESUME:
		handle_usr_rq_resume(ps, jni_env, msg);
		break;
	case USR_RQ_PAUSE_THREADS:
		handle_usr_rq_pause_threads(ps, jni_env, msg);
		break;
	case USR_RQ_LIST_INSTRUMENTED:
		handle_usr_rq_list_instrumented(ps, msg);
		break;
	case USR_RQ_GET_ASYNC_ERRORS:
		handle_usr_rq_get_async_errors(ps, msg);
		break;
	case USR_RQ_DEINSTRUMENT_BY_ID:
		handle_usr_rq_deinstrument_by_id(ps, jni_env, msg);
		break;
	case USR_RQ_INSTRUMENT_LINE:
		handle_usr_rq_instrument_line(ps, jni_env, msg);
		break;
	case PS_SHUTDOWN:
		free(msg);
		ret = 0;
		break;
	case PS_SHUTDOWN_REQUEST:
		handle_shutdown_request(ps, jni_env, msg);
		ret = 0;
		break;
	default:
		free(msg);
		break;
	}

	return ret;
}

static void JNICALL event_loop(jvmtiEnv *jvmti_env, JNIEnv *jni_env, void *arg)
{
	struct prof_server *ps = arg;
	void *msg;

	if (jni_profiler_init_refs(jni_env) != 0) {
		LOG_ERROR("jni_profiler_init_refs failed");
		sem_post(&ps->shutdown_sem);
		return;
	}

	if (bc_instrument_init_refs(jni_env) != 0) {
		LOG_ERROR("bc_instrument_init_refs failed");
		sem_post(&ps->shutdown_sem);
		return;
	}
	LOG_INFO("startup: agent_thread=%p", (void *)ps->agent_thread);

	for (;;) {
		evq_wait(ps->ev_q, &msg);
		if (!dispatch(ps, jni_env, msg)) {
			break;
		}
	}

	ih_ctx_release_jvm_resources(&ps->instr_ctx, jni_env);

	if (ps->agent_thread != NULL) {
		(*jni_env)->DeleteGlobalRef(jni_env, ps->agent_thread);
		ps->agent_thread = NULL;
	}

	sem_post(&ps->shutdown_sem);
}

struct prof_server *ps_init(void)
{
	struct prof_server *ps;

	ps = malloc(sizeof(*ps));
	if (ps == NULL) {
		return NULL;
	}

	ps->jvmti = NULL;
	ps->thread_running = 0;
	ps->agent_thread = NULL;
	ps->paused = prof_pause_on_start();
	prof_err_log_init(&ps->err_log);

	if (ih_ctx_init(&ps->instr_ctx) != 0) {
		goto fail;
	}

	if (sem_init(&ps->shutdown_sem, 0, 0) != 0) {
		goto fail_instr_ctx;
	}

	if (pthread_mutex_init(&ps->resume_mutex, NULL) != 0) {
		goto fail_sem;
	}

	if (pthread_cond_init(&ps->resume_cond, NULL) != 0) {
		goto fail_mutex;
	}

	ps->ev_q = evq_init();
	if (ps->ev_q == NULL) {
		goto fail_cond;
	}

	ps->uif = uif_init(prof_socket_path());
	if (ps->uif == NULL) {
		LOG_ERROR("user socket creation failed");
		goto fail_queue;
	}

	ps->thread_pause = ps_thread_pause_alloc();
	if (ps->thread_pause == NULL) {
		goto fail_uif;
	}

	uif_register_handler(ps->uif, ps_uif_handler, ps);

	return ps;

fail_uif:
	uif_destroy(ps->uif);
fail_queue:
	evq_destroy(ps->ev_q);
fail_cond:
	pthread_cond_destroy(&ps->resume_cond);
fail_mutex:
	pthread_mutex_destroy(&ps->resume_mutex);
fail_sem:
	sem_destroy(&ps->shutdown_sem);
fail_instr_ctx:
	ih_ctx_destroy(&ps->instr_ctx);
fail:
	free(ps);
	return NULL;
}

struct prof_server *ps_start(
	struct prof_server *ps, jvmtiEnv *jvmti, JNIEnv *jni_env
) {
	jclass thread_class;
	jmethodID thread_ctor;
	jthread thread;
	jvmtiError err;

	ps->jvmti = jvmti;

	thread_class = (*jni_env)->FindClass(jni_env, "java/lang/Thread");
	if (thread_class == NULL) {
		return NULL;
	}

	thread_ctor = (*jni_env)->GetMethodID(
		jni_env, thread_class, "<init>", "()V"
	);
	if (thread_ctor == NULL) {
		return NULL;
	}

	thread = (*jni_env)->NewObject(jni_env, thread_class, thread_ctor);
	if (thread == NULL) {
		return NULL;
	}

	err = (*jvmti)->RunAgentThread(
		jvmti, thread, event_loop, ps, JVMTI_THREAD_NORM_PRIORITY
	);
	if (err != JVMTI_ERROR_NONE) {
		return NULL;
	}

	ps->agent_thread = (*jni_env)->NewGlobalRef(jni_env, thread);
	ps->thread_running = 1;
	return ps;
}


void ps_wait_for_resume(struct prof_server *ps)
{
	pthread_mutex_lock(&ps->resume_mutex);
	while (ps->paused) {
		pthread_cond_wait(&ps->resume_cond, &ps->resume_mutex);
	}
	pthread_mutex_unlock(&ps->resume_mutex);
}

void ps_destroy(struct prof_server *ps)
{
	if (ps->thread_running) {
		struct ps_msg *sd = malloc(sizeof(*sd));
		if (sd != NULL) {
			sd->type = PS_SHUTDOWN;
			if (evq_push(ps->ev_q, sd, sizeof(*sd)) != 0) {
				free(sd);
			}
		}
		sem_wait(&ps->shutdown_sem);
	}

	uif_destroy(ps->uif);
	ps_thread_pause_free(ps->thread_pause);

	ih_ctx_destroy(&ps->instr_ctx);
	prof_err_log_destroy(&ps->err_log);

	pthread_cond_destroy(&ps->resume_cond);
	pthread_mutex_destroy(&ps->resume_mutex);
	sem_destroy(&ps->shutdown_sem);
	evq_destroy(ps->ev_q);
	free(ps);
}

int ps_send_ev(struct prof_server *ps, struct ps_msg *msg, size_t size)
{
	if (evq_push(ps->ev_q, msg, size) != 0) {
		return -1;
	}
	return 0;
}
