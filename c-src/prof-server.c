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
#include "class-info.h"
#include "bytecode.h"
#include "jni/jni-profiler.h"
#include "jni/jni-system.h"
#include "jni/bc-instrument.h"
#include "util/log.h"

#include <jvmti.h>
#include <jni.h>
#include <pthread.h>
#include <semaphore.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define LOG_TAG "prof-server"

#define PS_CLASSES_INITIAL_CAP 16

struct pending_instrument {
	volatile const char *class_name;
	volatile const char *method_sig;
	volatile const unsigned char *bytecode;
	volatile size_t bytecode_len;
	volatile int profiler_id;
};

struct prof_server {
	struct ev_queue *ev_q;
	struct user_if *uif;
	jvmtiEnv *jvmti;
	sem_t shutdown_sem;
	pthread_mutex_t resume_mutex;
	pthread_cond_t resume_cond;
	int paused;
	size_t num_loaded_classes;
	size_t classes_capacity;
	struct class_info **loaded_classes;
	int thread_running;
	struct pending_instrument pending;
};

static int add_class(struct prof_server *ps, struct class_info *ci)
{
	if (ps->num_loaded_classes == ps->classes_capacity) {
		size_t new_cap = ps->classes_capacity * 2;
		struct class_info **arr = realloc(
			ps->loaded_classes,
			new_cap * sizeof(*arr)
		);
		if (arr == NULL) {
			return -1;
		}
		ps->loaded_classes = arr;
		ps->classes_capacity = new_cap;
	}

	ps->loaded_classes[ps->num_loaded_classes] = ci;
	ps->num_loaded_classes += 1;

	return 0;
}

static void handle_class_loaded(
	struct prof_server *ps,
	struct ps_msg *msg
) {
	char *name = msg->body.class_loaded.name;
	unsigned char *bytecode = msg->body.class_loaded.bytecode;
	size_t bytecode_len = msg->body.class_loaded.bytecode_len;
	struct method_list methods;
	struct class_info *ci;

	if (method_list_init(&methods, 0) != 0) {
		ps_send_class_ev_dealloc(msg);
		return;
	}

	bc_extract_methods(bytecode, bytecode_len, &methods);

	ci = ci_alloc(name, bytecode, bytecode_len, &methods);
	if (ci == NULL) {
		method_list_deep_destroy(&methods);
	} else if (add_class(ps, ci) != 0) {
		ci_free(ci);
	}

	ps_send_class_ev_dealloc(msg);
}

static void handle_usr_rq_loaded_classes(
	struct prof_server *ps,
	struct ps_msg *msg
) {
	struct user_if_client *client = msg->body.usr_rq_loaded_classes.client;
	struct user_msg *response;
	uint8_t *p;
	uint32_t body_size;
	uint32_t count;
	size_t i;

	free(msg);

	body_size = sizeof(uint32_t);
	for (i = 0; i < ps->num_loaded_classes; i++) {
		body_size +=
			sizeof(uint16_t) + strlen(ps->loaded_classes[i]->name);
	}

	response = malloc(offsetof(struct user_msg, body) + body_size);
	if (response == NULL) {
		uif_client_release(client);
		return;
	}

	response->type = RESPONSE_LOADED_CLASSES;
	response->size = body_size;

	p = (uint8_t *)&response->body;

	count = (uint32_t)ps->num_loaded_classes;
	memcpy(p, &count, sizeof(count));
	p += sizeof(count);

	for (i = 0; i < ps->num_loaded_classes; i++) {
		uint16_t name_len = (uint16_t)strlen(
			ps->loaded_classes[i]->name
		);
		memcpy(p, &name_len, sizeof(name_len));
		p += sizeof(name_len);
		memcpy(p, ps->loaded_classes[i]->name, name_len);
		p += name_len;
	}

	uif_send(ps->uif, client, response);
	free(response);
	uif_client_release(client);
}

static void handle_usr_rq_class_methods(
	struct prof_server *ps,
	struct ps_msg *msg
) {
	struct user_if_client *client = msg->body.usr_rq_class_methods.client;
	const char *class_name = msg->body.usr_rq_class_methods.class_name;
	struct class_info *ci = NULL;
	struct user_msg *response;
	uint8_t *p;
	uint32_t body_size;
	uint32_t count;
	size_t i;

	for (i = 0; i < ps->num_loaded_classes; i++) {
		if (strcmp(ps->loaded_classes[i]->name, class_name) == 0) {
			ci = ps->loaded_classes[i];
			break;
		}
	}

	body_size = sizeof(uint32_t);
	if (ci != NULL) {
		for (i = 0; i < ci->methods.len; i++) {
			body_size += pstring_total_size(ci->methods.arr[i]);
		}
	}
	count = (ci != NULL) ? (uint32_t)ci->methods.len : 0;

	response = malloc(offsetof(struct user_msg, body) + body_size);
	if (response == NULL) {
		ps_usr_rq_class_methods_dealloc(msg);
		return;
	}

	response->type = RESPONSE_CLASS_METHODS;
	response->size = body_size;

	p = response->body.raw;
	memcpy(p, &count, sizeof(count));
	p += sizeof(count);

	if (ci != NULL) {
		for (i = 0; i < ci->methods.len; i++) {
			size_t sz = pstring_total_size(ci->methods.arr[i]);
			memcpy(p, ci->methods.arr[i], sz);
			p += sz;
		}
	}

	uif_send(ps->uif, client, response);
	free(response);
	ps_usr_rq_class_methods_dealloc(msg);
}

