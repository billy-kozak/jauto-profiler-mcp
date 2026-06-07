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

#include "util/murmur3.h"
#include "util/intrinsic.h"

TEST_CASE(basic_spread_test) {

	char test_key1[] = "A";
	char test_key2[] = "B";

	uint32_t hash1 = murmur3_hash_x86_32(
		test_key1, sizeof(test_key1), 1234
	);
	uint32_t hash2 = murmur3_hash_x86_32(
		test_key2, sizeof(test_key2), 1234
	);

	TEST_ASSERT(popcount32(hash1 ^ hash2) > 10);

	return 0;
}

TEST_SUITE(murmur3_suite, basic_spread_test)
