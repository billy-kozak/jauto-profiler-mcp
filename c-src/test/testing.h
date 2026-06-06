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
#ifndef _TESTING_H
#define _TESTING_H

#include <stdlib.h>
#include <stdio.h>

#define TEST_ASSERT(cond) \
	if(!(cond)) { \
		printf("Test assert fail line %d\n", __LINE__); \
		return 1; \
	}

#define TEST_CASE(name) \
	static int _test_case_##name(void); \
	\
	static int name(void) { \
		int ret = _test_case_##name(); \
		if(ret) { \
			printf("Test case %s failed\n", #name); \
		} \
		return ret; \
	} \
	static int _test_case_##name(void)

#define TEST_SUITE_DECL(name) int name(void)

#define TEST_SUITE(name, ...) \
	TEST_SUITE_DECL(name) { \
		int (*tests[])(void) = {__VA_ARGS__}; \
		int n = sizeof(tests) / sizeof(tests[0]); \
		int ret = 0; \
		for(int i = 0; i < n; i++) { \
			if(tests[i]()) { \
				ret += 1; \
			} \
		} \
		if(ret) { \
			printf( \
				"Suite \"%s\" failed %d tests.\n", #name, ret \
			); \
		} else { \
			printf("Suite \"%s\" passed all tests.\n", #name); \
	 	} \
		return ret; \
	}

#endif /* _TESTING_H */
