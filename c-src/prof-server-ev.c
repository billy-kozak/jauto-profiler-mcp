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

#include "prof-server-ev.h"
#include "prof-server.h"
#include "prof-server-msg.h"

#include <stdlib.h>
#include <string.h>

int ps_send_class_loaded(struct prof_server *ps, const char *name)
{
	struct ps_msg *msg;
	char *name_copy;
	size_t msg_size = sizeof(*msg);

	name_copy = strdup(name);
	if (name_copy == NULL) {
		return -1;
	}

	msg = malloc(sizeof(*msg));
	if (msg == NULL) {
		free(name_copy);
		return -1;
	}

	msg->type = CLASS_LOADED;
	msg->body.class_loaded.name = name_copy;

	if (ps_send_ev(ps, msg, msg_size) != 0) {
		free(name_copy);
		free(msg);
		return -1;
	}

	return 0;
}
