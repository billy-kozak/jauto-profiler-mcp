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
#ifndef _JVM_SHUTDOWN_H
#define _JVM_SHUTDOWN_H

#include <jni.h>
#include <semaphore.h>

struct ev_queue;

struct jvm_shutdown_blocker {
	sem_t started_sem;
	sem_t sem;
	struct ev_queue *ev_q;
};

int jvm_register_shutdown_hook(JNIEnv *env);
int jvm_register_shutdown_blocker(
	struct jvm_shutdown_blocker *blocker,
	JNIEnv *env,
	struct ev_queue *ev_q
);
void jvm_prepare_shutdown(struct jvm_shutdown_blocker *blocker, JNIEnv *env);
void jvm_unblock_shutdown(struct jvm_shutdown_blocker *blocker);

#endif /* _JVM_SHUTDOWN_H */
