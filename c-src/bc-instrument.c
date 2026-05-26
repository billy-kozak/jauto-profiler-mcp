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

#include "bc-instrument.h"

#include <string.h>
#include <stdlib.h>

/* -------------------------------------------------------------------------
 * Main function
 * ---------------------------------------------------------------------- */
unsigned char *bc_instrument_method(
	const unsigned char *orig,
	size_t orig_len,
	const char *method_name,
	const char *method_desc,
	int profiler_id,
	size_t *new_len_out
) {
	*new_len_out = orig_len;

	unsigned char *out = malloc(orig_len);
	memcpy(out, orig, orig_len);
	return out;
}
