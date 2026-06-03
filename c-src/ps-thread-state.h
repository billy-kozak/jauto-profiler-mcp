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
#ifndef _PS_THREAD_STATE_H
#define _PS_THREAD_STATE_H

#include <jvmti.h>

struct user_if_client;

/*
 * Tracks the threads currently suspended by a pause-threads request.
 * suspended_threads holds a global JNI ref for each suspended application
 * thread; suspended_count is the length of that array.
 * Both are NULL/0 when no pause is active.
 */
struct ps_thread_pause {
	jthread *suspended_threads;
	jint suspended_count;
};

enum ps_suspend_result {
	PS_SUSPEND_OK    =  0,
	PS_SUSPEND_RACE  =  1,
	PS_SUSPEND_ERROR = -1,
};

struct ps_thread_pause *ps_thread_pause_alloc(void);
void ps_thread_pause_free(struct ps_thread_pause *state);

enum ps_suspend_result ps_thread_pause_suspend(
	struct ps_thread_pause *state,
	jvmtiEnv *jvmti,
	JNIEnv *jni_env,
	jthread agent_thread
);

int ps_thread_pause_resume(
	struct ps_thread_pause *state,
	jvmtiEnv *jvmti,
	JNIEnv *jni_env
);

#endif /* _PS_THREAD_STATE_H */
