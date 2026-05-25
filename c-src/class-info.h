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

#include "dyn-arr.h"
#include "pstring.h"

#include <stddef.h>

DYNARR_STRUCT(method_list, struct pstring *)

int method_list_init(struct method_list *arr, size_t init_cap);
struct pstring **method_list_add(struct method_list *arr);
void method_list_remove(struct method_list *arr, int index);
void method_list_deep_destroy(struct method_list *arr);

struct class_info {
	char *name;
	unsigned char *bytecode;
	size_t bytecode_len;
	struct method_list methods;
};

struct class_info *ci_alloc(
	char *name,
	const unsigned char *bytecode,
	size_t bytecode_len,
	struct method_list *methods
);
void ci_free(struct class_info *ci);

#endif /* _CLASS_INFO_H */
