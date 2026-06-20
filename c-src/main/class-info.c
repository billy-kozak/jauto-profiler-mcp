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

#include "class-info.h"

#include "util/dyn-arr.h"
#include "util/log.h"
#include "util/pstring.h"

#define LOG_TAG "class-info"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

DYNARR_FUNCS(method_list, method_list, struct pstring *)


void method_list_deep_destroy(struct method_list *arr)
{
	size_t i;

	for (i = 0; i < arr->len; i++) {
		free(arr->arr[i]);
	}
	method_list_destroy(arr);
}

DYNARR_FUNCS(
	instrumented_method_list,
	instrumented_method_list,
	struct instrumented_method
)

DYNARR_FUNCS(instrumented_lines, instrumented_lines, struct instrumented_line)

void instrumented_method_init(
        struct instrumented_method *im, const char *method_sig, int id
) {
	im->method_sig = pstring_from_cstr(method_sig);
	im->profiler_id = id;
	im->instrument_id = 0;
}

void instrumented_method_destroy(struct instrumented_method *im) {
    free(im->method_sig);
}

struct instrumented_method *instrumented_method_list_add_and_init(
	struct instrumented_method_list *arr, const char *method_sig, int id
) {
    struct instrumented_method *m = instrumented_method_list_add(arr);

    if(m != NULL) {
        instrumented_method_init(m, method_sig, id);
    }

    return m;
}

void instrumented_method_list_deep_destroy(struct instrumented_method_list *arr)
{
	size_t i;

	for (i = 0; i < arr->len; i++) {
        instrumented_method_destroy(arr->arr + i);
	}
	instrumented_method_list_destroy(arr);
}

void instrumented_method_list_remove_and_destroy(
	struct instrumented_method_list *im, int index
) {
    struct instrumented_method *m = im->arr + index;
    instrumented_method_destroy(m);

    instrumented_method_list_remove(im, index);
}

struct class_info *ci_alloc(
	char *name,
	const unsigned char *bytecode,
	size_t bytecode_len,
	struct method_list *methods
) {
	struct class_info *ci = malloc(sizeof(*ci));

	if (ci == NULL) {
		return NULL;
	}
	ci->name = pstring_from_cstr(name);
	if (ci->name == NULL) {
		goto fail;
	}
	ci->bytecode = malloc(bytecode_len);
	if (ci->bytecode == NULL) {
		goto fail_name;
	}
	memcpy(ci->bytecode, bytecode, bytecode_len);
	ci->bytecode_len = bytecode_len;
	ci->methods = *methods;
	if (instrumented_method_list_init(&ci->instrumented, 1) != 0) {
		goto fail_bytecode;
	}
	if (instrumented_lines_init(&ci->lines, 1) != 0) {
		goto fail_instrumented;
	}
	return ci;

fail_instrumented:
	instrumented_method_list_deep_destroy(&ci->instrumented);

fail_bytecode:
	free(ci->bytecode);
fail_name:
	free(ci->name);
fail:
	free(ci);
	return NULL;
}

int ci_remove_instrumented_by_sig(
	struct class_info *ci,
	const char *method_sig,
	uint64_t *instrument_id_out
) {
	struct instrumented_method_list *list = &ci->instrumented;
	size_t i;

	for (i = 0; i < list->len; i++) {
		if (strcmp((char *)list->arr[i].method_sig->str, method_sig) == 0) {
			int id = list->arr[i].profiler_id;
			*instrument_id_out = list->arr[i].instrument_id;
			instrumented_method_list_remove_and_destroy(list, (int)i);
			return id;
		}
	}
	return -1;
}

void ci_free(struct class_info *ci)
{
	free(ci->name);
	free(ci->bytecode);
	method_list_deep_destroy(&ci->methods);
	instrumented_method_list_deep_destroy(&ci->instrumented);
	instrumented_lines_destroy(&ci->lines);
	free(ci);
}

struct instrumented_line *ci_add_instrumented_line(
	struct class_info *ci,
	int line_number,
	int profiler_id,
	enum instrument_type type
) {
	struct instrumented_line *line = instrumented_lines_add(&ci->lines);
	if (line != NULL) {
		line->line_number = line_number;
		line->profiler_id = profiler_id;
		line->type = type;
	}
	return line;
}

int ci_remove_instrumented_line(struct class_info *ci, int line_number)
{
	struct instrumented_lines *lines = &ci->lines;
	size_t i;

	for (i = 0; i < lines->len; i++) {
		if (lines->arr[i].line_number == line_number) {
			int profiler_id = lines->arr[i].profiler_id;
			instrumented_lines_remove(lines, (int)i);
			return profiler_id;
		}
	}
	return -1;
}

struct class_instrument_params *class_instrument_params_alloc(
	const struct class_info *ci
) {
	struct class_instrument_params *p;
	int count = (int)ci->instrumented.len;
	size_t i;

	p = malloc(sizeof(*p));
	if (p == NULL) {
		return NULL;
	}

	int line_count = (int)ci->lines.len;

	p->class_data = ci->bytecode;
	p->class_data_len = ci->bytecode_len;
	p->count = count;
	p->line_count = line_count;
	p->method_sigs = NULL;
	p->profiler_ids = NULL;
	p->line_numbers = NULL;
	p->line_types = NULL;
	p->line_profiler_ids = NULL;

	p->method_sigs = malloc((size_t)count * sizeof(*p->method_sigs));
	p->profiler_ids = malloc((size_t)count * sizeof(*p->profiler_ids));

	if (p->method_sigs == NULL || p->profiler_ids == NULL) {
		goto fail;
	}

	for (i = 0; i < (size_t)count; i++) {
		p->method_sigs[i] = (char *)ci->instrumented.arr[i].method_sig->str;
		p->profiler_ids[i] = ci->instrumented.arr[i].profiler_id;
	}

	if (line_count > 0) {
		p->line_numbers = malloc((size_t)line_count * sizeof(*p->line_numbers));
		p->line_types = malloc((size_t)line_count * sizeof(*p->line_types));
		p->line_profiler_ids = malloc(
			(size_t)line_count * sizeof(*p->line_profiler_ids)
		);
		if (
			p->line_numbers == NULL ||
			p->line_types == NULL ||
			p->line_profiler_ids == NULL
		) {
			goto fail;
		}
		for (i = 0; i < (size_t)line_count; i++) {
			p->line_numbers[i] = ci->lines.arr[i].line_number;
			p->line_types[i] = (int)ci->lines.arr[i].type;
			p->line_profiler_ids[i] = ci->lines.arr[i].profiler_id;
		}
	}

	return p;

fail:
	class_instrument_params_free(p);
	return NULL;
}

void class_instrument_params_free(struct class_instrument_params *params)
{
	if (params == NULL) {
		return;
	}
	free(params->method_sigs);
	free(params->profiler_ids);
	free(params->line_numbers);
	free(params->line_types);
	free(params->line_profiler_ids);
	free(params);
}
