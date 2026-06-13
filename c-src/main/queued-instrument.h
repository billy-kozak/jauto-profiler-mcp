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
#ifndef _QUEUED_INSTR_H
#define _QUEUED_INSTR_H

#include "util/dyn-arr.h"
#include "util/pstring.h"

#include <stddef.h>
#include <stdint.h>

enum qi_type {
	QI_METHOD,
	QI_LINE_ENTER,
	QI_LINE_EXIT
};

struct queued_instr {
	enum qi_type type;
	uint64_t instrument_id;
	int profiler_id;
	struct pstring *class_name;
	union {
		struct pstring *method_sig;
		int line_number;
	};
};

DYNARR_STRUCT(queued_instr_list, struct queued_instr)

int queued_instr_list_init(
	struct queued_instr_list *arr, size_t init_cap
);
struct queued_instr *queued_instr_method_add(
	struct queued_instr_list *arr,
	const char *class_name,
	const char *method_sig
);
void queued_instr_list_remove_and_destroy(
	struct queued_instr_list *arr, int index
);
void queued_instr_list_deep_destroy(
	struct queued_instr_list *arr
);
int queued_instr_list_find_by_class(
	const struct queued_instr_list *arr, const char *class_name
);
int queued_instr_list_find_by_instrument_id(
	const struct queued_instr_list *arr, uint64_t instrument_id
);

#endif /* _QUEUED_INSTR_H */
