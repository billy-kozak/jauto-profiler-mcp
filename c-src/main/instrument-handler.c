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

#include "instrument-handler.h"

#include "class-info.h"
#include "class-info-list.h"
#include "queued-instrument.h"
#include "master-instruments.h"
#include "prof-err-log.h"
#include "jni/jni-profiler.h"
#include "jni/bc-instrument.h"
#include "util/log.h"

#include <jvmti.h>
#include <jni.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define LOG_TAG "instrument-handler"

int ih_ctx_init(struct instr_ctx *ctx)
{
	ctx->pending = (struct pending_instrument){0};
	ctx->master_instruments = NULL;

	if (ci_list_init(&ctx->loaded_classes) != 0) {
		return -1;
	}

	if (queued_instr_list_init(&ctx->queued_instruments, 0) != 0) {
		ci_list_deep_destroy(&ctx->loaded_classes);
		return -1;
	}

	ctx->master_instruments = mi_alloc();
	if (ctx->master_instruments == NULL) {
		queued_instr_list_deep_destroy(&ctx->queued_instruments);
		ci_list_deep_destroy(&ctx->loaded_classes);
		return -1;
	}

	return 0;
}

void ih_ctx_destroy(struct instr_ctx *ctx)
{
	ci_list_deep_destroy(&ctx->loaded_classes);
	queued_instr_list_deep_destroy(&ctx->queued_instruments);
	mi_free(ctx->master_instruments);
}

static unsigned char *do_instrumentation(
	JNIEnv *jni_env,
	const struct class_info *ci,
	size_t *new_bc_len_out
) {
	struct class_instrument_params *params;
	unsigned char *result;

	params = class_instrument_params_alloc(ci);
	if (params == NULL) {
		return NULL;
	}

	result = bc_instrument_method(
		jni_env,
		params->class_data,
		params->class_data_len,
		params->method_sigs,
		params->profiler_ids, params->count,
		params->line_numbers,
		params->line_profiler_ids, params->line_count,
		new_bc_len_out
	);

	if (result == NULL) {
		LOG_ERROR("bc_instrument_method failed");
	}

	class_instrument_params_free(params);
	return result;
}

static int retransform_pending(
	struct instr_ctx *ctx,
	JNIEnv *jni_env,
	jvmtiEnv *jvmti,
	const char *class_name,
	const char *method_sig,
	const unsigned char *new_bc,
	size_t new_bc_len,
	int profiler_id
) {
	int ret;

	ctx->pending.class_name   = class_name;
	ctx->pending.method_sig   = method_sig;
	ctx->pending.bytecode     = new_bc;
	ctx->pending.bytecode_len = new_bc_len;
	ctx->pending.profiler_id  = profiler_id;

	ret = jni_retransform_class(jni_env, jvmti, class_name);
	if (ret != 0) {
		LOG_ERROR("RetransformClasses failed");
	}

	ctx->pending.class_name   = NULL;
	ctx->pending.method_sig   = NULL;
	ctx->pending.bytecode     = NULL;
	ctx->pending.bytecode_len = 0;
	ctx->pending.profiler_id  = -1;

	return ret;
}

static enum instrument_resp_status instrument_later(
	struct instr_ctx *ctx,
	const char *class_name,
	const char *method_sig,
	uint64_t instrument_id,
	int profiler_id
) {
	struct queued_instr *qi = queued_instr_method_add(
		&ctx->queued_instruments,
		class_name,
		method_sig,
		profiler_id,
		instrument_id
	);
	bool alloc_ok = (
		qi != NULL && qi->class_name != NULL && qi->method_sig != NULL
	);
	if (!alloc_ok) {
		if (qi != NULL) {
			queued_instr_list_remove_and_destroy(
				&ctx->queued_instruments,
				(int)(ctx->queued_instruments.len - 1)
			);
		}
		mi_remove(ctx->master_instruments, instrument_id);
		return INSTRUMENT_RP_ERROR;
	}
	return INSTRUMENT_RP_DEFERRED;
}

