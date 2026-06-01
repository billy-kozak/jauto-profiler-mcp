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
#ifndef _CLASS_INFO_LIST_H
#define _CLASS_INFO_LIST_H

#include "class-info.h"
#include "util/dyn-arr.h"

#include <stddef.h>

DYNARR_STRUCT(class_info_list, struct class_info *)

int ci_list_init(struct class_info_list *list, size_t init_cap);
int ci_list_add(struct class_info_list *list, struct class_info *ci);
void ci_list_deep_destroy(struct class_info_list *list);
struct class_info *ci_list_find_by_name(
	const struct class_info_list *list, const char *name
);

#endif /* _CLASS_INFO_LIST_H */
