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

#include "class-info-list.h"

#include "util/hash-tab.h"
#include "util/log.h"

#include <stddef.h>
#include <string.h>

#define LOG_TAG "ci-list"

static void ci_destructor(void *ci_ptr)
{
	struct class_info *ci = ci_ptr;
	ci_free(ci);
}

size_t ci_list_size(const struct class_info_list *list)
{
	return hash_tab_size(list->tab);
}

void ci_list_itr_init(
	const struct class_info_list *list, struct hash_tab_itr *itr
) {
	hash_tab_itr_init(list->tab, itr);
}

struct class_info *ci_list_iterate(
	const struct class_info_list *list, struct hash_tab_itr *itr
) {
	const struct hash_tab_kv *kv = hash_tab_iterate(list->tab, itr);

	if(kv == NULL) {
		return NULL;
	}

	return kv->val;
}

int ci_list_init(struct class_info_list *list)
{
	list->tab = hash_tab_init(NULL);
	return list->tab != NULL ? 0 : 1;
}

int ci_list_add(struct class_info_list *list, struct class_info *ci)
{
	uint8_t *name = (unsigned char*)ci->name->str;
	struct hash_add_result r = hash_tab_add(
		list->tab, name, ci->name->size, ci
	);

	if(r.prev != NULL) {
		LOG_ERROR("Duplicate class add '%s'", ci->name->str);
		ci_destructor(r.prev);
	}

	return r.err;
}

void ci_list_deep_destroy(struct class_info_list *list)
{
	hash_tab_destroy(list->tab, ci_destructor);
}

struct class_info *ci_list_find_by_name(
	const struct class_info_list *list, const char *name
) {
	const uint8_t *uname = (const unsigned char*)name;
	size_t name_len = strlen(name);
	return hash_tab_lookup(list->tab, uname, name_len);
}