static int create_profiler_for_method(
	JNIEnv *jni_env,
	const struct mi_entry *entry,
	uint64_t instrument_id
) {
	int id = jni_create_profiler(
		jni_env, instrument_id,
		(char *)entry->entry_class_name->str,
		(char *)entry->method.method_name->str
	);
	if (id < 0 && id != -1) {
		LOG_ERROR("jni_create_profiler failed");
	}
	return id;
}

static int populate_class_info(
	struct class_info *ci,
	const struct mi_entry *entry,
	int profiler_id,
	uint64_t instrument_id
) {
	const char *method_sig = (char *)entry->method.method_name->str;
	struct instrumented_method *im = instrumented_method_list_add_and_init(
		&ci->instrumented, method_sig, profiler_id
	);
	if (im == NULL || im->method_sig == NULL) {
		if (im != NULL) {
			ci->instrumented.len--;
		}
		return -1;
	}
	im->instrument_id = instrument_id;
	return 0;
}

static int populate_class_info_line(
	struct class_info *ci_entry,
	struct class_info *ci_exit,
	const struct mi_entry *entry,
	int profiler_id
) {
	struct instrumented_line *r = ci_add_instrumented_line(
		ci_entry,
		entry->line.entry_line_number,
		profiler_id,
		INSTRUMENT_ENTER
	);
	if (r == NULL) {
		return -1;
	}
	r = ci_add_instrumented_line(
		ci_exit,
		entry->line.exit_line_number,
		profiler_id,
		INSTRUMENT_EXIT
	);

	if (r == NULL) {
		ci_remove_instrumented_line(
			ci_entry, entry->line.entry_line_number
		);
		return -1;
	}
	return 0;
}

static int perform_instrumentation(
	struct instr_ctx *ctx,
	JNIEnv *jni_env,
	jvmtiEnv *jvmti,
	struct class_info *ci,
	const struct mi_entry *entry,
	int profiler_id,
	uint64_t instrument_id
) {
	const char *method_sig = (entry->type == MI_METHOD)
		? (char *)entry->method.method_name->str
		: NULL;
	unsigned char *new_bc;
	size_t new_bc_len;
	int ret;

	new_bc = do_instrumentation(jni_env, ci, &new_bc_len);
	if (new_bc == NULL) {
		return -1;
	}

	ret = retransform_pending(
		ctx, jni_env, jvmti,
		(char *)ci->name->str, method_sig,
		new_bc, new_bc_len, profiler_id
	);
	free(new_bc);

	if (ret != 0) {
		return -1;
	}

	if (entry->type == MI_METHOD) {
		mi_mark_method_applied(ctx->master_instruments, instrument_id);
	}
	return 0;
}

static int perform_instrumentation_line(
	struct instr_ctx *ctx,
	JNIEnv *jni_env,
	jvmtiEnv *jvmti,
	struct class_info *ci_entry,
	struct class_info *ci_exit,
	const struct mi_entry *entry,
	int profiler_id,
	uint64_t instrument_id
) {
	int r = perform_instrumentation(
		ctx,
		jni_env,
		jvmti,
		ci_entry,
		entry,
		profiler_id,
		instrument_id
	);
	if (r != 0) {
		goto fail;
	}

	if (ci_exit != ci_entry) {
		r = perform_instrumentation(
			ctx,
			jni_env,
			jvmti,
			ci_exit,
			entry,
			profiler_id,
			instrument_id
		);
		if (r != 0) {
			goto fail;
		}
	}
	mi_mark_line_entry_applied(ctx->master_instruments, instrument_id);
	mi_mark_line_exit_applied(ctx->master_instruments, instrument_id);
	return 0;

fail:
	ci_remove_instrumented_line(ci_entry, entry->line.entry_line_number);
	ci_remove_instrumented_line(ci_exit, entry->line.exit_line_number);
	jni_remove_profiler(jni_env, profiler_id);
	mi_remove(ctx->master_instruments, instrument_id);
	return -1;
}

