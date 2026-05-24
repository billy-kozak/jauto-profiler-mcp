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

#define DEFAULT_INIT_CAPACITY 16

int dynarr_init(
	struct dynarr_head *head,
	size_t elem_size,
	size_t capacity,
	void **data
) {
	head->capacity = capacity;
	head->len = 0;
	head->elem_size = elem_size;

	*data = calloc(capacity, elem_size);

	if(*data == NULL) {
		return 1;
	} else {
		return 0;
	}
}

int dynarr_grow(struct dynarr_head *head, void **data) {

	head->len += 1;

	if(head->capacity >= head->len) {
		return 0;
	}

	size_t new_cap = head->capacity != 0 ?
		head->capacity * 2 : DEFAULT_INIT_CAPACITY;

	void *new = realloc(*data, new_cap * head->elem_size);
	if(new == NULL) {
		return 1;
	}

	head->capacity = new_cap;
	*data = new;
	return 0;
}

void dynarr_remove(struct dynarr_head *head, void *data, int index) {

	if(index == (head->len - 1)) {
		head->len -= 1;
		return;
	}

	assert(index >= 0 && index < head->len);

	uint8_t *dst = ((uint8_t*)data) + (index * head->elem_size);
	uint8_t *src = dst + head->elem_size;

	size_t move_len = (head->len - (index + 1)) * head->elem_size;

	memmove(dst, src, move_len);

	head->len -= 1;
}

void dynarr_destroy(struct dynarr_head *head, void *data) {
	head->len = 0;
	head->capacity = 0;
	free(data);
}
