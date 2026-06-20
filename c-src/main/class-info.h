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
#ifndef _CLASS_INFO_H
#define _CLASS_INFO_H

#include "util/dyn-arr.h"
#include "util/pstring.h"

#include <stddef.h>
#include <stdint.h>


DYNARR_STRUCT(method_list, struct pstring *)

int method_list_init(struct method_list *arr, size_t init_cap);
struct pstring **method_list_add(struct method_list *arr);
void method_list_remove(struct method_list *arr, int index);
void method_list_deep_destroy(struct method_list *arr);

struct instrumented_method {
	struct pstring *method_sig;
	int profiler_id;
	uint64_t instrument_id;
};

DYNARR_STRUCT(instrumented_method_list, struct instrumented_method)

int instrumented_method_list_init(
	struct instrumented_method_list *arr, size_t init_cap
);
struct instrumented_method *instrumented_method_list_add_and_init(
	struct instrumented_method_list *arr, const char *method_sig, int id
);
void instrumented_method_list_remove_and_destroy(
	struct instrumented_method_list *im, int index
);
void instrumented_method_list_deep_destroy(
	struct instrumented_method_list *arr
);

enum instrument_type {
	INSTRUMENT_ENTER = 0,
	INSTRUMENT_EXIT = 1
};

struct instrumented_line {
	int line_number;
	int profiler_id;
	enum instrument_type type;
};

DYNARR_STRUCT(instrumented_lines, struct instrumented_line)

struct class_info {
	struct pstring *name;
	unsigned char *bytecode;
	size_t bytecode_len;
	struct method_list methods;
	struct instrumented_method_list instrumented;
	struct instrumented_lines lines;
};

struct class_info *ci_alloc(
	char *name,
	const unsigned char *bytecode,
	size_t bytecode_len,
	struct method_list *methods
);
void ci_free(struct class_info *ci);
int ci_remove_instrumented_by_sig(
	struct class_info *ci,
	const char *method_sig,
	uint64_t *instrument_id_out
);
struct instrumented_line *ci_add_instrumented_line(
	struct class_info *ci,
	int line_number,
	int profiler_id,
	enum instrument_type type
);
/* Returns profiler_id on success, -1 if not found. */
int ci_remove_instrumented_line(struct class_info *ci, int line_number);

struct class_instrument_params {
	const unsigned char *class_data;
	size_t class_data_len;
	const char **method_sigs;
	int *profiler_ids;
	int count;
	int *line_numbers;
	int *line_types;
	int *line_profiler_ids;
	int line_count;
};

struct class_instrument_params *class_instrument_params_alloc(
	const struct class_info *ci
);
void class_instrument_params_free(struct class_instrument_params *params);

#endif /* _CLASS_INFO_H */
