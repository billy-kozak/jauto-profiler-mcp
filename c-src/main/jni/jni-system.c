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

#include "jni-system.h"

#include "util/log.h"

#include <jni.h>

#define LOG_TAG "jni-system"

#define SHUTDOWN_CLASS "app/autoprofiler/util/SysUtils"
#define EXIT_SIG       "(I)V"

void jni_system_exit(JNIEnv *env, int exit_code)
{
	jclass shutdown_class;
	jmethodID exit_method;

	shutdown_class = (*env)->FindClass(env, SHUTDOWN_CLASS);
	if (shutdown_class == NULL) {
		LOG_ERROR("FindClass " SHUTDOWN_CLASS " failed");
		return;
	}

	exit_method = (*env)->GetStaticMethodID(
		env, shutdown_class, "jniSafeExit", EXIT_SIG
	);
	if (exit_method == NULL) {
		LOG_ERROR("GetStaticMethodID Shutdown.exit failed");
		return;
	}

	(*env)->CallStaticVoidMethod(
		env, shutdown_class, exit_method, (jint)exit_code
	);
}
