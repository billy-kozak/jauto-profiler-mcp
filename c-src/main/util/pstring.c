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

#include "pstring.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PSTRING_SPRINTF_INIT_CAP 512

struct pstring *pstring_dup(const struct pstring *ps)
{
	struct pstring *copy = malloc(sizeof(*ps) + ps->size + 1);

	if (copy == NULL) {
		return NULL;
	}
	copy->size = ps->size;
	memcpy(copy->str, ps->str, ps->size);
	copy->str[ps->size] = '\0';
	return copy;
}

struct pstring *pstring_from_cstr(const char *str) {
	size_t str_len = strlen(str);

	if(str_len > PSTR_MAX) {
		return NULL;
	}

	/* Include terminating null so we can still run regular cstring
	 * functions on the str */
	struct pstring *ps = malloc(sizeof(*ps) + str_len + 1);

	if(ps == NULL) {
		return NULL;
	}

	ps->size = str_len;
	memcpy(ps->str, str, str_len + 1);

	return ps;
}

struct pstring *pstring_vsprintf(const char *fmt, va_list args)
{
	struct pstring *ps;
	va_list args_copy;
	int written;

	ps = malloc(sizeof(*ps) + PSTRING_SPRINTF_INIT_CAP);
	if (ps == NULL) {
		return NULL;
	}

	va_copy(args_copy, args);
	written = vsnprintf(
		(char *)ps->str, PSTRING_SPRINTF_INIT_CAP, fmt, args_copy
	);
	va_end(args_copy);

	if (written < 0) {
		free(ps);
		return NULL;
	}

	size_t written_size = (size_t)written;

	if (written_size >= PSTRING_SPRINTF_INIT_CAP) {
		size_t needed = written_size <= PSTR_MAX ?
			written_size : PSTR_MAX;

		struct pstring *new_ps = realloc(ps, sizeof(*ps) + needed + 1);
		if (new_ps == NULL) {
			free(ps);
			return NULL;
		}
		ps = new_ps;

		va_copy(args_copy, args);
		vsnprintf((char *)ps->str, needed + 1, fmt, args_copy);
		va_end(args_copy);

		ps->size = needed;
	} else {
		ps->size = (uint16_t)written_size;
	}

	return ps;
}

struct pstring *pstring_sprintf(const char *fmt, ...)
{
	va_list args;
	struct pstring *ps;

	va_start(args, fmt);
	ps = pstring_vsprintf(fmt, args);
	va_end(args);

	return ps;
}
