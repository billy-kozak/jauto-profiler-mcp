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

#include "ps-thread-state.h"
#include "util/log.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define LOG_TAG "ps-thread-state"
#define MAX_SUSPEND_ITERATIONS 8

struct ps_thread_pause *ps_thread_pause_alloc(void)
{
	struct ps_thread_pause *state = malloc(sizeof(*state));
	if (state == NULL) {
		return NULL;
	}

	state->suspended_threads = NULL;
	state->suspended_count = 0;

	return state;
}

void ps_thread_pause_free(struct ps_thread_pause *state)
{
	free(state->suspended_threads);
	free(state);
}

/*
 * Returns 1 if thread should be suspended, 0 if it should be skipped.
 * Cleans up all fields of *info on return.
 */
static int classify_thread(
	jvmtiEnv *jvmti,
	JNIEnv *jni_env,
	jthread thread,
	jthread agent_thread
) {
	jvmtiThreadInfo info;
	jboolean is_daemon;

	if (agent_thread != NULL &&
		(*jni_env)->IsSameObject(jni_env, thread, agent_thread)) {
		return 0;
	}

	if ((*jvmti)->GetThreadInfo(jvmti, thread, &info) != JVMTI_ERROR_NONE) {
		return 0;
	}

	is_daemon = info.is_daemon;
	(*jvmti)->Deallocate(jvmti, (unsigned char *)info.name);
	if (info.thread_group != NULL) {
		(*jni_env)->DeleteLocalRef(jni_env, info.thread_group);
	}
	if (info.context_class_loader != NULL) {
		(*jni_env)->DeleteLocalRef(jni_env, info.context_class_loader);
	}

	return !is_daemon;
}

static void log_suspend_results(
	const jvmtiError *results, size_t count
) {
	for (int i = 0; i < count; i++) {
		if (results[i] == JVMTI_ERROR_NONE ||
			results[i] == JVMTI_ERROR_THREAD_NOT_ALIVE ||
			results[i] == JVMTI_ERROR_THREAD_SUSPENDED) {
			continue;
		}
		LOG_WARN(
			"SuspendThreadList: thread[%d] error=%d",
			(int)i, (int)results[i]
		);
	}
}

/*
 * Builds state->suspended_threads from an array of local refs to suspended
 * application threads. Converts each to a global ref for long-term storage.
 * Replaces any previously stored list. Returns 0 on success, -1 on error.
 */
static int build_snapshot(
	struct ps_thread_pause *state,
	JNIEnv *jni_env,
	const jthread *snapshot,
	size_t snap_count
) {
	jthread *arr = NULL;

	for(int i = 0; i < state->suspended_count; i++) {
		(*jni_env)->DeleteGlobalRef(
			jni_env, state->suspended_threads[i]
		);
	}
	state->suspended_count = 0;

	if(snap_count == 0){
		state->suspended_count = 0;
		return 0;
	}

	arr = realloc(state->suspended_threads, snap_count * sizeof(*arr));
	if (arr == NULL) {
		goto fail1;
	}
	state->suspended_threads = arr;

	for (int i = 0; i < snap_count; i++) {
		arr[i] = (*jni_env)->NewGlobalRef(jni_env, snapshot[i]);
		if (arr[i] == NULL) {
			goto fail0;
		}
		state->suspended_count += 1;
	}

	return 0;
fail0:
	for(int i = 0; i < state->suspended_count; i++) {
		(*jni_env)->DeleteGlobalRef(
			jni_env, state->suspended_threads[i]
		);
	}
fail1:
	state->suspended_count = 0;
	return -1;
}


static int fetch_threads(
	jvmtiEnv *jvmti,
	JNIEnv *jni_env,
	jthread agent_thread,
	size_t *suspend_count,
	size_t *to_suspend_count,
	jthread **suspended,
	jthread **to_suspend,
	jthread **threads_raw
) {
	*threads_raw = NULL;
	jthread *ret_list = NULL;

	jint thread_count;

	jvmtiError get_all_err= (*jvmti)->GetAllThreads(
		jvmti, &thread_count, threads_raw
	);

	if (get_all_err != JVMTI_ERROR_NONE) {
		LOG_ERROR("GetAllThreads failed");
		goto fail;
	}

	ret_list = malloc(thread_count * 2 * sizeof(*ret_list));
	if(ret_list == NULL) {
		goto fail;
	}
	*suspended = ret_list;
	*to_suspend = ret_list + thread_count;

	for(int i = 0; i < thread_count; i++) {
		jint thread_state;

		int include = classify_thread(
			jvmti, jni_env, (*threads_raw)[i], agent_thread
		);
		if (!include) {
			continue;
		}

		jvmtiError err = (*jvmti)->GetThreadState(
			jvmti, (*threads_raw)[i], &thread_state
		);
		bool is_suspended = (
			err == JVMTI_ERROR_NONE &&
			thread_state & JVMTI_THREAD_STATE_SUSPENDED
		);

		if (!is_suspended) {
			(*to_suspend)[*to_suspend_count] = (*threads_raw)[i];
			*to_suspend_count += 1;
		} else {
			(*suspended)[*suspend_count] = (*threads_raw)[i];
			*suspend_count += 1;
		}
	}

	return 0;
fail:
	if(*threads_raw != NULL) {
		(*jvmti)->Deallocate(jvmti, (unsigned char *)(*threads_raw));
	}
	free(ret_list);
	*suspended = NULL;
	*threads_raw = NULL;
	*to_suspend = NULL;
	return -1;
}

