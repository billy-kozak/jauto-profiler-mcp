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
#ifndef _PROF_ERR_LOG_H
#define _PROF_ERR_LOG_H

#include "util/pstring.h"

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#define PROF_ERR_LOG_CAP 4096
#define PROF_ERR_LOG_ITR_INIT {0}

struct prof_err_entry {
	int64_t timestamp;
	struct pstring *msg;
};

struct prof_err_log_size {
	size_t string_size;
	size_t count;
	size_t overhead_size;
};

struct prof_err_log {
	struct prof_err_entry entries[PROF_ERR_LOG_CAP];
	size_t next;
	size_t count;
};

struct prof_err_log_itr {
	int i;
};

struct prof_err_log_size prof_err_log_size(const struct prof_err_log *log);
const struct prof_err_entry *prof_err_log_iterate(
	struct prof_err_log_itr *itr,
	const struct prof_err_log *log
);
void prof_err_log_init(struct prof_err_log *log);
void prof_err_log_destroy(struct prof_err_log *log);
void prof_err_log_push(struct prof_err_log *log, const char *msg);
void prof_err_log_printf(struct prof_err_log *log, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

#endif /* _PROF_ERR_LOG_H */