static void undo_ci_and_mi(
	const struct mi_entry *entry,
	struct class_info *ci,
	struct instr_ctx *ctx,
	uint64_t instrument_id
) {
	uint64_t ignored_id;
	ci_remove_instrumented_by_sig(
		ci, (char *)entry->method.method_name->str, &ignored_id
	);
	mi_remove(ctx->master_instruments, instrument_id);
}

static enum instrument_resp_status instrument_now(
	struct instr_ctx *ctx,
	JNIEnv *jni_env,
	jvmtiEnv *jvmti,
	struct class_info *ci,
	uint64_t instrument_id,
	int profiler_id
) {
	const struct mi_entry *entry = mi_find(
		ctx->master_instruments, instrument_id
	);

	int pop_ret = populate_class_info(
		ci, entry, profiler_id, instrument_id
	);

	if (pop_ret != 0) {
		jni_remove_profiler(jni_env, profiler_id);
		mi_remove(ctx->master_instruments, instrument_id);
		return INSTRUMENT_RP_ERROR;
	}

	int instr_ret = perform_instrumentation(
		ctx, jni_env, jvmti, ci, entry, profiler_id, instrument_id
	);
	if (instr_ret != 0) {
		undo_ci_and_mi(entry, ci, ctx, instrument_id);
		jni_remove_profiler(jni_env, profiler_id);
		return INSTRUMENT_RP_ERROR;
	}

	return INSTRUMENT_RP_OK;
}

static void do_deferred_method_instrumentation(
	struct instr_ctx *ctx,
	JNIEnv *jni_env,
	jvmtiEnv *jvmti,
	struct prof_err_log *err_log,
	struct class_info *ci,
	const struct queued_instr *qi
) {
	const char *method_sig = (char *)qi->method_sig->str;
	enum instrument_resp_status status = instrument_now(
		ctx, jni_env, jvmti, ci, qi->instrument_id, qi->profiler_id
	);
	if (status != INSTRUMENT_RP_OK) {
		LOG_WARN(
			"deferred instrumentation failed for %s %s",
			(char *)ci->name->str, method_sig
		);
		prof_err_log_printf(
			err_log,
			"deferred instrumentation failed: %s %s",
			(char *)ci->name->str, method_sig
		);
	}
}

static void do_deferred_line_instrumentation(
	struct instr_ctx *ctx,
	JNIEnv *jni_env,
	jvmtiEnv *jvmti,
	struct prof_err_log *err_log,
	const struct queued_instr *qi
) {
	uint64_t instrument_id = qi->instrument_id;
	int profiler_id = qi->profiler_id;

	if (qi->type == QI_LINE_ENTER) {
		mi_mark_line_entry_applied(
			ctx->master_instruments, instrument_id
		);
	} else {
		mi_mark_line_exit_applied(
			ctx->master_instruments, instrument_id
		);
	}

	const struct mi_entry *entry = mi_find(
		ctx->master_instruments, instrument_id
	);
	if (entry == NULL || !mi_line_is_ready(entry)) {
		return;
	}

	struct class_info *ci_entry = ci_list_find_by_name(
		&ctx->loaded_classes, (char *)entry->entry_class_name->str
	);
	struct class_info *ci_exit = ci_list_find_by_name(
		&ctx->loaded_classes, (char *)entry->line.exit_class_name->str
	);

	if (ci_entry == NULL || ci_exit == NULL) {
		LOG_WARN("deferred line instrumentation: class not found");
		prof_err_log_printf(
			err_log,
			"deferred line instrumentation: class not found"
		);
		jni_remove_profiler(jni_env, profiler_id);
		mi_remove(ctx->master_instruments, instrument_id);
		return;
	}

	int pop_ret = populate_class_info_line(
		ci_entry, ci_exit, entry, profiler_id
	);
	if (pop_ret != 0) {
		LOG_WARN(
			"deferred line instrumentation failed for %s %s",
			(char *)entry->entry_class_name->str,
			(char *)entry->line.exit_class_name->str
		);
		prof_err_log_printf(
			err_log,
			"deferred line instrumentation failed: %s %s",
			(char *)entry->entry_class_name->str,
			(char *)entry->line.exit_class_name->str
		);
		jni_remove_profiler(jni_env, profiler_id);
		mi_remove(ctx->master_instruments, instrument_id);
		return;
	}

	int instr_ret = perform_instrumentation_line(
		ctx, jni_env, jvmti,
		ci_entry, ci_exit, entry, profiler_id, instrument_id
	);
	if (instr_ret != 0) {
		prof_err_log_printf(
			err_log,
			"deferred line instrumentation failed: %s %s",
			(char *)entry->entry_class_name->str,
			(char *)entry->line.exit_class_name->str
		);
	}
}

