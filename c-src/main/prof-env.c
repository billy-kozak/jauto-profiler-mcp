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

#include "prof-env.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TCP_SCHEME     "tcp://"
#define TCP_SCHEME_LEN (sizeof(TCP_SCHEME) - 1)

static int parse_tcp_spec(const char *spec, struct prof_socket_spec *out)
{
	const char *addr_part = spec + TCP_SCHEME_LEN;
	unsigned int a, b, c, d, port;
	int n;

	n = sscanf(addr_part, "%u.%u.%u.%u:%u", &a, &b, &c, &d, &port);
	if (n != 5) {
		goto fail;
	}
	if (a > 255 || b > 255 || c > 255 || d > 255) {
		goto fail;
	}
	if (port == 0 || port > 65535) {
		goto fail;
	}

	out->type         = PROF_SOCKET_TCP;
	out->tcp.addr[0]  = (uint8_t)a;
	out->tcp.addr[1]  = (uint8_t)b;
	out->tcp.addr[2]  = (uint8_t)c;
	out->tcp.addr[3]  = (uint8_t)d;
	out->tcp.port     = (uint16_t)port;
	return 0;

fail:
	fprintf(
		stderr,
		"jauto-profiler: invalid tcp socket spec '%s'. "
		"Expected format: tcp://<a.b.c.d>:<port>. "
		"Set %s to configure.\n",
		spec, SOCKET_ENV_VAR
	);
	return -1;
}

int prof_socket(struct prof_socket_spec *out)
{
	const char *val = getenv(SOCKET_ENV_VAR);

	if (val == NULL) {
		out->type      = PROF_SOCKET_UNIX;
		out->unix_path = "/tmp/jauto-profiler.sock";
		return 0;
	}

	if (strncmp(val, TCP_SCHEME, TCP_SCHEME_LEN) == 0) {
		return parse_tcp_spec(val, out);
	}

	out->type      = PROF_SOCKET_UNIX;
	out->unix_path = val;
	return 0;
}

int prof_pause_on_start(void)
{
	const char *val = getenv(PAUSE_ENV_VAR);
	if (val == NULL) {
		return 1;
	}
	return strcmp(val, "0") != 0;
}
