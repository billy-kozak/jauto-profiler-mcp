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

#include "jvm-shutdown.h"

#include "jni/jni-shutdown.h"
#include "util/log.h"

#include <jni.h>

#define LOG_TAG "jvm-shutdown"

static void log_shutdown_callback(void *data)
{
	(void)data;
	LOG_INFO("JVM shutdown hook called");
}

int jvm_register_shutdown_hook(JNIEnv *env)
{
	if (jni_register_shutdown_hook(env) != 0) {
		return -1;
	}
	return jni_add_shutdown_callback(env, log_shutdown_callback, NULL);
}
