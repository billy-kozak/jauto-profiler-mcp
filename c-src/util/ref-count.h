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
#ifndef _REF_COUNT_H
#define _REF_COUNT_H

#include <stdatomic.h>

typedef _Atomic int ref_count;

static inline void rc_init(ref_count *rc)
{
	atomic_init(rc, 1);
}

static inline void rc_acquire(ref_count *rc)
{
	atomic_fetch_add_explicit(rc, 1, memory_order_relaxed);
}

/* Returns the refcount value after decrement. Caller must free when 0. */
static inline int rc_release(ref_count *rc)
{
	return atomic_fetch_sub_explicit(rc, 1, memory_order_acq_rel) - 1;
}

#endif /* _REF_COUNT_H */
