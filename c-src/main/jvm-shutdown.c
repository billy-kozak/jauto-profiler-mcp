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

#include "jvm-shutdown.h"

#include "jni/jni-shutdown.h"
#include "jni/jni-profiler.h"
#include "prof-server-msg.h"
#include "util/ev-queue.h"
#include "util/log.h"

#include <jni.h>
#include <semaphore.h>
#include <stdlib.h>

#define LOG_TAG "jvm-shutdown"

static void log_shutdown_callback(void *data)
{
	(void)data;
	LOG_INFO("JVM shutdown hook called");
}

static void shutdown_wait_callback(void *data)
{
	struct jvm_shutdown_blocker *blocker = (struct jvm_shutdown_blocker *)data;
	struct ps_msg *msg;

	msg = malloc(sizeof(*msg));
	if (msg != NULL) {
		msg->type = PS_SHUTDOWN_STARTING;
		if (evq_push(blocker->ev_q, msg, sizeof(*msg)) != 0) {
			free(msg);
			sem_post(&blocker->started_sem);
		}
	} else {
		sem_post(&blocker->started_sem);
	}

	sem_wait(&blocker->started_sem);
	sem_wait(&blocker->sem);
}

int jvm_register_shutdown_hook(JNIEnv *env)
{
	if (jni_register_shutdown_hook(env) != 0) {
		return -1;
	}
	return jni_add_shutdown_callback(env, log_shutdown_callback, NULL);
}

void jvm_prepare_shutdown(struct jvm_shutdown_blocker *blocker, JNIEnv *env)
{
	jni_stop_profiler_collector(env);
	sem_post(&blocker->started_sem);
}

void jvm_unblock_shutdown(struct jvm_shutdown_blocker *blocker)
{
	sem_post(&blocker->sem);
}

int jvm_register_shutdown_blocker(
	struct jvm_shutdown_blocker *blocker,
	JNIEnv *env,
	struct ev_queue *ev_q
) {
	if (sem_init(&blocker->started_sem, 0, 0) != 0) {
		LOG_ERROR("sem_init for shutdown started_sem failed");
		return -1;
	}
	if (sem_init(&blocker->sem, 0, 0) != 0) {
		LOG_ERROR("sem_init for shutdown sem failed");
		sem_destroy(&blocker->started_sem);
		return -1;
	}
	blocker->ev_q = ev_q;
	return jni_add_shutdown_callback(env, shutdown_wait_callback, blocker);
}
