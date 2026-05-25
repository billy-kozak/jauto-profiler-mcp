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

#include "ev-queue.h"
#include "prof-server-msg.h"
#include "prof-env.h"
#include "user-if.h"
#include "ps-uif-handler.h"
#include "prof-server-ev.h"
#include "class-info.h"
#include "bytecode.h"

#include <jvmti.h>
#include <jni.h>
#include <semaphore.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define PS_CLASSES_INITIAL_CAP 16

struct prof_server {
	struct ev_queue *ev_q;
	struct user_if *uif;
	jvmtiEnv *jvmti;
	sem_t shutdown_sem;
	size_t num_loaded_classes;
	size_t classes_capacity;
	struct class_info **loaded_classes;
	int thread_running;
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

	ci = ci_alloc(name, &methods);
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

static int dispatch(struct prof_server *ps, void *raw)
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
	case PS_SHUTDOWN:
		free(msg);
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
	(void)jni_env;

	for (;;) {
		evq_wait(ps->ev_q, &msg);
		if (!dispatch(ps, msg)) {
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
	ps->num_loaded_classes = 0;
	ps->classes_capacity = PS_CLASSES_INITIAL_CAP;
	ps->loaded_classes = malloc(
		PS_CLASSES_INITIAL_CAP * sizeof(*ps->loaded_classes)
	);
	if (ps->loaded_classes == NULL) {
		goto fail;
	}

	if (sem_init(&ps->shutdown_sem, 0, 0) != 0) {
		goto fail_classes;
	}

	ps->ev_q = evq_init();
	if (ps->ev_q == NULL) {
		goto fail_sem;
	}

	ps->uif = uif_init(prof_socket_path());
	if (ps->uif == NULL) {
		fprintf(stderr, "User socket creation failed\n");
		goto fail_queue;
	}

	uif_register_handler(ps->uif, ps_uif_handler, ps);

	return ps;

fail_queue:
	evq_destroy(ps->ev_q);
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

void ps_destroy(struct prof_server *ps)
{
	size_t i;

	if (ps->thread_running) {
		struct ps_msg *shutdown = malloc(sizeof(*shutdown));
		if (shutdown != NULL) {
			shutdown->type = PS_SHUTDOWN;
			if (evq_push(ps->ev_q, shutdown, sizeof(*shutdown)) != 0) {
				free(shutdown);
			}
		}
		sem_wait(&ps->shutdown_sem);
	}

	uif_destroy(ps->uif);

	for (i = 0; i < ps->num_loaded_classes; i++) {
		ci_free(ps->loaded_classes[i]);
	}
	free(ps->loaded_classes);

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
