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
#ifndef _INSTRINSIC_H_
#define _INSTRINSIC_H_

#include <stdint.h>

#if defined(__GNUC__) && __has_builtin(__builtin_stdc_rotate_left)
static inline uint32_t rotl32 ( uint32_t x, int8_t r )
{
	return __builtin_stdc_rotate_left(x, r);
}

static inline uint64_t rotl64 ( uint64_t x, int8_t r )
{
	return __builtin_stdc_rotate_left(x, r);
}

static inline int popcount32(uint32_t x)
{
	return __builtin_popcount(x);
}
#else
static inline uint32_t rotl32 ( uint32_t x, int8_t r )
{
	return (x << r) | (x >> (32 - r));
}

static inline uint64_t rotl64 ( uint64_t x, int8_t r )
{
	return (x << r) | (x >> (64 - r));
}

static inline int popcount32(uint32_t x)
{
	x -= (x >> 1) & 0x55555555u;
	x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
	return (int)(((x + (x >> 4)) & 0x0f0f0f0fu) * 0x01010101u >> 24);
}
#endif

#endif /* _INSTRINSIC_H_ */
