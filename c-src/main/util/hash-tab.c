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
#include "hash-tab.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "murmur3.h"

struct hash_entry {
	struct hash_entry *next;
	struct hash_entry *prev;
	struct hash_tab_kv kv;
};

struct hash_tab {
	size_t capacity;
	size_t count;
	hash_tab_hash_func hash_fn;
	double thresh;
	struct hash_entry *head;
	struct hash_entry *tab;
};

static uint32_t hashtab_default_hash(const uint8_t *key, size_t key_len)
{
	return murmur3_hash_x86_32(key, key_len, HASHTAB_SEED_DEFAULT);
}

static int keycmp(
	const struct hash_tab_kv *kv, const uint8_t *key, size_t key_len
) {
	if(key_len != kv->key_len) {
		return 1;
	} else {
		return memcmp(kv->key, key, key_len);
	}
}

static void* hash_insert(
	struct hash_tab *tab, uint8_t *key, size_t key_len, void *val
) {
	uint32_t hash = tab->hash_fn(key, key_len);
	int idx = hash % tab->capacity;
	int i;
	struct hash_entry *ent;

	for(i = 0; i < tab->capacity; i++) {
		ent = tab->tab + idx;

		if(ent->kv.key == NULL) {
			break;
		} else if(keycmp(&ent->kv, key, key_len) == 0) {
			break;
		} else {
			idx = (idx + 1) % tab->capacity;
		}
	}
	assert(i < tab->capacity);

	if(ent->kv.key == NULL) {
		ent->kv.key = key;
		ent->kv.key_len = key_len;
		ent->kv.val = val;
		ent->prev = NULL;

		ent->next = tab->head;
		tab->head = ent;

		if(ent->next != NULL) {
			assert(ent->next->prev == NULL);
			ent->next->prev = ent;
		}

		tab->count += 1;
		return NULL;
	} else {
		/* replace duplicate */
		void *prev = ent->kv.val;

		free(ent->kv.key);

		ent->kv.key = key;
		ent->kv.key_len = key_len;
		ent->kv.val = val;

		return prev;
	}
}

static struct hash_tab_kv ent_unlink(
	struct hash_tab *tab, struct hash_entry *ent
) {
	struct hash_tab_kv kv = ent->kv;
	ent->kv.key = NULL;
	ent->kv.val = NULL;

	if(tab->head == ent) {
		tab->head = ent->next;
		if(tab->head != NULL) {
			tab->head->prev = NULL;
		}
	} else {
		assert(ent->prev != NULL);
		ent->prev->next = ent->next;

		if(ent->next != NULL) {
			ent->next->prev = ent->prev;
		}
	}

	tab->count -= 1;

	return kv;
}

static int need_rehash(const struct hash_tab *tab)
{
	size_t cnext = tab->count + 1;

	float l = ((float)(cnext)) / ((float)tab->capacity);

	return l >= tab->thresh;
}

static int rehash(struct hash_tab *tab)
{
	size_t cap_new = tab->capacity * 2;

	struct hash_entry *new = calloc(
		cap_new, sizeof(*(tab->tab))
	);
	if(new == NULL) {
		return -1;
	}

	struct hash_entry *old = tab->tab;
	struct hash_entry *ent = tab->head;

	tab->tab = new;
	tab->head = NULL;
	tab->count = 0;
	tab->capacity = cap_new;

	while(ent != NULL) {
		hash_insert(tab, ent->kv.key, ent->kv.key_len, ent->kv.val);
		ent = ent->next;
	}

	free(old);
	return 0;
}

static struct hash_entry *ent_lookup(
	const struct hash_tab *tab, const uint8_t *key, size_t key_len
) {
	uint32_t hash = tab->hash_fn(key, key_len);
	int idx = hash % tab->capacity;


	for(int i = 0; i < tab->capacity; i++) {
		struct hash_entry *ent = &tab->tab[idx];
		struct hash_tab_kv *kv = &ent->kv;

		if(kv->key == NULL) {
			return NULL;
		} else if(keycmp(kv, key, key_len) == 0) {
			return ent;
		}

		idx = (idx + 1) % tab->capacity;
	}

	return NULL;
}

