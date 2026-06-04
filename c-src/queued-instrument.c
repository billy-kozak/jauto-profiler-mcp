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

#include "queued-instrument.h"

#include "util/dyn-arr.h"

#include <stdlib.h>
#include <string.h>

DYNARR_FUNCS(
	queued_instrument_list,
	queued_instrument_list,
	struct queued_instrument
)

static void queued_instrument_init(
	struct queued_instrument *qi,
	const char *class_name,
	const char *method_sig
) {
	qi->class_name = strdup(class_name);
	qi->method_sig = strdup(method_sig);
}

static void queued_instrument_destroy(struct queued_instrument *qi)
{
	free(qi->class_name);
	free(qi->method_sig);
}

struct queued_instrument *queued_instrument_list_add_and_init(
	struct queued_instrument_list *arr,
	const char *class_name,
	const char *method_sig
) {
	struct queued_instrument *qi = queued_instrument_list_add(arr);

	if (qi != NULL) {
		queued_instrument_init(qi, class_name, method_sig);
	}

	return qi;
}

void queued_instrument_list_remove_and_destroy(
	struct queued_instrument_list *arr, int index
) {
	queued_instrument_destroy(arr->arr + index);
	queued_instrument_list_remove(arr, index);
}

void queued_instrument_list_deep_destroy(struct queued_instrument_list *arr)
{
	size_t i;

	for (i = 0; i < arr->len; i++) {
		queued_instrument_destroy(arr->arr + i);
	}
	queued_instrument_list_destroy(arr);
}

int queued_instrument_list_find_by_class(
	const struct queued_instrument_list *arr, const char *class_name
) {
	size_t i;

	for (i = 0; i < arr->len; i++) {
		if (strcmp(arr->arr[i].class_name, class_name) == 0) {
			return (int)i;
		}
	}
	return -1;
}