static unsigned char *build_instrumented_bytecode(
	JNIEnv *jni_env,
	const struct class_info *ci,
	size_t *new_bc_len_out
) {
	int count = (int)ci->instrumented.len;
	const char **method_names = NULL;
	const char **method_descs = NULL;
	int *profiler_ids = NULL;
	char *name_buf = NULL;
	unsigned char *result = NULL;
	size_t total_name_buf = 0;
	size_t i;
	char *p;

	method_names  = malloc((size_t)count * sizeof(*method_names));
	method_descs  = malloc((size_t)count * sizeof(*method_descs));
	profiler_ids  = malloc((size_t)count * sizeof(*profiler_ids));

	bool input_fail = (
		method_names == NULL ||
		method_descs == NULL ||
		profiler_ids == NULL
	);
	if (input_fail) {
		goto cleanup;
	}

	for (i = 0; i < (size_t)count; i++) {
		const char *colon = strchr(
			ci->instrumented.arr[i].method_sig, ':'
		);
		if (colon == NULL) {
			LOG_ERROR("malformed method_sig in instrumented list");
			goto cleanup;
		}
		total_name_buf += (size_t)(
			colon - ci->instrumented.arr[i].method_sig
		) + 1;
	}

	name_buf = malloc(total_name_buf);
	if (name_buf == NULL) {
		goto cleanup;
	}

	p = name_buf;
	for (i = 0; i < (size_t)count; i++) {
		const char *sig   = ci->instrumented.arr[i].method_sig;
		const char *colon = strchr(sig, ':');
		size_t name_len   = (size_t)(colon - sig);
		memcpy(p, sig, name_len);
		p[name_len]     = '\0';
		method_names[i] = p;
		method_descs[i] = colon + 1;
		profiler_ids[i] = ci->instrumented.arr[i].profiler_id;
		p += name_len + 1;
	}

	result = bc_instrument_method(
		jni_env,
		ci->bytecode, ci->bytecode_len,
		method_names, method_descs, profiler_ids, count,
		new_bc_len_out
	);

	if (result == NULL) {
		LOG_ERROR("bc_instrument_method failed");
	}

cleanup:
	free(name_buf);
	free(method_names);
	free(method_descs);
	free(profiler_ids);
	return result;
}

static int retransform_pending(
	struct prof_server *ps,
	JNIEnv *jni_env,
	const char *class_name,
	const char *method_sig,
	const unsigned char *new_bc,
	size_t new_bc_len,
	int profiler_id
) {
	int ret;

	ps->pending.class_name   = class_name;
	ps->pending.method_sig   = method_sig;
	ps->pending.bytecode     = new_bc;
	ps->pending.bytecode_len = new_bc_len;
	ps->pending.profiler_id  = profiler_id;

	ret = jni_retransform_class(jni_env, ps->jvmti, class_name);
	if (ret != 0) {
		LOG_ERROR("RetransformClasses failed");
	}

	ps->pending.class_name   = NULL;
	ps->pending.method_sig   = NULL;
	ps->pending.bytecode     = NULL;
	ps->pending.bytecode_len = 0;
	ps->pending.profiler_id  = -1;

	return ret;
}

