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

#include <string.h>
#include <stdlib.h>

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
