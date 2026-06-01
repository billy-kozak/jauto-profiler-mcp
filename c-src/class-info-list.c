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

#include "class-info-list.h"

#include "util/dyn-arr.h"

#include <stddef.h>
#include <string.h>

DYNARR_FUNCS(class_info_list, class_info_list, struct class_info *)

int class_info_list_append(
	struct class_info_list *list, struct class_info *ci
) {
	struct class_info **slot = class_info_list_add(list);

	if (slot == NULL) {
		return -1;
	}
	*slot = ci;
	return 0;
}

void class_info_list_deep_destroy(struct class_info_list *list)
{
	size_t i;

	for (i = 0; i < list->len; i++) {
		ci_free(list->arr[i]);
	}
	class_info_list_destroy(list);
}

struct class_info *class_info_list_find_by_name(
	const struct class_info_list *list, const char *name
) {
	size_t i;

	for (i = 0; i < list->len; i++) {
		if (strcmp(list->arr[i]->name, name) == 0) {
			return list->arr[i];
		}
	}
	return NULL;
}

int ci_list_init(struct class_info_list *list, size_t init_cap)
{
	return class_info_list_init(list, init_cap);
}

int ci_list_add(struct class_info_list *list, struct class_info *ci)
{
	return class_info_list_append(list, ci);
}

void ci_list_deep_destroy(struct class_info_list *list)
{
	class_info_list_deep_destroy(list);
}

struct class_info *ci_list_find_by_name(
	const struct class_info_list *list, const char *name
) {
	return class_info_list_find_by_name(list, name);
}