static void do_deferred_instrumentations(
	struct instr_ctx *ctx,
	JNIEnv *jni_env,
	jvmtiEnv *jvmti,
	struct prof_err_log *err_log,
	struct class_info *ci
) {
	while (true) {
		int idx = queued_instr_list_find_by_class(
			&ctx->queued_instruments, (char *)ci->name->str
		);
		if (idx < 0) {
			break;
		}

		struct queued_instr *qi = &ctx->queued_instruments.arr[idx];
		if (qi->type == QI_METHOD) {
			do_deferred_method_instrumentation(
				ctx, jni_env, jvmti, err_log, ci, qi
			);
		} else {
			do_deferred_line_instrumentation(
				ctx, jni_env, jvmti, err_log, qi
			);
		}
		queued_instr_list_remove_and_destroy(
			&ctx->queued_instruments, idx
		);
	}
}

static int remove_queued_instr_by_id(
	struct queued_instr_list *queue, uint64_t iid
) {
	int profiler_id = -1;
	enum qi_type type;

	int idx = queued_instr_list_find_by_instrument_id(queue, iid);

	if(idx < 0) {
		return -1;
	}

	profiler_id = queue->arr[idx].profiler_id;
	type = queue->arr[idx].type;

	queued_instr_list_remove_and_destroy(queue, idx);

	if(type == QI_METHOD) {
		assert(
			queued_instr_list_find_by_instrument_id(queue, iid) < 0
		);
		return profiler_id;
	}

	idx = queued_instr_list_find_by_instrument_id(queue, iid);

	if(idx < 0) {
		return profiler_id;
	}

	assert(queue->arr[idx].profiler_id == profiler_id);
	assert(queue->arr[idx].type != QI_METHOD);

	queued_instr_list_remove_and_destroy(queue, idx);

	assert(queued_instr_list_find_by_instrument_id(queue, iid) < 0);

	return profiler_id;
}

