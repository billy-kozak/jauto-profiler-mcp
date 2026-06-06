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

void instrumented_method_init(
        struct instrumented_method *im, const char *method_sig, int id
) {
	im->method_sig = pstring_from_cstr(method_sig);
	im->profiler_id = id;
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
	return ci;

fail_bytecode:
	free(ci->bytecode);
fail_name:
	free(ci->name);
fail:
	free(ci);
	return NULL;
}

int ci_remove_instrumented_by_sig(
	struct class_info *ci, const char *method_sig
) {
	struct instrumented_method_list *list = &ci->instrumented;
	size_t i;

	for (i = 0; i < list->len; i++) {
		if (strcmp((char *)list->arr[i].method_sig->str, method_sig) == 0) {
			int id = list->arr[i].profiler_id;
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
	free(ci);
}

struct class_instrument_params *class_instrument_params_alloc(
	const struct class_info *ci
) {
	struct class_instrument_params *p;
	int count = (int)ci->instrumented.len;
	size_t total_buf = 0;
	char *buf;
	size_t i;

	p = malloc(sizeof(*p));
	if (p == NULL) {
		return NULL;
	}

	p->class_data = ci->bytecode;
	p->class_data_len = ci->bytecode_len;
	p->count = count;
	p->method_names = NULL;
	p->method_descs = NULL;
	p->profiler_ids = NULL;
	p->name_buf = NULL;

	p->method_names = malloc((size_t)count * sizeof(*p->method_names));
	p->method_descs = malloc((size_t)count * sizeof(*p->method_descs));
	p->profiler_ids = malloc((size_t)count * sizeof(*p->profiler_ids));

	bool alloc_failed = (
		p->method_names == NULL ||
		p->method_descs == NULL ||
		p->profiler_ids == NULL
	);
	if (alloc_failed) {
		goto fail;
	}

	for (i = 0; i < (size_t)count; i++) {
		const char *sig = (char *)ci->instrumented.arr[i].method_sig->str;
		const char *colon = strchr(sig, ':');
		if (colon == NULL) {
			LOG_ERROR("malformed method_sig in instrumented list");
			goto fail;
		}
		total_buf += (size_t)(colon - sig) + 1;
		total_buf += strlen(colon + 1) + 1;
	}

	buf = malloc(total_buf);
	if (buf == NULL) {
		goto fail;
	}
	p->name_buf = buf;

	for (i = 0; i < (size_t)count; i++) {
		const char *sig = (char *)ci->instrumented.arr[i].method_sig->str;
		const char *colon = strchr(sig, ':');
		size_t name_len = (size_t)(colon - sig);
		size_t desc_len = strlen(colon + 1);

		memcpy(buf, sig, name_len);
		buf[name_len] = '\0';
		p->method_names[i] = buf;
		buf += name_len + 1;

		memcpy(buf, colon + 1, desc_len);
		buf[desc_len] = '\0';
		p->method_descs[i] = buf;
		buf += desc_len + 1;

		p->profiler_ids[i] = ci->instrumented.arr[i].profiler_id;
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
	free(params->name_buf);
	free(params->method_names);
	free(params->method_descs);
	free(params->profiler_ids);
	free(params);
}
