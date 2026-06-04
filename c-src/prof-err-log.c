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

#include "prof-err-log.h"

#include "util/pstring.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>

static int prof_err_log_start(const struct prof_err_log *log)
{
	return (log->next + PROF_ERR_LOG_CAP - log->count) % PROF_ERR_LOG_CAP;
}

void prof_err_log_init(struct prof_err_log *log)
{
	size_t i;

	log->next  = 0;
	log->count = 0;

	for (i = 0; i < PROF_ERR_LOG_CAP; i++) {
		log->entries[i].msg = NULL;
	}
}

void prof_err_log_destroy(struct prof_err_log *log)
{
	size_t i;

	for (i = 0; i < PROF_ERR_LOG_CAP; i++) {
		free(log->entries[i].msg);
	}
}

static void do_push(struct prof_err_log *log, struct pstring *msg)
{
	struct prof_err_entry *entry = &log->entries[log->next];

	free(entry->msg);
	entry->timestamp = (int64_t)time(NULL);
	entry->msg = msg;

	log->next = (log->next + 1) % PROF_ERR_LOG_CAP;
	if (log->count < PROF_ERR_LOG_CAP) {
		log->count++;
	}
}

void prof_err_log_push(struct prof_err_log *log, const char *msg)
{
	struct pstring *ps = pstring_from_cstr(msg);

	if (ps == NULL) {
		return;
	}
	do_push(log, ps);
}

void prof_err_log_printf(struct prof_err_log *log, const char *fmt, ...)
{
	va_list args;
	struct pstring *ps;

	va_start(args, fmt);
	ps = pstring_vsprintf(fmt, args);
	va_end(args);

	if (ps == NULL) {
		return;
	}
	do_push(log, ps);
}

struct prof_err_log_size prof_err_log_size(const struct prof_err_log *log)
{
	struct prof_err_log_size ret = {0};
	int start = prof_err_log_start(log);

	for(int i = 0; i < log->count; i++) {
		int idx = (start + i) % PROF_ERR_LOG_CAP;
		const struct prof_err_entry *e = &log->entries[idx];

		ret.count += 1;
		ret.string_size += e->msg->size;

		ret.overhead_size += pstring_total_size(e->msg);
		ret.overhead_size += sizeof(e->timestamp);
	}

	return ret;
}

const struct prof_err_entry *prof_err_log_iterate(
	struct prof_err_log_itr *itr,
	const struct prof_err_log *log
) {
	int start = prof_err_log_start(log);
	int idx = (start + itr->i) % PROF_ERR_LOG_CAP;

	if(itr->i < log->count) {
		itr->i += 1;
		return &log->entries[idx];
	} else {
		return NULL;
	}
}
