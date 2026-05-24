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

#include "dyn-arr.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

int dynarr_alloc(
	size_t capacity,
	size_t elem_size,
	void **data
) {
	*data = calloc(capacity, elem_size);

	if(*data == NULL) {
		return 1;
	} else {
		return 0;
	}
}

int dynarr_grow(size_t new_cap, size_t elem_size, void **data) {
	void *new = realloc(*data, new_cap * elem_size);

	if(new == NULL) {
		return 1;
	}

	*data = new;
	return 0;
}

void dynarr_remove(void *data, int index, size_t len, size_t elem_size) {

	if(index == len - 1) {
		return;
	}

	assert(index >= 0 && index < len);

	uint8_t *dst = ((uint8_t*)data) + (index * elem_size);
	uint8_t *src = dst + elem_size;

	size_t move_len = (len - (index + 1)) * elem_size;

	memmove(dst, src, move_len);
}