static void handle_usr_rq_instrument_method(
	struct prof_server *ps,
	JNIEnv *jni_env,
	struct ps_msg *msg
) {
	struct user_if_client *client = msg->body
		.usr_rq_instrument_method.client;

	const char *class_name = msg->body.usr_rq_instrument_method.class_name;
	const char *method_sig = msg->body.usr_rq_instrument_method.method_sig;
	struct user_msg_instr_resp resp_body = {INSTRUMENT_RP_ERROR};
	struct user_msg *response;
	struct class_info *ci = NULL;
	struct instrumented_method *im = NULL;
	unsigned char *new_bc = NULL;
	size_t new_bc_len;
	int id;
	size_t i;

	id = jni_create_profiler(jni_env, class_name, method_sig);
	if (id == -1) {
		resp_body.status = INSTRUMENT_RP_DOUBLE_INSTRUMENT;
		goto respond;
	}
	if (id < 0) {
		LOG_ERROR("jni_create_profiler failed");
		goto respond;
	}

	for (i = 0; i < ps->num_loaded_classes; i++) {
		if (strcmp(ps->loaded_classes[i]->name, class_name) == 0) {
			ci = ps->loaded_classes[i];
			break;
		}
	}
	if (ci == NULL) {
		LOG_WARN("instrument: class not found: %s", class_name);
		goto fail_cleanup_profiler;
	}

	im = instrumented_method_list_add_and_init(
		&ci->instrumented,
		method_sig,
		id
	);
	if (im == NULL || im->method_sig == NULL) {
		if (im != NULL) {
			ci->instrumented.len--;
		}
		goto fail_cleanup_profiler;
	}

	new_bc = build_instrumented_bytecode(jni_env, ci, &new_bc_len);
	if (new_bc == NULL) {
		goto fail_cleanup_instrumented;
	}

	if (retransform_pending(
		ps, jni_env, class_name, method_sig, new_bc, new_bc_len, id
	) != 0) {
		goto fail_cleanup_instrumented;
	}

	resp_body.status = INSTRUMENT_RP_OK;
	goto respond;

fail_cleanup_instrumented:
	instrumented_method_list_remove_and_destroy(
		&ci->instrumented, (int)(ci->instrumented.len - 1)
	);
fail_cleanup_profiler:
	jni_remove_profiler(jni_env, id);
respond:
	free(new_bc);
	response = malloc(offsetof(struct user_msg, body) + sizeof(resp_body));
	if (response == NULL) {
		ps_usr_rq_instrument_method_dealloc(msg);
		return;
	}

	response->type = RESPONSE_INSTRUMENT_METHOD;
	response->size = sizeof(resp_body);
	response->body.instr_rep = resp_body;

	uif_send(ps->uif, client, response);
	free(response);
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

	if (ps->pending.class_name == NULL) {
		return;
	}
	if (strcmp(name, (const char *)ps->pending.class_name) != 0) {
		return;
	}

	err = (*jvmti)->Allocate(jvmti, ps->pending.bytecode_len, &buf);
	if (err != JVMTI_ERROR_NONE) {
		LOG_ERROR("Allocate failed in retransform");
		return;
	}

	LOG_DEBUG(
		"retransform hook fired for %s, bytecode_len=%zu",
		name, ps->pending.bytecode_len
	);

	memcpy(
		buf,
		(const void *)ps->pending.bytecode,
		ps->pending.bytecode_len
	);
	*new_class_data = buf;
	*new_class_data_len = (jint)ps->pending.bytecode_len;
}

static void handle_usr_rq_get_stats(
	struct prof_server *ps,
	JNIEnv *jni_env,
	struct ps_msg *msg
) {
	struct user_if_client *client = msg->body.usr_rq_get_stats.client;
	struct user_msg *response;
	uint8_t *stats_buf = NULL;
	size_t stats_len = 0;

	free(msg);

	if (jni_get_profiler_stats(jni_env, &stats_buf, &stats_len) != 0) {
		LOG_ERROR("jni_get_profiler_stats failed");
		uif_client_release(client);
		return;
	}

	response = malloc(offsetof(struct user_msg, body) + stats_len);
	if (response == NULL) {
		free(stats_buf);
		uif_client_release(client);
		return;
	}

	response->type = RESPONSE_GET_STATS;
	response->size = (uint32_t)stats_len;
	memcpy(response->body.raw, stats_buf, stats_len);
	free(stats_buf);

	uif_send(ps->uif, client, response);
	free(response);
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

	struct user_msg *response;
	struct class_info *ci = NULL;
	unsigned char *new_bc = NULL;
	size_t new_bc_len;
	struct user_msg_deinstr_resp resp_body = {DEINSTRUMENT_RP_OK};
	int profiler_id = -1;
	size_t i;

	for (i = 0; i < ps->num_loaded_classes; i++) {
		if (strcmp(ps->loaded_classes[i]->name, class_name) == 0) {
			ci = ps->loaded_classes[i];
			break;
		}
	}
	if (ci == NULL) {
		LOG_WARN("deinstrument: class not found: %s", class_name);
		resp_body.status = DEINSTRUMENT_RP_FAIL;
		goto respond;
	}
	struct instrumented_method_list *instrumented = &ci->instrumented;

	for (i = 0; i < instrumented->len; i++) {
		if (strcmp(instrumented->arr[i].method_sig, method_sig) == 0) {
			profiler_id = instrumented->arr[i].profiler_id;
			instrumented_method_list_remove_and_destroy(
				&ci->instrumented, (int)i
			);
			break;
		}
	}
	if (profiler_id == -1) {
		LOG_WARN(
			"deinstrument: %s not instrumented in %s",
			method_sig, class_name
		);
		resp_body.status = DEINSTRUMENT_RP_FAIL;
		goto respond;
	}

	if (instrumented->len > 0) {
		new_bc = build_instrumented_bytecode(jni_env, ci, &new_bc_len);
		if (new_bc == NULL) {
			resp_body.status = DEINSTRUMENT_RP_FAIL;
			goto cleanup_profiler;
		}
		if (retransform_pending(
			ps, jni_env, class_name, NULL, new_bc, new_bc_len, -1
		) != 0) {
			LOG_ERROR("deinstrument retransform failed");
			resp_body.status = DEINSTRUMENT_RP_FAIL;
		}
	} else {
		if (retransform_pending(
			ps, jni_env, class_name, NULL,
			ci->bytecode, ci->bytecode_len, -1
		) != 0) {
			LOG_ERROR("deinstrument retransform failed");
			resp_body.status = DEINSTRUMENT_RP_FAIL;
		}
	}

cleanup_profiler:
	if (jni_remove_profiler(jni_env, profiler_id) != 0) {
		LOG_ERROR("jni_remove_profiler failed");
	}

respond:
	free(new_bc);
	response = malloc(offsetof(struct user_msg, body) + sizeof(resp_body));
	if (response == NULL) {
		ps_usr_rq_deinstrument_method_dealloc(msg);
		return;
	}

	response->type = RESPONSE_DEINSTRUMENT_METHOD;
	response->size = sizeof(resp_body);
	memcpy(&response->body.deinstr_resp, &resp_body, response->size);

	uif_send(ps->uif, client, response);
	free(response);
	ps_usr_rq_deinstrument_method_dealloc(msg);
}

