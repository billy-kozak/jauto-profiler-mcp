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

#include "pstring-suite.h"

#include "util/pstring.h"
#include "testing.h"

#include <stdlib.h>

TEST_CASE(pstring_test_null_term) {
	char test_str[] = "This is a test string";
	struct pstring *pstr = pstring_from_cstr(test_str);

	TEST_ASSERT(pstr->str[sizeof(test_str) - 1] == '\0');

	free(pstr);

	return 0;
}

TEST_SUITE(pstring_suite, pstring_test_null_term)
