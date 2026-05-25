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

#include "ps-uif-handler.h"
#include "prof-server-ev.h"

#include <stdlib.h>
#include <string.h>

int ps_uif_handler(
	void *data, struct user_msg *msg, struct user_if_client *client
) {
	struct prof_server *ps = (struct prof_server *)data;

	(void)client;

	switch (msg->type) {
	case REQUEST_LOADED_CLASSES:
		return ps_send_usr_rq_loaded_classes(ps, client);
	case REQUEST_CLASS_METHODS: {
		uint16_t name_len;
		char *name;
		int ret;

		if (msg->size < sizeof(uint16_t)) {
			return -1;
		}
		name_len = msg->body.class_request.size;
		if (msg->size < (uint32_t)(sizeof(uint16_t) + name_len)) {
			return -1;
		}
		name = malloc((size_t)name_len + 1);
		if (name == NULL) {
			return -1;
		}
		memcpy(name, msg->body.class_request.str, name_len);
		name[name_len] = '\0';

		ret = ps_send_usr_rq_class_methods(ps, client, name);
		free(name);
		return ret;
	}
	default:
		return -1;
	}
}