static void handle_shutdown_request(JNIEnv *jni_env, struct ps_msg *msg)
{
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

	jni_system_exit(jni_env, exit_code);
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

static void handle_usr_rq_resume(
	struct prof_server *ps,
	struct ps_msg *msg
) {
	struct user_if_client *client = msg->body.usr_rq_resume.client;
	struct user_msg_resume_resp resp_body;
	struct user_msg *response;

	free(msg);

	if (resume_vm(ps)) {
		resp_body.status = RESUME_RP_UNBLOCKED;
	} else {
		resp_body.status = RESUME_RP_NOCHANGE;
	}

	response = malloc(offsetof(struct user_msg, body) + sizeof(resp_body));
	if (response == NULL) {
		uif_client_release(client);
		return;
	}

	response->type = RESPONSE_RESUME;
	response->size = sizeof(resp_body);
	response->body.resume_resp = resp_body;

	uif_send(ps->uif, client, response);
	free(response);
	uif_client_release(client);
}

static int dispatch(struct prof_server *ps, JNIEnv *jni_env, void *raw)
{
	struct ps_msg *msg = (struct ps_msg *)raw;
	int ret = 1;

	switch (msg->type) {
	case CLASS_LOADED:
		handle_class_loaded(ps, msg);
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
		handle_usr_rq_resume(ps, msg);
		break;
	case PS_SHUTDOWN:
		free(msg);
		ret = 0;
		break;
	case PS_SHUTDOWN_REQUEST:
		handle_shutdown_request(jni_env, msg);
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

	(void)jvmti_env;

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

	for (;;) {
		evq_wait(ps->ev_q, &msg);
		if (!dispatch(ps, jni_env, msg)) {
			break;
		}
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
	ps->pending = (struct pending_instrument){0};
	ps->num_loaded_classes = 0;
	ps->classes_capacity = PS_CLASSES_INITIAL_CAP;
	ps->paused = prof_pause_on_start();
	ps->loaded_classes = malloc(
		PS_CLASSES_INITIAL_CAP * sizeof(*ps->loaded_classes)
	);
	if (ps->loaded_classes == NULL) {
		goto fail;
	}

	if (sem_init(&ps->shutdown_sem, 0, 0) != 0) {
		goto fail_classes;
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

	uif_register_handler(ps->uif, ps_uif_handler, ps);

	return ps;

fail_queue:
	evq_destroy(ps->ev_q);
fail_cond:
	pthread_cond_destroy(&ps->resume_cond);
fail_mutex:
	pthread_mutex_destroy(&ps->resume_mutex);
fail_sem:
	sem_destroy(&ps->shutdown_sem);
fail_classes:
	free(ps->loaded_classes);
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
	size_t i;

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

	for (i = 0; i < ps->num_loaded_classes; i++) {
		ci_free(ps->loaded_classes[i]);
	}
	free(ps->loaded_classes);

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
