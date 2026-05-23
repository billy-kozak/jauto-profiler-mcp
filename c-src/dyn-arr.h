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
#ifndef _DYN_ARR_H
#define _DYN_ARR_H

#include <stdlib.h>

struct dynarr_head {
	size_t len;
	size_t capacity;
	size_t elem_size;
};

int dynarr_init(
	struct dynarr_head *head,
	size_t elem_size,
	size_t capacity,
	void **data
);

int dynarr_grow(struct dynarr_head *head, void **data);
void dynarr_remove(struct dynarr_head *head, void *data, int index);
void dynarr_destroy(struct dynarr_head *head, void *data);

#define DYNARR_TEMPLATE(typ) struct { struct dynarr_head head; typ *arr; }
#define DYNARR_INIT(s, cap) dynarr_init( \
		&s.head, sizeof(*s.arr), cap, (void**)&(s.arr) \
)
#define DYNARR_LEN(s) (s.head.len)
#define DYNARR_REMOVE(s, i) dynarr_remove(&s.head, s.arr, i)
#define DYNARR_DESTROY(s) dynarr_destroy(&s.head, s.arr)
#define DYNARR_GROW(s) dynarr_grow(&s.head, (void**)&(s.arr))
#define DYNARR_AT(s, i) (s.arr[i])

#endif /* _DYN_ARR_H */
