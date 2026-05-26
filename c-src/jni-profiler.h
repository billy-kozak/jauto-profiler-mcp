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
#ifndef _JNI_PROFILER_H
#define _JNI_PROFILER_H

#include <jni.h>
#include <jvmti.h>
#include <stdint.h>
#include <stddef.h>

int jni_profiler_init_refs(JNIEnv *env);
int jni_create_profiler(
	JNIEnv *env, const char *class_name, const char *method_sig
);
int jni_get_profiler_stats(JNIEnv *env, uint8_t **buf_out, size_t *len_out);
int jni_retransform_class(
	JNIEnv *env, jvmtiEnv *jvmti, const char *class_name
);

#endif /* _JNI_PROFILER_H */