static enum deinstrument_resp_status deinstrument_line_by_id(
	struct instr_ctx *ctx,
	JNIEnv *jni_env,
	jvmtiEnv *jvmti,
	const struct mi_entry *entry,
	uint64_t instrument_id
) {
	enum deinstrument_resp_status status = DEINSTRUMENT_RP_OK;
	unsigned char *new_bc = NULL;
	size_t new_bc_len;
	int profiler_id = -1;

	if (!mi_line_is_ready(entry)) {
		profiler_id = remove_queued_instr_by_id(
			&ctx->queued_instruments, instrument_id
		);
		if (profiler_id >= 0) {
			if (jni_remove_profiler(jni_env, profiler_id) != 0) {
				LOG_ERROR(
					"jni_remove_profiler failed for "
					"deferred line deinstrument"
				);
			}
		}
		mi_remove(ctx->master_instruments, instrument_id);
		return DEINSTRUMENT_RP_OK;
	}

	struct class_info *ci_entry = ci_list_find_by_name(
		&ctx->loaded_classes, (char *)entry->entry_class_name->str
	);
	struct class_info *ci_exit = ci_list_find_by_name(
		&ctx->loaded_classes, (char *)entry->line.exit_class_name->str
	);

	if (ci_entry == NULL || ci_exit == NULL) {
		LOG_WARN("deinstrument-by-id line: class not found");
		return DEINSTRUMENT_RP_FAIL;
	}

	profiler_id = ci_remove_instrumented_line(
		ci_entry, entry->line.entry_line_number
	);
	ci_remove_instrumented_line(ci_exit, entry->line.exit_line_number);

	new_bc = do_instrumentation(jni_env, ci_entry, &new_bc_len);
	if (new_bc == NULL) {
		status = DEINSTRUMENT_RP_FAIL;
		goto cleanup;
	}
	retransform_pending(
		ctx, jni_env, jvmti, (char *)ci_entry->name->str,
		NULL, new_bc, new_bc_len, -1
	);
	free(new_bc);
	new_bc = NULL;

	if (ci_exit != ci_entry) {
		new_bc = do_instrumentation(jni_env, ci_exit, &new_bc_len);
		if (new_bc == NULL) {
			status = DEINSTRUMENT_RP_FAIL;
			goto cleanup;
		}
		retransform_pending(
			ctx, jni_env, jvmti, (char *)ci_exit->name->str,
			NULL, new_bc, new_bc_len, -1
		);
		free(new_bc);
		new_bc = NULL;
	}

cleanup:
	free(new_bc);
	mi_remove(ctx->master_instruments, instrument_id);
	if (profiler_id >= 0) {
		if(jni_remove_profiler(jni_env, profiler_id) != 0) {
			LOG_ERROR(
				"jni_remove_profiler failed for line "
				"deinstrument"
			);
		}
	}
	return status;
}

static enum deinstrument_resp_status deinstrument_method_by_id(
	struct instr_ctx *ctx,
	JNIEnv *jni_env,
	jvmtiEnv *jvmti,
	const struct mi_entry *entry,
	uint64_t instrument_id
) {
	enum deinstrument_resp_status status = DEINSTRUMENT_RP_OK;
	struct class_info *ci;
	unsigned char *new_bc = NULL;
	size_t new_bc_len;
	int profiler_id;
	uint64_t ignored_id;

	if (entry->method.deferred) {
		int deferred_profiler_id = remove_queued_instr_by_id(
			&ctx->queued_instruments, instrument_id
		);
		if (deferred_profiler_id >= 0) {
			int rm_ret = jni_remove_profiler(
				jni_env, deferred_profiler_id
			);
			if (rm_ret != 0) {
				LOG_ERROR(
					"jni_remove_profiler failed for "
					"deferred deinstrument-by-id"
				);
			}
		}
		mi_remove(ctx->master_instruments, instrument_id);
		return DEINSTRUMENT_RP_OK;
	}

	ci = ci_list_find_by_name(
		&ctx->loaded_classes, (char *)entry->entry_class_name->str
	);
	if (ci == NULL) {
		LOG_WARN(
			"deinstrument-by-id: class not found: %s",
			(char *)entry->entry_class_name->str
		);
		return DEINSTRUMENT_RP_FAIL;
	}

	profiler_id = ci_remove_instrumented_by_sig(
		ci, (char *)entry->method.method_name->str, &ignored_id
	);
	if (profiler_id == -1) {
		LOG_WARN(
			"deinstrument-by-id: not in instrumented list: %s %s",
			(char *)entry->entry_class_name->str,
			(char *)entry->method.method_name->str
		);
		return DEINSTRUMENT_RP_FAIL;
	}

	new_bc = do_instrumentation(jni_env, ci, &new_bc_len);
	if (new_bc == NULL) {
		status = DEINSTRUMENT_RP_FAIL;
		goto cleanup;
	}
	if (retransform_pending(
		ctx, jni_env, jvmti,
		(char *)entry->entry_class_name->str,
		NULL, new_bc, new_bc_len, -1
	) != 0) {
		LOG_ERROR("deinstrument-by-id retransform failed");
		status = DEINSTRUMENT_RP_FAIL;
	}

cleanup:
	free(new_bc);
	mi_remove(ctx->master_instruments, instrument_id);
	if (jni_remove_profiler(jni_env, profiler_id) != 0) {
		LOG_ERROR("jni_remove_profiler failed");
	}
	return status;
}

