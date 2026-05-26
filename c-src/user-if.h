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

#ifndef _USER_IF_H
#define _USER_IF_H

#include <stdint.h>

#include "cc.h"
#include "pstring.h"

struct user_if;
struct user_if_client;

enum user_msg_type {
	REQUEST_LOADED_CLASSES = 0,
	RESPONSE_LOADED_CLASSES = 1,
	REQUEST_CLASS_METHODS = 2,
	RESPONSE_CLASS_METHODS = 3,
	REQUEST_INSTRUMENT_METHOD = 4,
	RESPONSE_INSTRUMENT_METHOD = 5,
	REQUEST_GET_STATS = 6,
	RESPONSE_GET_STATS = 7,
};

struct user_msg_class_list {
	uint32_t len;
	struct pstring classes[];
};

struct PACKED user_msg {
	uint32_t type;
	uint32_t size;
	union {
		struct user_msg_class_list class_list;
		struct pstring class_request;
		uint8_t raw[];
	} body;
};

struct user_if *uif_init(const char *path);
struct user_if *uif_destroy(struct user_if *uif);
void uif_register_handler(
	struct user_if *uif,
	int (*handler)(void* data, struct user_msg*, struct user_if_client*),
	void *data
);
int uif_send(
		struct user_if *uif,
		struct user_if_client *client,
		const struct user_msg *msg
);
void uif_client_acquire(struct user_if_client *client);
void uif_client_release(struct user_if_client *client);
void uif_client_set_closed(struct user_if_client *client);
int uif_client_is_closed(struct user_if_client *client);

#endif /* _USER_IF_H */
