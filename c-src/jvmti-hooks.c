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

#include <jvmti.h>
#include <jni.h>

#include <string.h>

#include "prof-server.h"
#include "prof-server-ev.h"
#include "util/log.h"

#define LOG_TAG "jvmti-hooks"

static jvmtiEnv *jvmti = NULL;
static struct prof_server *server = NULL;

static void JNICALL class_file_load_hook(
    jvmtiEnv *jvmti_env,
    JNIEnv *jni_env,
    jclass class_being_redefined,
    jobject loader,
    const char *name,
    jobject protection_domain,
    jint class_data_len,
    const unsigned char *class_data,
    jint *new_class_data_len,
    unsigned char **new_class_data
) {
	(void)jni_env;
	(void)loader;
	(void)protection_domain;

	if (class_being_redefined != NULL) {
		if (server != NULL && name != NULL) {
			ps_handle_retransform(
				server, jvmti_env, name,
				new_class_data_len, new_class_data
			);
		}
		return;
	}

	if (name != NULL) {
		if (server != NULL) {
			ps_send_class_loaded(
				server,
				name,
				class_data,
				(size_t)class_data_len
			);
		} else {
			LOG_WARN("server was null at class load");
		}
	} else {
		LOG_DEBUG("loaded unnamed class");
	}
}

static void JNICALL vm_init(
    jvmtiEnv *jvmti_env,
    JNIEnv *jni_env,
    jthread thread
) {
	if (ps_start(server, jvmti_env, jni_env) == NULL) {
		LOG_ERROR("ps_start failed");
	}
}

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *vm, char *options, void *reserved)
{
	jint ret = (*vm)->GetEnv(vm, (void**)&jvmti, JVMTI_VERSION_1_2);

	if (ret != JNI_OK) {
		return JNI_ERR;
	}

	jvmtiCapabilities caps;
	memset(&caps,0,sizeof(caps));

	caps.can_generate_all_class_hook_events = 1;
	caps.can_retransform_classes = 1;
	(*jvmti)->AddCapabilities(jvmti, &caps);

	jvmtiEventCallbacks cb;
	memset(&cb,0,sizeof(cb));

	cb.ClassFileLoadHook = &class_file_load_hook;
	cb.VMInit = &vm_init;

	(*jvmti)->SetEventCallbacks(
		jvmti,
		&cb,
		sizeof(cb)
	);

	(*jvmti)->SetEventNotificationMode(
		jvmti,
		JVMTI_ENABLE,
		JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,
		NULL
	);

	(*jvmti)->SetEventNotificationMode(
		jvmti,
		JVMTI_ENABLE,
		JVMTI_EVENT_VM_INIT,
		NULL
	);

	log_init();

	server = ps_init();
	if (server == NULL) {
		return JNI_ERR;
	}

	LOG_INFO("agent ready");

	return JNI_OK;
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm)
{
	if (server != NULL) {
		ps_destroy(server);
		server = NULL;
	}
	LOG_INFO("agent unloaded");
}
