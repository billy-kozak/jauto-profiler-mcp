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

int ps_uif_handler(
	void *data, struct user_msg *msg, struct user_if_client *client
) {
	struct prof_server *ps = (struct prof_server *)data;

	(void)client;

	switch (msg->type) {
	case REQUEST_LOADED_CLASSES:
		return ps_send_usr_rq_loaded_classes(ps);
	default:
		return -1;
	}
}
