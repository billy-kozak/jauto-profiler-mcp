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
#ifndef _BC_INSTRUMENT_H
#define _BC_INSTRUMENT_H

#include <stddef.h>
#include <jni.h>

int bc_instrument_init_refs(JNIEnv *env);

/*
 * Return a heap-allocated transformed copy of class_data with
 * ProfilerRegistry.enter/exit injected into each method identified by
 * method_sigs[] (in "name:descriptor" form) and profiler_ids[], both of
 * length count.  Caller must free() the result.
 * Returns NULL on error (OOM, malformed class, Java exception).
 */
unsigned char *bc_instrument_method(
	JNIEnv *env,
	const unsigned char *class_data,
	size_t class_data_len,
	const char **method_sigs,
	const int *profiler_ids,
	int count,
	size_t *new_len_out
);

#endif /* _BC_INSTRUMENT_H */
