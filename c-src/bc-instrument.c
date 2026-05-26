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
#include <stdio.h>

#define TRANSFORMER_CLASS "app/autoprofiler/BytecodeTransformer"
#define TRANSFORM_SIG     "([BLjava/lang/String;Ljava/lang/String;I)[B"

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
		fprintf(
			stderr,
			"jauto-profiler: FindClass BytecodeTransformer failed\n"
		);
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
		fprintf(
			stderr,
			"jauto-profiler: GetStaticMethodID transform failed\n"
		);
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
	const char *method_name,
	const char *method_desc,
	int profiler_id,
	size_t *new_len_out
) {
	jbyteArray input_arr;
	jstring j_method_name;
	jstring j_method_desc;
	jbyteArray result_arr;
	jsize result_len;
	unsigned char *out;

	if (bc_instrument_init_refs(env) != 0) {
		return NULL;
	}

	input_arr = (*env)->NewByteArray(env, (jsize)class_data_len);
	if (input_arr == NULL) {
		return NULL;
	}
	(*env)->SetByteArrayRegion(
		env, input_arr, 0, (jsize)class_data_len, (const jbyte *)class_data
	);

	j_method_name = (*env)->NewStringUTF(env, method_name);
	if (j_method_name == NULL) {
		(*env)->DeleteLocalRef(env, input_arr);
		return NULL;
	}

	j_method_desc = (*env)->NewStringUTF(env, method_desc);
	if (j_method_desc == NULL) {
		(*env)->DeleteLocalRef(env, input_arr);
		(*env)->DeleteLocalRef(env, j_method_name);
		return NULL;
	}

	result_arr = (*env)->CallStaticObjectMethod(
		env, transformer_class, transform_method,
		input_arr, j_method_name, j_method_desc, (jint)profiler_id
	);

	(*env)->DeleteLocalRef(env, input_arr);
	(*env)->DeleteLocalRef(env, j_method_name);
	(*env)->DeleteLocalRef(env, j_method_desc);

	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
		return NULL;
	}

	if (result_arr == NULL) {
		return NULL;
	}

	result_len = (*env)->GetArrayLength(env, result_arr);
	out = malloc((size_t)result_len);
	if (out == NULL) {
		(*env)->DeleteLocalRef(env, result_arr);
		return NULL;
	}

	(*env)->GetByteArrayRegion(
		env, result_arr, 0, result_len, (jbyte *)out
	);
	(*env)->DeleteLocalRef(env, result_arr);

	*new_len_out = (size_t)result_len;
	return out;
}
