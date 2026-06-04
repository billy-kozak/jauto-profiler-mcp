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
#ifndef _QUEUED_INSTRUMENT_H
#define _QUEUED_INSTRUMENT_H

#include "util/dyn-arr.h"

#include <stddef.h>

struct queued_instrument {
	char *class_name;
	char *method_sig;
};

DYNARR_STRUCT(queued_instrument_list, struct queued_instrument)

int queued_instrument_list_init(
	struct queued_instrument_list *arr, size_t init_cap
);
struct queued_instrument *queued_instrument_list_add_and_init(
	struct queued_instrument_list *arr,
	const char *class_name,
	const char *method_sig
);
void queued_instrument_list_remove_and_destroy(
	struct queued_instrument_list *arr, int index
);
void queued_instrument_list_deep_destroy(
	struct queued_instrument_list *arr
);
int queued_instrument_list_find_by_class(
	const struct queued_instrument_list *arr, const char *class_name
);

#endif /* _QUEUED_INSTRUMENT_H */
