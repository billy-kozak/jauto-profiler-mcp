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

#ifndef _PROF_SERVER_H
#define _PROF_SERVER_H

#include <jvmti.h>
#include <jni.h>
#include <stdlib.h>

struct prof_server;
struct ps_msg;

struct prof_server *ps_init();
struct prof_server *ps_start(
	struct prof_server *ps, jvmtiEnv *jvmti, JNIEnv *jni_env
);
void ps_destroy(struct prof_server *ps);
int ps_send_ev(struct prof_server *ps, struct ps_msg *msg, size_t size);
void ps_handle_retransform(
	struct prof_server *ps,
	jvmtiEnv *jvmti,
	const char *name,
	jint *new_class_data_len,
	unsigned char **new_class_data
);

#endif /* _PROF_SERVER_H */
