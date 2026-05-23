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

struct user_if;
struct user_if_client;

enum user_msg_type {
	REQUEST_LOADED_CLASSES = 0,
	RESPONSE_LOADED_CLASSES = 1
};

struct user_msg_string {
	uint16_t size;
	unsigned char str[];
};

struct user_msg_class_list {
	uint32_t len;
	struct user_msg_string classes[];
};

struct PACKED user_msg {
	uint32_t type;
	uint32_t size;
	union {
		struct user_msg_class_list class_list;
	} body;
};

struct user_if *uif_init(const char *path);
struct user_if *uif_destroy(struct user_if *uif);
void uif_register_handler(
		struct user_if *uif,
		int (*handler)(struct user_msg*, struct user_if_client*)
);
int uif_send(
		struct user_if *uif,
		struct user_if_client *client,
		const struct user_msg *msg
);

#endif /* _USER_IF_H */
