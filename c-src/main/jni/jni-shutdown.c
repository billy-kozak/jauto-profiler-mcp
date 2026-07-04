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

#include "jni-shutdown.h"

#include "util/log.h"

#include <jni.h>
#include <stdint.h>

#define LOG_TAG "jni-shutdown"

#define SHUTDOWN_HOOK_CLASS  "app/autoprofiler/util/ShutdownHook"
#define REGISTER_SIG         "()V"
#define NATIVE_CB_SIG        "()V"
#define ADD_CALLBACK_SIG     "(JJ)V"
#define GET_CALLBACKS_SIG    "()[J"

static void invoke_callbacks(JNIEnv *env)
{
	jclass hook_class;
	jmethodID get_callbacks;
	jlongArray callbacks;
	jlong *elems;
	jsize len;
	jsize i;

	hook_class = (*env)->FindClass(env, SHUTDOWN_HOOK_CLASS);
	if (hook_class == NULL) {
		LOG_ERROR("FindClass " SHUTDOWN_HOOK_CLASS " failed");
		return;
	}

	get_callbacks = (*env)->GetStaticMethodID(
		env, hook_class, "getCallbacks", GET_CALLBACKS_SIG
	);
	if (get_callbacks == NULL) {
		LOG_ERROR("GetStaticMethodID getCallbacks failed");
		(*env)->DeleteLocalRef(env, hook_class);
		return;
	}

	callbacks = (*env)->CallStaticObjectMethod(
		env, hook_class, get_callbacks
	);
	(*env)->DeleteLocalRef(env, hook_class);

	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionClear(env);
		LOG_ERROR("getCallbacks threw an exception");
		return;
	}
	if (callbacks == NULL) {
		return;
	}

	len = (*env)->GetArrayLength(env, callbacks);
	elems = (*env)->GetLongArrayElements(env, callbacks, NULL);
	if (elems == NULL) {
		(*env)->DeleteLocalRef(env, callbacks);
		return;
	}

	for (i = 0; i + 1 < len; i += 2) {
		jvm_shutdown_fn fn = (jvm_shutdown_fn)(uintptr_t)elems[i];
		void *data = (void *)(uintptr_t)elems[i + 1];
		if (fn != NULL) {
			fn(data);
		}
	}

	(*env)->ReleaseLongArrayElements(env, callbacks, elems, JNI_ABORT);
	(*env)->DeleteLocalRef(env, callbacks);
}

static void shutdown_callback(JNIEnv *env, jclass cls)
{
	(void)cls;
	invoke_callbacks(env);
}

int jni_register_shutdown_hook(JNIEnv *env)
{
	jclass hook_class;
	jmethodID register_method;
	JNINativeMethod native_methods[] = {
		{
			"nativeShutdownCallback",
			NATIVE_CB_SIG,
			(void *)shutdown_callback
		}
	};

	hook_class = (*env)->FindClass(env, SHUTDOWN_HOOK_CLASS);
	if (hook_class == NULL) {
		LOG_ERROR("FindClass " SHUTDOWN_HOOK_CLASS " failed");
		return -1;
	}

	if ((*env)->RegisterNatives(env, hook_class, native_methods, 1) != 0) {
		LOG_ERROR("RegisterNatives for nativeShutdownCallback failed");
		(*env)->DeleteLocalRef(env, hook_class);
		return -1;
	}

	register_method = (*env)->GetStaticMethodID(
		env, hook_class, "registerShutdownHook", REGISTER_SIG
	);
	if (register_method == NULL) {
		LOG_ERROR("GetStaticMethodID registerShutdownHook failed");
		(*env)->DeleteLocalRef(env, hook_class);
		return -1;
	}

	(*env)->CallStaticVoidMethod(env, hook_class, register_method);
	(*env)->DeleteLocalRef(env, hook_class);

	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionClear(env);
		LOG_ERROR("registerShutdownHook threw an exception");
		return -1;
	}

	return 0;
}

int jni_add_shutdown_callback(JNIEnv *env, jvm_shutdown_fn fn, void *data)
{
	jclass hook_class;
	jmethodID add_method;

	hook_class = (*env)->FindClass(env, SHUTDOWN_HOOK_CLASS);
	if (hook_class == NULL) {
		LOG_ERROR("FindClass " SHUTDOWN_HOOK_CLASS " failed");
		return -1;
	}

	add_method = (*env)->GetStaticMethodID(
		env, hook_class, "addCallback", ADD_CALLBACK_SIG
	);
	if (add_method == NULL) {
		LOG_ERROR("GetStaticMethodID addCallback failed");
		(*env)->DeleteLocalRef(env, hook_class);
		return -1;
	}

	(*env)->CallStaticVoidMethod(
		env, hook_class, add_method,
		(jlong)(uintptr_t)fn,
		(jlong)(uintptr_t)data
	);
	(*env)->DeleteLocalRef(env, hook_class);

	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionClear(env);
		LOG_ERROR("addCallback threw an exception");
		return -1;
	}

	return 0;
}
