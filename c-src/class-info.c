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
	return ci;

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
	free(ci);
}
