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
#ifndef _PROF_SERVER_EV_H
#define _PROF_SERVER_EV_H

#include <stddef.h>

struct prof_server;
struct ps_msg;
struct user_if_client;

struct ps_msg *ps_send_class_ev_alloc(
	const char *name,
	const unsigned char *bytecode,
	size_t bytecode_len
);
void ps_send_class_ev_dealloc(struct ps_msg *msg);

int ps_send_class_loaded(
	struct prof_server *ps,
	const char *name,
	const unsigned char *bytecode,
	size_t bytecode_len
);
int ps_send_usr_rq_loaded_classes(
	struct prof_server *ps, struct user_if_client *client
);
int ps_send_usr_rq_get_stats(
	struct prof_server *ps, struct user_if_client *client
);
int ps_send_usr_rq_resume(
	struct prof_server *ps, struct user_if_client *client
);

struct ps_msg *ps_usr_rq_class_methods_alloc(
	struct user_if_client *client, const char *class_name
);
void ps_usr_rq_class_methods_dealloc(struct ps_msg *msg);
int ps_send_usr_rq_class_methods(
	struct prof_server *ps,
	struct user_if_client *client,
	const char *class_name
);

struct ps_msg *ps_usr_rq_instrument_method_alloc(
	struct user_if_client *client,
	const char *class_name,
	const char *method_sig
);
void ps_usr_rq_instrument_method_dealloc(struct ps_msg *msg);
int ps_send_usr_rq_instrument_method(
	struct prof_server *ps,
	struct user_if_client *client,
	const char *class_name,
	const char *method_sig
);

struct ps_msg *ps_usr_rq_deinstrument_method_alloc(
	struct user_if_client *client,
	const char *class_name,
	const char *method_sig
);
void ps_usr_rq_deinstrument_method_dealloc(struct ps_msg *msg);
int ps_send_usr_rq_deinstrument_method(
	struct prof_server *ps,
	struct user_if_client *client,
	const char *class_name,
	const char *method_sig
);

int ps_send_shutdown_request(
	struct prof_server *ps, int exit_code, const char *msg
);

int ps_send_usr_rq_pause_threads(
	struct prof_server *ps, struct user_if_client *client
);

int ps_send_usr_rq_list_instrumented(
	struct prof_server *ps, struct user_if_client *client
);

int ps_send_usr_rq_get_async_errors(
	struct prof_server *ps, struct user_if_client *client
);

#endif /* _PROF_SERVER_EV_H */
