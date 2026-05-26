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

#include "jni-profiler.h"

#include <jni.h>
#include <jvmti.h>

#define REGISTRY_CLASS "app/autoprofiler/ProfilerRegistry"
#define CREATE_SIG     "(Ljava/lang/String;Ljava/lang/String;)I"

static jclass registry_class = NULL;
static jmethodID create_method = NULL;

int jni_profiler_init_refs(JNIEnv *env)
{
	jclass local;

	if (registry_class != NULL) {
		return 0;
	}

	local = (*env)->FindClass(env, REGISTRY_CLASS);
	if (local == NULL) {
		return -1;
	}

	registry_class = (*env)->NewGlobalRef(env, local);
	(*env)->DeleteLocalRef(env, local);
	if (registry_class == NULL) {
		return -1;
	}

	create_method = (*env)->GetStaticMethodID(
		env, registry_class, "create", CREATE_SIG
	);
	if (create_method == NULL) {
		(*env)->DeleteGlobalRef(env, registry_class);
		registry_class = NULL;
		return -1;
	}

	return 0;
}

int jni_create_profiler(
	JNIEnv *env, const char *class_name, const char *method_sig
) {
	jstring j_class_name;
	jstring j_method_sig;
	jint result;

	if (registry_class == NULL) {
		return -1;
	}

	j_class_name = (*env)->NewStringUTF(env, class_name);
	if (j_class_name == NULL) {
		return -1;
	}

	j_method_sig = (*env)->NewStringUTF(env, method_sig);
	if (j_method_sig == NULL) {
		(*env)->DeleteLocalRef(env, j_class_name);
		return -1;
	}

	result = (*env)->CallStaticIntMethod(
		env, registry_class, create_method, j_class_name, j_method_sig
	);

	(*env)->DeleteLocalRef(env, j_method_sig);
	(*env)->DeleteLocalRef(env, j_class_name);

	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionClear(env);
		return -1;
	}

	return (int)result;
}

int jni_retransform_class(
	JNIEnv *env, jvmtiEnv *jvmti, const char *class_name
) {
	jclass cls;
	jvmtiError err;

	cls = (*env)->FindClass(env, class_name);
	if (cls == NULL) {
		if ((*env)->ExceptionCheck(env)) {
			(*env)->ExceptionClear(env);
		}
		return -1;
	}

	err = (*jvmti)->RetransformClasses(jvmti, 1, &cls);
	(*env)->DeleteLocalRef(env, cls);

	if (err != JVMTI_ERROR_NONE) {
		printf("jauto-profiler: RetransformClasses error code: %d\n", (int)err);
		fflush(stdout);
	}

	return (err == JVMTI_ERROR_NONE) ? 0 : -1;
}