/* ── public entry points ────────────────────────────────────────────────── */

enum instrument_resp_status ih_instrument_method(
	struct instr_ctx *ctx,
	JNIEnv *jni_env,
	jvmtiEnv *jvmti,
	const char *class_name,
	const char *method_sig,
	uint64_t *instrument_id_out
) {
	struct mi_val miv = mi_add_method(
		ctx->master_instruments, class_name, class_name, method_sig
	);
	uint64_t instrument_id = miv.id;
	const struct mi_entry *entry = miv.entry;

	*instrument_id_out = instrument_id;

	if (instrument_id == 0) {
		return INSTRUMENT_RP_ERROR;
	}

	int profiler_id = create_profiler_for_method(
		jni_env, entry, instrument_id
	);
	if (profiler_id < 0) {
		mi_remove(ctx->master_instruments, instrument_id);
		return profiler_id == -1 ?
			INSTRUMENT_RP_DOUBLE_INSTRUMENT : INSTRUMENT_RP_ERROR;
	}

	struct class_info *ci = ci_list_find_by_name(
		&ctx->loaded_classes, class_name
	);
	if (ci == NULL) {
		enum instrument_resp_status status = instrument_later(
			ctx, class_name, method_sig, instrument_id, profiler_id
		);
		if (status == INSTRUMENT_RP_ERROR) {
			jni_remove_profiler(jni_env, profiler_id);
		}
		return status;
	}

	return instrument_now(
		ctx, jni_env, jvmti, ci, instrument_id, profiler_id
	);
}

static enum instrument_line_resp_status instrument_later_line(
	struct instr_ctx *ctx,
	const struct mi_entry *entry,
	int profiler_id,
	uint64_t instrument_id,
	bool entry_loaded,
	bool exit_loaded
) {
	bool same_class = (strcmp(
		(char *)entry->entry_class_name->str,
		(char *)entry->line.exit_class_name->str
	) == 0);

	if (!entry_loaded) {
		struct queued_instr *qi = queued_instr_line_add(
			&ctx->queued_instruments,
			(char *)entry->entry_class_name->str,
			entry->line.entry_line_number,
			profiler_id, instrument_id, QI_LINE_ENTER
		);
		if (qi == NULL || qi->class_name == NULL) {
			goto fail;
		}
	} else {
		mi_mark_line_entry_applied(
			ctx->master_instruments, instrument_id
		);
	}

	if (!exit_loaded && !same_class) {
		struct queued_instr *qi = queued_instr_line_add(
			&ctx->queued_instruments,
			(char *)entry->line.exit_class_name->str,
			entry->line.exit_line_number,
			profiler_id, instrument_id, QI_LINE_EXIT
		);
		if (qi == NULL || qi->class_name == NULL) {
			goto fail;
		}
	} else {
		mi_mark_line_exit_applied(
			ctx->master_instruments, instrument_id
		);
	}

	return INSTRUMENT_LINE_RP_DEFERRED;

fail:
	remove_queued_instr_by_id(&ctx->queued_instruments, instrument_id);
	mi_remove(ctx->master_instruments, instrument_id);
	return INSTRUMENT_LINE_RP_ERROR;
}

