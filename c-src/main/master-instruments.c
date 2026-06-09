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

#include "master-instruments.h"

#include "util/hash-tab.h"
#include "util/pstring.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

struct master_instruments {
	struct hash_tab *tab;
	uint64_t next_id;
};

static_assert(
	sizeof(struct mi_itr) == sizeof(struct hash_tab_itr),
	"mi_itr and hash_tab_itr must have the same layout"
);

static void mi_entry_free(void *p)
{
	struct mi_entry *e = p;
	free(e->entry_class_name);
	if (e->type == MI_METHOD) {
		free(e->method.method_name);
	} else {
		free(e->line.exit_class_name);
	}
	free(e);
}

static uint32_t mi_id_hash(const uint8_t *key, size_t key_len)
{
	assert(key_len == sizeof(uint64_t));
	uint64_t id;
	memcpy(&id, key, sizeof(id));
	return (uint32_t)id;
}

struct master_instruments *mi_alloc(void)
{
	static const struct hash_tab_opts opts = {
		HASHTAB_CAP_DEFAULT, HASHTAB_THRESH_DEFAULT, mi_id_hash
	};

	struct master_instruments *mi = malloc(sizeof(*mi));
	if (mi == NULL) {
		return NULL;
	}
	mi->tab = hash_tab_init(&opts);
	if (mi->tab == NULL) {
		free(mi);
		return NULL;
	}
	mi->next_id = 1;
	return mi;
}

void mi_free(struct master_instruments *mi)
{
	hash_tab_destroy(mi->tab, mi_entry_free);
	free(mi);
}

static uint64_t mi_insert(struct master_instruments *mi, struct mi_entry *e)
{
	uint64_t id = mi->next_id;
	struct hash_add_result r = hash_tab_add(
		mi->tab, (uint8_t *)&id, sizeof(id), e
	);
	if (r.err != 0) {
		mi_entry_free(e);
		return 0;
	}
	mi->next_id++;
	return id;
}

uint64_t mi_add_method(
	struct master_instruments *mi,
	const char *entry_class,
	const char *exit_class,
	const char *method_name
) {
	struct mi_entry *e = malloc(sizeof(*e));
	if (e == NULL) {
		return 0;
	}
	e->type = MI_METHOD;
	e->entry_class_name = pstring_from_cstr(entry_class);
	e->method.method_name = pstring_from_cstr(method_name);
	e->method.deferred = true;

	if (e->entry_class_name == NULL || e->method.method_name == NULL) {
		mi_entry_free(e);
		return 0;
	}
	return mi_insert(mi, e);
}

uint64_t mi_add_line(
	struct master_instruments *mi,
	const char *entry_class,
	const char *exit_class,
	int entry_line,
	int exit_line
) {
	struct mi_entry *e = malloc(sizeof(*e));
	if (e == NULL) {
		return 0;
	}
	e->type = MI_LINE;
	e->entry_class_name = pstring_from_cstr(entry_class);
	e->line.exit_class_name  = pstring_from_cstr(exit_class);
	e->line.entry_line_number = entry_line;
	e->line.exit_line_number  = exit_line;
	e->line.entry_deferred = true;
	e->line.exit_deferred  = true;

	if (e->entry_class_name == NULL || e->line.exit_class_name == NULL) {
		mi_entry_free(e);
		return 0;
	}
	return mi_insert(mi, e);
}

void mi_remove(struct master_instruments *mi, uint64_t id)
{
	void *val = hash_tab_remove(mi->tab, (uint8_t *)&id, sizeof(id));
	if (val != NULL) {
		mi_entry_free(val);
	}
}

static struct mi_entry *mi_find_mut(
	const struct master_instruments *mi, uint64_t id
) {
	return hash_tab_lookup(mi->tab, (uint8_t *)&id, sizeof(id));
}

const struct mi_entry *mi_find(
	const struct master_instruments *mi, uint64_t id
) {
	return mi_find_mut(mi, id);
}

int mi_mark_method_applied(struct master_instruments *mi, uint64_t id)
{
	struct mi_entry *e = mi_find_mut(mi, id);
	if (e == NULL || e->type != MI_METHOD) {
		return -1;
	}
	e->method.deferred = false;
	return 0;
}

int mi_mark_line_entry_applied(struct master_instruments *mi, uint64_t id)
{
	struct mi_entry *e = mi_find_mut(mi, id);
	if (e == NULL || e->type != MI_LINE) {
		return -1;
	}
	e->line.entry_deferred = false;
	return 0;
}

int mi_mark_line_exit_applied(struct master_instruments *mi, uint64_t id)
{
	struct mi_entry *e = mi_find_mut(mi, id);
	if (e == NULL || e->type != MI_LINE) {
		return -1;
	}
	e->line.exit_deferred = false;
	return 0;
}

struct mi_itr_result mi_find_method_by_name(
	const struct master_instruments *mi,
	const char *class_name,
	const char *method_sig
) {
	struct mi_itr itr;
	struct mi_itr_result r;

	mi_itr_init(mi, &itr);
	while ((r = mi_iterate(mi, &itr)).entry != NULL) {
		if (r.entry->type != MI_METHOD) {
			continue;
		}
		if (
			strcmp((char *)r.entry->entry_class_name->str,
				class_name) == 0 &&
			strcmp((char *)r.entry->method.method_name->str,
				method_sig) == 0
		) {
			return r;
		}
	}
	return (struct mi_itr_result){0, NULL};
}

void mi_itr_init(const struct master_instruments *mi, struct mi_itr *itr)
{
	hash_tab_itr_init(mi->tab, (struct hash_tab_itr *)itr);
}

struct mi_itr_result mi_iterate(
	const struct master_instruments *mi, struct mi_itr *itr
) {
	struct mi_itr_result result = {0, NULL};
	const struct hash_tab_kv *kv = hash_tab_iterate(
		mi->tab, (struct hash_tab_itr *)itr
	);
	if (kv == NULL) {
		return result;
	}
	result.id    = *(const uint64_t *)kv->key;
	result.entry = kv->val;
	return result;
}

