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
#ifndef _JNI_UTIL_H
#define _JNI_UTIL_H

#include <jni.h>

static inline void jni_safe_free_global_obj(
	JNIEnv * jni_env, jobject obj
) {
	if(obj != NULL) {
		(*jni_env)->DeleteGlobalRef(jni_env, obj);
	}
}

static inline void jni_safe_free_global_class(
	JNIEnv * jni_env, jclass obj
) {
	if(obj != NULL) {
		(*jni_env)->DeleteGlobalRef(jni_env, obj);
	}
}

#endif /* _JNI_UTIL_H */
