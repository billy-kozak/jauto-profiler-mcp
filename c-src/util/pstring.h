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
#ifndef _PSTRING_H
#define _PSTRING_H

#include <stdint.h>
#include <stddef.h>

struct pstring {
	uint16_t size;
	unsigned char str[];
};

#define PSTR_MAX ((1 << (sizeof(((struct pstring*)NULL)->size) * 8)) -1)

static inline size_t pstring_total_size(const struct pstring *ps) {
	return sizeof(*ps) + ps->size;
}

struct pstring *pstring_from_cstr(const char *str);

#endif /* _PSTRING_H */
