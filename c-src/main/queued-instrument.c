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
#include "util/pstring.h"

#include <stdlib.h>
#include <string.h>

DYNARR_FUNCS(
	queued_instr_list,
	queued_instr_list,
	struct queued_instr
)

static void queued_instr_method_init(
	struct queued_instr *qi,
	const char *class_name,
	const char *method_sig,
	int profiler_id,
	uint64_t instrument_id
) {
	qi->type = QI_METHOD;
	qi->instrument_id = instrument_id;
	qi->profiler_id = profiler_id;
	qi->class_name = pstring_from_cstr(class_name);
	qi->method_sig = pstring_from_cstr(method_sig);
}

static void queued_instr_destroy(struct queued_instr *qi)
{
	free(qi->class_name);
	if (qi->type == QI_METHOD) {
		free(qi->method_sig);
	}
}

struct queued_instr *queued_instr_method_add(
	struct queued_instr_list *arr,
	const char *class_name,
	const char *method_sig,
	int profiler_id,
	uint64_t instrument_id
) {
	struct queued_instr *qi = queued_instr_list_add(arr);

	if (qi != NULL) {
		queued_instr_method_init(
			qi, class_name, method_sig, profiler_id, instrument_id
		);
	}

	return qi;
}

void queued_instr_list_remove_and_destroy(
	struct queued_instr_list *arr, int index
) {
	queued_instr_destroy(arr->arr + index);
	queued_instr_list_remove(arr, index);
}

void queued_instr_list_deep_destroy(struct queued_instr_list *arr)
{
	size_t i;

	for (i = 0; i < arr->len; i++) {
		queued_instr_destroy(arr->arr + i);
	}
	queued_instr_list_destroy(arr);
}

int queued_instr_list_find_by_class(
	const struct queued_instr_list *arr, const char *class_name
) {
	size_t i;

	for (i = 0; i < arr->len; i++) {
		if (strcmp((char *)arr->arr[i].class_name->str, class_name) == 0) {
			return (int)i;
		}
	}
	return -1;
}

int queued_instr_list_find_by_instrument_id(
	const struct queued_instr_list *arr, uint64_t instrument_id
) {
	size_t i;

	for (i = 0; i < arr->len; i++) {
		if (arr->arr[i].instrument_id == instrument_id) {
			return (int)i;
		}
	}
	return -1;
}