enum instrument_line_resp_status ih_instrument_line(
	struct instr_ctx *ctx,
	JNIEnv *jni_env,
	jvmtiEnv *jvmti,
	const struct pstring *entry_class,
	const struct pstring *exit_class,
	uint32_t entry_line,
	uint32_t exit_line,
	uint64_t *instrument_id_out
) {
	struct mi_val miv = mi_add_linep(
		ctx->master_instruments,
		entry_class, exit_class,
		(int)entry_line, (int)exit_line
	);
	uint64_t instrument_id = miv.id;
	const struct mi_entry *entry = miv.entry;

	*instrument_id_out = instrument_id;

	if (instrument_id == 0) {
		return INSTRUMENT_LINE_RP_ERROR;
	}

	struct class_info *ci_entry = ci_list_find_by_name(
		&ctx->loaded_classes, (char *)entry_class->str
	);
	struct class_info *ci_exit = ci_list_find_by_name(
		&ctx->loaded_classes, (char *)exit_class->str
	);

	int profiler_id = jni_create_line_profiler(
		jni_env, instrument_id,
		(char *)entry_class->str, (char *)exit_class->str,
		(int)entry_line, (int)exit_line
	);
	if (profiler_id < 0) {
		mi_remove(ctx->master_instruments, instrument_id);
		return INSTRUMENT_LINE_RP_ERROR;
	}

	if (ci_entry == NULL || ci_exit == NULL) {
		enum instrument_line_resp_status stat = instrument_later_line(
			ctx, entry, profiler_id, instrument_id,
			ci_entry != NULL, ci_exit != NULL
		);
		if (stat == INSTRUMENT_LINE_RP_ERROR) {
			jni_remove_profiler(jni_env, profiler_id);
		}
		return stat;
	}

	int pop_ret = populate_class_info_line(
		ci_entry, ci_exit, entry, profiler_id
	);
	if (pop_ret != 0) {
		jni_remove_profiler(jni_env, profiler_id);
		mi_remove(ctx->master_instruments, instrument_id);
		return INSTRUMENT_LINE_RP_ERROR;
	}

	int instr_ret = perform_instrumentation_line(
		ctx, jni_env, jvmti, ci_entry, ci_exit, entry, profiler_id,
		instrument_id
	);
	return instr_ret != 0 ?
		INSTRUMENT_LINE_RP_ERROR : INSTRUMENT_LINE_RP_OK;
}

enum deinstrument_resp_status ih_deinstrument_method(
	struct instr_ctx *ctx,
	JNIEnv *jni_env,
	jvmtiEnv *jvmti,
	const char *class_name,
	const char *method_sig
) {
	struct mi_val found = mi_find_method_by_name(
		ctx->master_instruments, class_name, method_sig
	);
	if (found.entry == NULL) {
		LOG_WARN(
			"deinstrument: %s not found in %s",
			method_sig, class_name
		);
		return DEINSTRUMENT_RP_FAIL;
	}
	return deinstrument_method_by_id(
		ctx, jni_env, jvmti, found.entry, found.id
	);
}

enum deinstrument_resp_status ih_deinstrument_by_id(
	struct instr_ctx *ctx,
	JNIEnv *jni_env,
	jvmtiEnv *jvmti,
	uint64_t instrument_id
) {
	const struct mi_entry *entry = mi_find(
		ctx->master_instruments, instrument_id
	);
	if (entry == NULL) {
		return DEINSTRUMENT_RP_FAIL;
	}
	if (entry->type == MI_LINE) {
		return deinstrument_line_by_id(
			ctx, jni_env, jvmti, entry, instrument_id
		);
	} else {
		return deinstrument_method_by_id(
			ctx, jni_env, jvmti, entry, instrument_id
		);
	}
}

void ih_apply_deferred_instrumentations(
	struct instr_ctx *ctx,
	JNIEnv *jni_env,
	jvmtiEnv *jvmti,
	struct prof_err_log *err_log,
	struct class_info *ci
) {
	do_deferred_instrumentations(ctx, jni_env, jvmti, err_log, ci);
}
