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
#ifndef _MASTER_INSTRUMENTS_H
#define _MASTER_INSTRUMENTS_H

#include "util/pstring.h"

#include <stdint.h>
#include <stdbool.h>

enum mi_type {
	MI_METHOD,
	MI_LINE
};

struct master_instruments;

struct mi_entry {
	enum mi_type type;
	struct pstring *entry_class_name;
	union {
		struct {
			struct pstring *method_name;
			bool deferred;
		} method;
		struct {
			struct pstring *exit_class_name;
			int entry_line_number;
			int exit_line_number;
			bool entry_deferred;
			bool exit_deferred;
		} line;
	};
};

struct mi_itr {
	void *_next;
};

struct mi_val {
	uint64_t id;
	const struct mi_entry *entry;
};

static inline bool mi_line_is_ready(const struct mi_entry *ent)
{
	return !ent->line.entry_deferred && !ent->line.exit_deferred;
}

struct master_instruments *mi_alloc(void);
void mi_free(struct master_instruments *mi);

struct mi_val mi_add_method(
	struct master_instruments *mi,
	const char *entry_class,
	const char *exit_class,
	const char *method_name
);
struct mi_val mi_add_line(
	struct master_instruments *mi,
	const char *entry_class,
	const char *exit_class,
	int entry_line,
	int exit_line
);
struct mi_val mi_add_linep(
	struct master_instruments *mi,
	const struct pstring *entry_class,
	const struct pstring *exit_class,
	int entry_line,
	int exit_line
);

void mi_remove(struct master_instruments *mi, uint64_t id);

int mi_mark_method_applied(struct master_instruments *mi, uint64_t id);
int mi_mark_line_entry_applied(struct master_instruments *mi, uint64_t id);
int mi_mark_line_exit_applied(struct master_instruments *mi, uint64_t id);

const struct mi_entry *mi_find(
	const struct master_instruments *mi, uint64_t id
);

struct mi_val mi_find_method_by_name(
	const struct master_instruments *mi,
	const char *class_name,
	const char *method_sig
);

void mi_itr_init(const struct master_instruments *mi, struct mi_itr *itr);
struct mi_val mi_iterate(
	const struct master_instruments *mi, struct mi_itr *itr
);

#endif /* _MASTER_INSTRUMENTS_H */