size_t hash_tab_size(const struct hash_tab *tab)
{
	return tab->count;
}

float hash_tab_loading(const struct hash_tab *tab)
{
	return (float)(tab->count) / (float)(tab->capacity);
}

void* hash_tab_lookup(
	const struct hash_tab *tab, const uint8_t *key, size_t key_len
) {
	struct hash_entry *ent = ent_lookup(tab, key, key_len);

	if(ent == NULL) {
		return NULL;
	} else {
		return ent->kv.val;
	}
}

void* hash_tab_remove(
	struct hash_tab *tab, const uint8_t *key, size_t key_len
) {
	struct hash_entry *ent = ent_lookup(tab, key, key_len);

	if(ent == NULL) {
		return NULL;
	}

	struct hash_tab_kv kv = ent_unlink(tab, ent);

	void *ret = kv.val;
	free(kv.key);

	/* rehash orphaned entries */
	int idx = (ent - tab->tab + 1) % tab->capacity;

	for(int i = 0; i < tab->capacity; i++) {
		struct hash_entry *e = tab->tab + idx;
		if(e->kv.key == NULL) {
			break;
		}

		struct hash_tab_kv r = ent_unlink(tab, e);
		hash_insert(tab, r.key, r.key_len, r.val);
		idx = (idx + 1) % tab->capacity;
	}

	return ret;
}

struct hash_add_result hash_tab_add (
	struct hash_tab *tab,
	const uint8_t *key,
	size_t key_len,
	void *val
) {
	struct hash_add_result ret = {0, NULL};
	assert(key != NULL);

	uint8_t *k_cpy = malloc(key_len);

	if(k_cpy == NULL) {
		goto fail;
	}
	memcpy(k_cpy, key, key_len);

	if(need_rehash(tab)) {
		if(rehash(tab) != 0) {
			goto fail;
		}
	}

	ret.prev = hash_insert(tab, k_cpy, key_len, val);
	return ret;
fail:
	free(k_cpy);
	ret.err = -1;
	return ret;
}

void hash_tab_itr_init(const struct hash_tab *tab, struct hash_tab_itr *itr)
{
	itr->next = tab->head;
}

const struct hash_tab_kv *hash_tab_iterate(
	const struct hash_tab *tab, struct hash_tab_itr *itr
) {
	struct hash_tab_kv *ret = NULL;

	if(itr->next == NULL) {
		return NULL;
	}

	struct hash_entry *ent = (struct hash_entry*)(itr->next);
	ret = &(ent->kv);
	itr->next = ent->next;

	return ret;
}

void hash_tab_destroy(struct hash_tab *tab, void (*val_destructor)(void*))
{
	struct hash_entry *ent = tab->head;

	while(ent != NULL) {
		free(ent->kv.key);
		if(val_destructor != NULL) {
			val_destructor(ent->kv.val);
		}
		ent = ent->next;
	}

	free(tab->tab);
	free(tab);
}

struct hash_tab *hash_tab_init(const struct hash_tab_opts *opts)
{
	struct hash_tab *hash_tab = NULL;
	struct hash_entry *tab = NULL;
	size_t cap = HASHTAB_CAP_DEFAULT;
	float thresh = HASHTAB_THRESH_DEFAULT;
	hash_tab_hash_func hash_fn = hashtab_default_hash;

	if(opts != NULL) {
		cap = opts->cap_init;
		thresh = opts->rehash_thresh;
		if(opts->hash_func != NULL) {
			hash_fn = opts->hash_func;
		}
	}

	assert(cap != 0);
	assert(thresh > 0.0f && thresh < 1.0f);

	hash_tab = malloc(sizeof(*hash_tab));
	tab = calloc(cap, sizeof(*tab));

	if(hash_tab == NULL || tab == NULL) {
		free(hash_tab);
		free(tab);
		return NULL;
	}

	hash_tab->capacity = cap;
	hash_tab->count = 0;
	hash_tab->hash_fn = hash_fn;
	hash_tab->thresh = thresh;
	hash_tab->head = NULL;
	hash_tab->tab = tab;

	return hash_tab;
}
