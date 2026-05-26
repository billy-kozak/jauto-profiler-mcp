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

#include "dyn-arr.h"

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
	im->method_sig = strdup(method_sig);
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
	ci->name = strdup(name);
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

void ci_free(struct class_info *ci)
{
	free(ci->name);
	free(ci->bytecode);
	method_list_deep_destroy(&ci->methods);
	instrumented_method_list_deep_destroy(&ci->instrumented);
	free(ci);
}
