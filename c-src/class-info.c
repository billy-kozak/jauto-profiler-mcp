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

#include "class-info.h"

#include <stdlib.h>
#include <string.h>

struct class_info *ci_alloc(char *name, char **methods, size_t num_methods)
{
	struct class_info *ci = malloc(sizeof(*ci));

	if (ci == NULL) {
		return NULL;
	}
	ci->name = strdup(name);
	if(ci->name == NULL) {
		goto fail;
	}

	ci->methods = methods;
	ci->num_methods = num_methods;
	return ci;
fail:
	free(ci);
	return NULL;
}

void ci_free(struct class_info *ci)
{
	size_t i;

	free(ci->name);
	for (i = 0; i < ci->num_methods; i++) {
		free(ci->methods[i]);
	}
	free(ci->methods);
	free(ci);
}
