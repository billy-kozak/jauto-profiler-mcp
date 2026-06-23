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
#ifndef _PROF_ENV_H
#define _PROF_ENV_H

#include <stdint.h>

#define SOCKET_ENV_VAR "JAUTO_PROF_SOCKET"
#define PAUSE_ENV_VAR  "JAUTO_PROF_PAUSE_ON_START"

enum prof_socket_type {
	PROF_SOCKET_UNIX,
	PROF_SOCKET_TCP,
};

struct prof_socket_tcp {
	uint8_t  addr[4]; /* IPv4 address, most-significant octet first */
	uint16_t port;    /* port, host byte order */
};

struct prof_socket_spec {
	enum prof_socket_type type;
	union {
		const char             *unix_path;
		struct prof_socket_tcp  tcp;
	};
};

int prof_socket(struct prof_socket_spec *out);
int prof_pause_on_start(void);

#endif /* _PROF_ENV_H */
