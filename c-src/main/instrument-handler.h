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
#ifndef _INSTRUMENT_HANDLER_H
#define _INSTRUMENT_HANDLER_H

#include "class-info.h"
#include "class-info-list.h"
#include "queued-instrument.h"
#include "master-instruments.h"
#include "prof-err-log.h"
#include "user-if.h"
#include "util/pstring.h"

#include <jvmti.h>
#include <jni.h>
#include <stddef.h>
#include <stdint.h>

struct pending_instrument {
	volatile const char          *class_name;
	volatile const char          *method_sig;
	volatile const unsigned char *bytecode;
	volatile size_t               bytecode_len;
	volatile int                  profiler_id;
};

struct instr_ctx {
	struct class_info_list     loaded_classes;
	struct queued_instr_list   queued_instruments;
	struct master_instruments *master_instruments;
	struct pending_instrument  pending;
};

int  ih_ctx_init(struct instr_ctx *ctx);
void ih_ctx_destroy(struct instr_ctx *ctx);

enum instrument_resp_status ih_instrument_method(
	struct instr_ctx *ctx,
	JNIEnv *jni_env,
	jvmtiEnv *jvmti,
	const char *class_name,
	const char *method_sig,
	uint64_t *instrument_id_out
);

enum instrument_line_resp_status ih_instrument_line(
	struct instr_ctx *ctx,
	JNIEnv *jni_env,
	jvmtiEnv *jvmti,
	const struct pstring *entry_class,
	const struct pstring *exit_class,
	uint32_t entry_line,
	uint32_t exit_line,
	uint64_t *instrument_id_out
);

enum deinstrument_resp_status ih_deinstrument_method(
	struct instr_ctx *ctx,
	JNIEnv *jni_env,
	jvmtiEnv *jvmti,
	const char *class_name,
	const char *method_sig
);

enum deinstrument_resp_status ih_deinstrument_by_id(
	struct instr_ctx *ctx,
	JNIEnv *jni_env,
	jvmtiEnv *jvmti,
	uint64_t instrument_id
);

void ih_apply_deferred_instrumentations(
	struct instr_ctx *ctx,
	JNIEnv *jni_env,
	jvmtiEnv *jvmti,
	struct prof_err_log *err_log,
	struct class_info *ci
);

#endif /* _INSTRUMENT_HANDLER_H */
