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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "jni/jni-util.h"
#include "util/log.h"

#define LOG_TAG "jni-profiler"

#define REGISTRY_CLASS       "app/autoprofiler/ProfilerRegistry"
#define CREATE_SIG           "(JLjava/lang/String;Ljava/lang/String;)I"
#define CREATE_LINE_SIG      "(JLjava/lang/String;Ljava/lang/String;II)I"
#define REMOVE_SIG           "(I)V"
#define GET_STATS_SIG        "()[B"

static jclass registry_class = NULL;
static jmethodID create_method = NULL;
static jmethodID create_line_method = NULL;
static jmethodID remove_method = NULL;
static jmethodID get_stats_method = NULL;

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
		goto fail;
	}

	create_line_method = (*env)->GetStaticMethodID(
		env, registry_class, "create", CREATE_LINE_SIG
	);
	if (create_line_method == NULL) {
		goto fail;
	}

	remove_method = (*env)->GetStaticMethodID(
		env, registry_class, "remove", REMOVE_SIG
	);
	if (remove_method == NULL) {
		goto fail;
	}

	get_stats_method = (*env)->GetStaticMethodID(
		env, registry_class, "getStats", GET_STATS_SIG
	);
	if (get_stats_method == NULL) {
		goto fail;
	}

	return 0;
fail:
	(*env)->DeleteGlobalRef(env, registry_class);
	registry_class = NULL;
	return -1;
}

int jni_create_profiler(
	JNIEnv *env,
	uint64_t instrument_id,
	const char *class_name,
	const char *method_sig
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
		env, registry_class, create_method,
		(jlong)instrument_id, j_class_name, j_method_sig
	);

	(*env)->DeleteLocalRef(env, j_method_sig);
	(*env)->DeleteLocalRef(env, j_class_name);

	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionClear(env);
		return -2;
	}

	return (int)result;
}

int jni_create_line_profiler(
	JNIEnv *env,
	uint64_t instrument_id,
	const char *entry_class,
	const char *exit_class,
	int entry_line,
	int exit_line
) {
	jstring j_entry_class;
	jstring j_exit_class;
	jint result;

	if (registry_class == NULL) {
		return -1;
	}

	j_entry_class = (*env)->NewStringUTF(env, entry_class);
	if (j_entry_class == NULL) {
		return -1;
	}

	j_exit_class = (*env)->NewStringUTF(env, exit_class);
	if (j_exit_class == NULL) {
		(*env)->DeleteLocalRef(env, j_entry_class);
		return -1;
	}

	result = (*env)->CallStaticIntMethod(
		env, registry_class, create_line_method,
		(jlong)instrument_id, j_entry_class, j_exit_class,
		(jint)entry_line, (jint)exit_line
	);

	(*env)->DeleteLocalRef(env, j_exit_class);
	(*env)->DeleteLocalRef(env, j_entry_class);

	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionClear(env);
		return -2;
	}

	return (int)result;
}

int jni_remove_profiler(JNIEnv *env, int profiler_id)
{
	if (registry_class == NULL) {
		return -1;
	}

	(*env)->CallStaticVoidMethod(
		env, registry_class, remove_method, (jint)profiler_id
	);

	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionClear(env);
		return -1;
	}

	return 0;
}

int jni_get_profiler_stats(JNIEnv *env, uint8_t **buf_out, size_t *len_out)
{
	jbyteArray result;
	jsize len;
	uint8_t *buf;

	if (registry_class == NULL) {
		return -1;
	}

	result = (*env)->CallStaticObjectMethod(
		env, registry_class, get_stats_method
	);

	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
		return -1;
	}

	if (result == NULL) {
		return -1;
	}

	len = (*env)->GetArrayLength(env, result);
	buf = malloc((size_t)len);
	if (buf == NULL) {
		(*env)->DeleteLocalRef(env, result);
		return -1;
	}

	(*env)->GetByteArrayRegion(env, result, 0, len, (jbyte *)buf);
	(*env)->DeleteLocalRef(env, result);

	*buf_out = buf;
	*len_out = (size_t)len;
	return 0;
}

static int check_loader_match(
	JNIEnv *env, jvmtiEnv *jvmti, jclass cls, jobject loader
) {
	jobject class_loader = NULL;
	int match = 0;
	jvmtiError err;

	err = (*jvmti)->GetClassLoader(jvmti, cls, &class_loader);
	if (err != JVMTI_ERROR_NONE) {
		return 0;
	}

	if (loader == NULL && class_loader == NULL) {
		match = 1;
	} else if (loader != NULL && class_loader != NULL) {
		match = (*env)->IsSameObject(env, loader, class_loader);
	}

	if (class_loader != NULL) {
		(*env)->DeleteLocalRef(env, class_loader);
	}
	return match;
}

int jni_retransform_class(
	JNIEnv *env, jvmtiEnv *jvmti, const char *class_name, jobject loader
) {
	jvmtiError err;
	jint class_count;
	jclass *classes;
	jclass best = NULL;
	int found_loader_match = 0;
	size_t name_len;
	jint i;

	err = (*jvmti)->GetLoadedClasses(jvmti, &class_count, &classes);
	if (err != JVMTI_ERROR_NONE) {
		return -1;
	}

	name_len = strlen(class_name);

	for (i = 0; i < class_count; i++) {
		char *sig = NULL;
		int name_match = 0;

		err = (*jvmti)->GetClassSignature(
			jvmti, classes[i], &sig, NULL
		);
		if (err == JVMTI_ERROR_NONE && sig != NULL) {
			name_match = (
				sig[0] == 'L' &&
				strncmp(sig + 1, class_name, name_len) == 0 &&
				sig[name_len + 1] == ';'
			);
			(*jvmti)->Deallocate(jvmti, (unsigned char *)sig);
		}

		if (name_match) {
			int is_loader_match = check_loader_match(
				env, jvmti, classes[i], loader
			);
			if (is_loader_match || best == NULL) {
				jni_safe_free_global_class(env, best);
				best = (*env)->NewGlobalRef(env, classes[i]);
			}
			if (is_loader_match) {
				found_loader_match = 1;
			}
		}

		(*env)->DeleteLocalRef(env, classes[i]);

		if(found_loader_match) {
			break;
		}
	}

	(*jvmti)->Deallocate(jvmti, (unsigned char *)classes);

	if (best == NULL) {
		return -1;
	}

	if (!found_loader_match) {
		LOG_WARN(
			"retransform %s: no loader match, using first name match",
			class_name
		);
	}

	err = (*jvmti)->RetransformClasses(jvmti, 1, &best);
	(*env)->DeleteGlobalRef(env, best);

	if (err != JVMTI_ERROR_NONE) {
		LOG_ERROR("RetransformClasses error code: %d", (int)err);
	}

	return (err == JVMTI_ERROR_NONE) ? 0 : -1;
}
