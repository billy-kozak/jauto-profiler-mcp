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

int dynarr_alloc(size_t capacity, size_t elem_size, void **data);
int dynarr_grow(size_t cap, size_t elem_size, void **data);
void dynarr_remove(void *data, int index, size_t len, size_t elem_size);

#define DYNARR_DEFAULT_INIT_CAPACITY 16

#define DYNARR_TEMPLATE(prefix, name, typ) \
	struct name { \
		size_t len; \
		size_t capacity; \
		typ *arr; \
	}; \
	int prefix##_init(struct name *arr, size_t init_cap) { \
		arr->len = 0; \
		if(init_cap <= 0) { \
			arr->capacity = DYNARR_DEFAULT_INIT_CAPACITY; \
		} else { \
			arr->capacity = init_cap; \
		} \
		return dynarr_alloc( \
			arr->capacity, sizeof(typ), (void**)&arr->arr \
		); \
	} \
	typ * prefix##_add(struct name *arr) { \
		if(arr->len >= arr->capacity) { \
			size_t new_cap = arr->capacity * 2; \
			int ret = dynarr_grow( \
				new_cap, sizeof(typ), (void**)&arr->arr \
			); \
			if(ret != 0) { \
				return NULL; \
			} \
		} \
		arr->len += 1; \
		return arr->arr + arr->len - 1; \
	} \
	void prefix##_remove(struct name *arr, int index) { \
		dynarr_remove( \
			arr->arr, index, arr->len, sizeof(*(arr->arr)) \
		); \
		arr->len -= 1; \
	} \
	void prefix##_destroy(struct name *arr) { \
		arr->len = 0; \
		arr->capacity = 0; \
		free(arr->arr); \
		arr->arr = NULL; \
	}

#endif /* _DYN_ARR_H */
