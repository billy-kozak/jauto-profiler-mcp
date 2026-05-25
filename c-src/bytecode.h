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
#ifndef _BYTECODE_H
#define _BYTECODE_H

#include <stddef.h>

struct method_list;

/*
 * Extract method names and descriptors from a Java class file into methods.
 * Each entry is a heap-allocated "name:descriptor" string owned by the list.
 * On error, any strings added are freed and methods->len is reset to 0.
 * Returns 0 on success, -1 on error.
 */
int bc_extract_methods(
	const unsigned char *class_data,
	size_t class_data_len,
	struct method_list *methods
);

#endif /* _BYTECODE_H */
