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

#include <stdio.h>
#include <string.h>

static jvmtiEnv *jvmti = NULL;

static void JNICALL
class_file_load_hook(
    jvmtiEnv *jvmti_env,
    JNIEnv *jni_env,
    jclass class_being_redefined,
    jobject loader,
    const char *name,
    jobject protection_domain,
    jint class_data_len,
    const unsigned char *class_data,
    jint *new_class_data_len,
    unsigned char **new_class_data)
{
    /* Anonymous/generated classes may have no name */
    if (name == NULL)
        return;

    char filename[1024];

    /*
       JVMTI names look like:

           java/lang/String
           com/example/Foo

       Convert into:

           dump_java_lang_String.class
    */

    snprintf(
        filename,
        sizeof(filename),
        "dump_%s.class",
        name);

    for (char *p = filename; *p; p++) {
        if (*p == '/')
            *p = '_';
    }

    FILE *f = fopen(filename, "wb");
    if (!f)
        return;

    fwrite(
        class_data,
        1,
        class_data_len,
        f);

    fclose(f);

    printf("dumped %s (%d bytes)\n",
           name,
           class_data_len);
}

JNIEXPORT jint JNICALL
Agent_OnLoad(
    JavaVM *vm,
    char *options,
    void *reserved)
{
    if ((*vm)->GetEnv(
            vm,
            (void**)&jvmti,
            JVMTI_VERSION_1_2) != JNI_OK)
    {
        return JNI_ERR;
    }

    jvmtiCapabilities caps;
    memset(&caps,0,sizeof(caps));

    caps.can_generate_all_class_hook_events = 1;

    (*jvmti)->AddCapabilities(
        jvmti,
        &caps);

    jvmtiEventCallbacks cb;
    memset(&cb,0,sizeof(cb));

    cb.ClassFileLoadHook =
        &class_file_load_hook;

    (*jvmti)->SetEventCallbacks(
        jvmti,
        &cb,
        sizeof(cb));

    (*jvmti)->SetEventNotificationMode(
        jvmti,
        JVMTI_ENABLE,
        JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,
        NULL);

    printf("agent ready\n");

    return JNI_OK;
}

JNIEXPORT void JNICALL
Agent_OnUnload(JavaVM *vm)
{
    printf("agent unloaded\n");
}
