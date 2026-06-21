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
#include <stdint.h>

#include <jni.h>
#include "util/pstring.h"

struct prof_server;
struct ps_msg;
struct user_if_client;

struct ps_msg *ps_send_class_ev_alloc(
	JNIEnv *env,
	const char *name,
	const unsigned char *bytecode,
	size_t bytecode_len,
	jobject loader
);
void ps_send_class_ev_dealloc(JNIEnv *env, struct ps_msg *msg);

int ps_send_class_loaded(
	struct prof_server *ps,
	JNIEnv *env,
	const char *name,
	const unsigned char *bytecode,
	size_t bytecode_len,
	jobject loader
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

int ps_send_usr_rq_deinstrument_by_id(
	struct prof_server *ps,
	struct user_if_client *client,
	uint64_t instrument_id
);

struct ps_msg *ps_usr_rq_instrument_line_alloc(
	struct user_if_client *client,
	const struct pstring *entry_class,
	const struct pstring *exit_class,
	uint32_t entry_line,
	uint32_t exit_line
);
void ps_usr_rq_instrument_line_dealloc(struct ps_msg *msg);
int ps_send_usr_rq_instrument_line(
	struct prof_server *ps,
	struct user_if_client *client,
	const struct pstring *entry_class,
	const struct pstring *exit_class,
	uint32_t entry_line,
	uint32_t exit_line
);

#endif /* _PROF_SERVER_EV_H */
