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
#include "testing.h"

#include "util/hash-tab.h"

#include <string.h>

static int un_strcmp(const unsigned char *a, const unsigned char *b)
{
	return strcmp((const char*)a, (const char *)b);
}

TEST_CASE(basic_lookup_test)
{
	unsigned char k[] = "Test Key";
	unsigned char v[] = "Test Value";

	struct hash_tab *tab = hash_tab_init(NULL);

	TEST_ASSERT(hash_tab_add(tab, k, sizeof(k), v).err == 0);

	unsigned char *val = (unsigned char*)hash_tab_lookup(
		tab, k, sizeof(k)
	);

	TEST_ASSERT(un_strcmp(val, v) == 0);

	hash_tab_destroy(tab, NULL);
	return 0;
}

TEST_CASE(rehash_test)
{
	struct hash_tab_opts opts = HASHTAB_OPTS_DEFAULT;

	unsigned char k1[] = "Key 1";
	unsigned char k2[] = "Key 2";
	unsigned char k3[] = "Key 3";

	unsigned char v1[] = "Val 1";
	unsigned char v2[] = "Val 2";
	unsigned char v3[] = "Val 3";


	opts.rehash_thresh = 0.5f;
	opts.cap_init = 4;

	struct hash_tab *tab = hash_tab_init(&opts);

	TEST_ASSERT(hash_tab_add(tab, k1, sizeof(k1), v1).err == 0);
	TEST_ASSERT(hash_tab_add(tab, k2, sizeof(k2), v2).err == 0);
	TEST_ASSERT(hash_tab_add(tab, k3, sizeof(k3), v3).err == 0);

	TEST_ASSERT(hash_tab_loading(tab) < 0.5f);

	unsigned char *r1 = hash_tab_lookup(tab, k1, sizeof(k1));
	unsigned char *r2 = hash_tab_lookup(tab, k2, sizeof(k2));
	unsigned char *r3 = hash_tab_lookup(tab, k3, sizeof(k3));

	TEST_ASSERT(un_strcmp(r1, v1) == 0);
	TEST_ASSERT(un_strcmp(r2, v2) == 0);
	TEST_ASSERT(un_strcmp(r3, v3) == 0);

	hash_tab_destroy(tab, NULL);

	return 0;
}

TEST_CASE(add_rm_itr_test)
{
	struct hash_tab_opts opts = HASHTAB_OPTS_DEFAULT;
	opts.cap_init = 4;

	unsigned char k[] = "a";
	struct hash_tab *tab = hash_tab_init(&opts);

	for(int i = 0; i < 25; i++) {
		uintptr_t v = i;
		int r = hash_tab_add(tab, k, sizeof(k), (void*)v).err;

		TEST_ASSERT(r == 0);

		if((i >= 3) && ((i % 3) == 0)) {
			unsigned char k_r[] = "0";
			k_r[0] = k[0] - 3;

			uintptr_t v_r = (uintptr_t)hash_tab_remove(
				tab, k_r, sizeof(k_r)
			);
			TEST_ASSERT((v - 3) == v_r);
		}
		k[0] += 1;
	}

	struct hash_tab_itr itr;
	hash_tab_itr_init(tab, &itr);
	int count = 0;
	const struct hash_tab_kv *kv;

	while((kv = hash_tab_iterate(tab, &itr)) != NULL) {
		count += 1;
		uintptr_t v = (uintptr_t)kv->val;
		TEST_ASSERT((v == 24) || ((v % 3) != 0));
	}
	TEST_ASSERT(count == 17);
	TEST_ASSERT(count == hash_tab_size(tab));

	k[0] = 'a';
	for(int i = 0; i < 25; i++) {
		uintptr_t l = (uintptr_t)hash_tab_lookup(tab, k, sizeof(k));

		if(((i % 3) != 0) || (i == 24)) {
			TEST_ASSERT(l == i);
		} else {
			TEST_ASSERT(l == 0);
		}
		k[0] += 1;
	}

	hash_tab_destroy(tab, NULL);

	return 0;
}

TEST_CASE(dup_add_test)
{
	unsigned char k[] = "Key 1";

	unsigned char v1[] = "Val 1";
	unsigned char v2[] = "Val 2";

	struct hash_tab *tab = hash_tab_init(NULL);

	struct hash_add_result r1 = hash_tab_add(tab, k, sizeof(k), v1);
	struct hash_add_result r2 = hash_tab_add(tab, k, sizeof(k), v2);

	TEST_ASSERT(r1.err == 0 && r1.prev == NULL);
	TEST_ASSERT(r2.err == 0 && r2.prev == v1);

	unsigned char *val = (unsigned char*)hash_tab_lookup(
		tab, k, sizeof(k)
	);

	TEST_ASSERT(un_strcmp(val, v2) == 0);

	hash_tab_destroy(tab, NULL);

	return 0;
}

TEST_SUITE(
	hash_tab_suite,
	basic_lookup_test,
	rehash_test,
	add_rm_itr_test,
	dup_add_test
)
