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

#include "bc-instrument.h"

#include <stdlib.h>
#include <string.h>

#include "util/log.h"

#define LOG_TAG "bc-instrument"

#define TRANSFORMER_CLASS "app/autoprofiler/transform/BytecodeTransformer"
#define TRANSFORM_SIG \
	"([B[Ljava/lang/String;[I[I[I[I)[B"

static jclass transformer_class = NULL;
static jmethodID transform_method = NULL;

int bc_instrument_init_refs(JNIEnv *env)
{
	jclass local;

	if (transformer_class != NULL) {
		return 0;
	}

	local = (*env)->FindClass(env, TRANSFORMER_CLASS);
	if (local == NULL) {
		LOG_ERROR("FindClass BytecodeTransformer failed");
		if ((*env)->ExceptionCheck(env)) {
			(*env)->ExceptionClear(env);
		}
		return -1;
	}

	transformer_class = (*env)->NewGlobalRef(env, local);
	(*env)->DeleteLocalRef(env, local);
	if (transformer_class == NULL) {
		return -1;
	}

	transform_method = (*env)->GetStaticMethodID(
		env, transformer_class, "transform", TRANSFORM_SIG
	);
	if (transform_method == NULL) {
		LOG_ERROR("GetStaticMethodID transform failed");
		if ((*env)->ExceptionCheck(env)) {
			(*env)->ExceptionClear(env);
		}
		(*env)->DeleteGlobalRef(env, transformer_class);
		transformer_class = NULL;
		return -1;
	}

	return 0;
}

unsigned char *bc_instrument_method(
	JNIEnv *env,
	const unsigned char *class_data,
	size_t class_data_len,
	const char **method_sigs,
	const int *profiler_ids,
	int count,
	const int *line_numbers,
	const int *line_types,
	const int *line_profiler_ids,
	int line_count,
	size_t *new_len_out
) {
	jclass string_class = NULL;
	jbyteArray input_arr = NULL;

	if (count == 0 && line_count == 0) {
		unsigned char *copy = malloc(class_data_len);
		if (copy == NULL) {
			return NULL;
		}
		memcpy(copy, class_data, class_data_len);
		*new_len_out = class_data_len;
		return copy;
	}
	jobjectArray j_sigs = NULL;
	jintArray j_ids = NULL;
	jintArray j_line_nums = NULL;
	jintArray j_line_types = NULL;
	jintArray j_line_ids = NULL;
	jbyteArray result_arr = NULL;
	jint *ids_buf = NULL;
	jsize result_len;
	unsigned char *out = NULL;
	int i;

	if (bc_instrument_init_refs(env) != 0) {
		return NULL;
	}

	string_class = (*env)->FindClass(env, "java/lang/String");
	if (string_class == NULL) {
		return NULL;
	}

	input_arr = (*env)->NewByteArray(env, (jsize)class_data_len);
	if (input_arr == NULL) {
		goto cleanup;
	}
	(*env)->SetByteArrayRegion(
		env, input_arr, 0, (jsize)class_data_len, (const jbyte *)class_data
	);

	j_sigs = (*env)->NewObjectArray(env, (jsize)count, string_class, NULL);
	if (j_sigs == NULL) {
		goto cleanup;
	}
	for (i = 0; i < count; i++) {
		jstring s = (*env)->NewStringUTF(env, method_sigs[i]);
		if (s == NULL) {
			goto cleanup;
		}
		(*env)->SetObjectArrayElement(env, j_sigs, (jsize)i, s);
		(*env)->DeleteLocalRef(env, s);
	}

	j_ids = (*env)->NewIntArray(env, (jsize)count);
	if (j_ids == NULL) {
		goto cleanup;
	}
	ids_buf = malloc((size_t)count * sizeof(jint));
	if (ids_buf == NULL) {
		goto cleanup;
	}
	for (i = 0; i < count; i++) {
		ids_buf[i] = (jint)profiler_ids[i];
	}
	(*env)->SetIntArrayRegion(env, j_ids, 0, (jsize)count, ids_buf);
	free(ids_buf);
	ids_buf = NULL;

	j_line_nums = (*env)->NewIntArray(env, (jsize)line_count);
	if (j_line_nums == NULL) {
		goto cleanup;
	}
	j_line_types = (*env)->NewIntArray(env, (jsize)line_count);
	if (j_line_types == NULL) {
		goto cleanup;
	}
	j_line_ids = (*env)->NewIntArray(env, (jsize)line_count);
	if (j_line_ids == NULL) {
		goto cleanup;
	}
	if (line_count > 0) {
		ids_buf = malloc((size_t)line_count * sizeof(jint));
		if (ids_buf == NULL) {
			goto cleanup;
		}
		for (i = 0; i < line_count; i++) {
			ids_buf[i] = (jint)line_numbers[i];
		}
		(*env)->SetIntArrayRegion(env, j_line_nums, 0, (jsize)line_count, ids_buf);
		for (i = 0; i < line_count; i++) {
			ids_buf[i] = (jint)line_types[i];
		}
		(*env)->SetIntArrayRegion(env, j_line_types, 0, (jsize)line_count, ids_buf);
		for (i = 0; i < line_count; i++) {
			ids_buf[i] = (jint)line_profiler_ids[i];
		}
		(*env)->SetIntArrayRegion(env, j_line_ids, 0, (jsize)line_count, ids_buf);
		free(ids_buf);
		ids_buf = NULL;
	}

	result_arr = (*env)->CallStaticObjectMethod(
		env, transformer_class, transform_method,
		input_arr, j_sigs, j_ids, j_line_nums, j_line_types, j_line_ids
	);

	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
		goto cleanup;
	}

	if (result_arr == NULL) {
		goto cleanup;
	}

	result_len = (*env)->GetArrayLength(env, result_arr);
	out = malloc((size_t)result_len);
	if (out == NULL) {
		goto cleanup;
	}
	(*env)->GetByteArrayRegion(env, result_arr, 0, result_len, (jbyte *)out);
	*new_len_out = (size_t)result_len;

cleanup:
	free(ids_buf);
	if (string_class != NULL) { (*env)->DeleteLocalRef(env, string_class); }
	if (input_arr    != NULL) { (*env)->DeleteLocalRef(env, input_arr);    }
	if (j_sigs       != NULL) { (*env)->DeleteLocalRef(env, j_sigs);       }
	if (j_ids        != NULL) { (*env)->DeleteLocalRef(env, j_ids);        }
	if (j_line_nums   != NULL) { (*env)->DeleteLocalRef(env, j_line_nums);   }
	if (j_line_types  != NULL) { (*env)->DeleteLocalRef(env, j_line_types);  }
	if (j_line_ids    != NULL) { (*env)->DeleteLocalRef(env, j_line_ids);    }
	if (result_arr   != NULL) { (*env)->DeleteLocalRef(env, result_arr);   }
	return out;
}