static void free_fetched_threads(
	jvmtiEnv *jvmti,
	jthread *suspended,
	jthread *to_suspend,
	jthread *threads_raw
) {
	free(suspended);

	if(threads_raw != NULL) {
		(*jvmti)->Deallocate(jvmti, (unsigned char *)threads_raw);
	}
}

static int convergence_check(
	struct ps_thread_pause *state,
	jvmtiEnv *jvmti,
	JNIEnv *jni_env,
	jthread agent_thread
) {
	jthread *to_suspend = NULL;
	jthread *suspended = NULL;
	jthread *threads_raw = NULL;

	size_t to_suspend_count = 0;
	size_t suspended_count = 0;

	int fetch_ret = fetch_threads(
		jvmti,
		jni_env,
		agent_thread,
		&suspended_count,
		&to_suspend_count,
		&suspended,
		&to_suspend,
		&threads_raw
	);

	if(fetch_ret != 0) {
		return -1;
	}

	int sn_ret = build_snapshot(
		state, jni_env, suspended, suspended_count
	);
	free_fetched_threads(
		jvmti, suspended, to_suspend, threads_raw
	);
	if (sn_ret != 0) {
		return -1;
	}

	if(to_suspend_count == 0) {
		return 0;
	} else {
		return 1;
	}
}

/*
 * One iteration of the convergence loop. Partitions live target threads
 * (non-daemon, non-agent) into already-suspended (snapshot) and still-running
 * (to_suspend). If to_suspend is empty all targets are accounted for:
 * builds state->suspended_threads from snapshot and returns 0 (done).
 * Otherwise suspends to_suspend and returns 1 (loop again). Returns -1 on
 * error.
 */
static int suspend_new_threads(
	struct ps_thread_pause *state,
	jvmtiEnv *jvmti,
	JNIEnv *jni_env,
	jthread agent_thread
) {
	int ret = 0;
	jthread *to_suspend = NULL;
	jthread *suspended = NULL;
	jthread *threads_raw = NULL;
	jvmtiError *results = NULL;
	size_t to_suspend_count = 0;
	size_t suspended_count = 0;

	int fetch_ret = fetch_threads(
		jvmti,
		jni_env,
		agent_thread,
		&suspended_count,
		&to_suspend_count,
		&suspended,
		&to_suspend,
		&threads_raw
	);

	if(fetch_ret != 0) {
		ret = -1;
		goto cleanup;
	}

	if (to_suspend_count == 0) {
		int sn_ret = build_snapshot(
			state, jni_env, suspended, suspended_count
		);
		if (sn_ret != 0) {
			ret = -1;
		}
		goto cleanup;
	}

	results = malloc(to_suspend_count * sizeof(*results));
	if (results == NULL) {
		ret = -1;
		goto cleanup;
	}

	(*jvmti)->SuspendThreadList(
		jvmti, to_suspend_count, to_suspend, results
	);
	log_suspend_results(results, to_suspend_count);
	ret = 1;

cleanup:
	free(results);
	free_fetched_threads(jvmti, suspended, to_suspend, threads_raw);
	return ret;
}


enum ps_suspend_result ps_thread_pause_suspend(
	struct ps_thread_pause *state,
	jvmtiEnv *jvmti,
	JNIEnv *jni_env,
	jthread agent_thread
) {
	int i;
	int ret;

	for (i = 0; i < MAX_SUSPEND_ITERATIONS; i++) {
		ret = suspend_new_threads(state, jvmti, jni_env, agent_thread);
		if (ret == 0) {
			return PS_SUSPEND_OK;
		}
		if (ret < 0) {
			return PS_SUSPEND_ERROR;
		}
	}

	/* Final convergence check after exhausting suspend iterations */
	ret = convergence_check(state, jvmti, jni_env, agent_thread);
	if (ret == 0) {
		return PS_SUSPEND_OK;
	}
	if (ret < 0) {
		return PS_SUSPEND_ERROR;
	}

	LOG_WARN("pause: gave up after %d iterations", MAX_SUSPEND_ITERATIONS);
	return PS_SUSPEND_RACE;
}
