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
#ifndef _HASH_TAB_H
#define _HASH_TAB_H

#include <stdlib.h>
#include <stdint.h>

#define HASHTAB_CAP_DEFAULT 32
#define HASHTAB_THRESH_DEFAULT 0.66f
#define HASHTAB_SEED_DEFAULT 560392091UL
#define HASHTAB_OPTS_DEFAULT \
	{HASHTAB_CAP_DEFAULT, HASHTAB_THRESH_DEFAULT, NULL}

typedef uint32_t (*hash_tab_hash_func)(const uint8_t *key, size_t key_len);

struct hash_tab;

struct hash_tab_itr {
	void *next;
};

struct hash_tab_kv {
	size_t key_len;
	uint8_t *key;
	void *val;
};

struct hash_tab_opts {
	size_t cap_init;
	float rehash_thresh;
	hash_tab_hash_func hash_func;
};

struct hash_add_result {
	int err;
	void *prev;
};


size_t hash_tab_size(const struct hash_tab *tab);
float hash_tab_loading(const struct hash_tab *tab);
void* hash_tab_lookup(
	const struct hash_tab *tab, const uint8_t *key, size_t key_len
);
void* hash_tab_remove(
	struct hash_tab *tab, const uint8_t *key, size_t key_len
);
struct hash_add_result hash_tab_add (
	struct hash_tab *tab, const uint8_t *key, size_t key_len, void *val
);
void hash_tab_itr_init(const struct hash_tab *tab, struct hash_tab_itr *itr);
const struct hash_tab_kv *hash_tab_iterate(
	const struct hash_tab *tab, struct hash_tab_itr *itr
);
const struct hash_tab_kv *hash_tab_iterate(
	const struct hash_tab *tab, struct hash_tab_itr *itr
);
void hash_tab_destroy(struct hash_tab *tab, void (*val_destructor)(void*));
struct hash_tab *hash_tab_init(const struct hash_tab_opts *opts);

#endif 	/* _HASH_TAB_H */
