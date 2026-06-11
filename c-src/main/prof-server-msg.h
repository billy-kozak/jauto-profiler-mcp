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
#ifndef _PROF_SERVER_MSG_H
#define _PROF_SERVER_MSG_H

#include <stddef.h>
#include <stdint.h>

struct user_if_client;
struct pstring;

enum ps_msg_type {
	CLASS_LOADED,
	USR_RQ_LOADED_CLASSES,
	USR_RQ_CLASS_METHODS,
	USR_RQ_INSTRUMENT_METHOD,
	USR_RQ_GET_STATS,
	USR_RQ_DEINSTRUMENT_METHOD,
	USR_RQ_RESUME,
	PS_SHUTDOWN,
	PS_SHUTDOWN_REQUEST,
	USR_RQ_PAUSE_THREADS,
	USR_RQ_LIST_INSTRUMENTED,
	USR_RQ_GET_ASYNC_ERRORS,
	USR_RQ_DEINSTRUMENT_BY_ID,
	USR_RQ_INSTRUMENT_LINE,
};

#define PSM_SHUTDOWN_REQUEST_MSG_MAX 256

struct psm_shutdown_request {
	int exit_code;
	char msg[PSM_SHUTDOWN_REQUEST_MSG_MAX];
};

struct psm_class_loaded {
	char *name;
	unsigned char *bytecode;
	size_t bytecode_len;
};

struct psm_usr_rq_loaded_classes {
	struct user_if_client *client;
};

struct psm_usr_rq_class_methods {
	struct user_if_client *client;
	char *class_name;
};

struct psm_usr_rq_instrument_method {
	struct user_if_client *client;
	char *class_name;
	char *method_sig;
};

struct psm_usr_rq_get_stats {
	struct user_if_client *client;
};

struct psm_usr_rq_deinstrument_method {
	struct user_if_client *client;
	char *class_name;
	char *method_sig;
};

struct psm_usr_rq_resume {
	struct user_if_client *client;
};

struct psm_usr_rq_pause_threads {
	struct user_if_client *client;
};

struct psm_usr_rq_list_instrumented {
	struct user_if_client *client;
};

struct psm_usr_rq_get_async_errors {
	struct user_if_client *client;
};

struct psm_usr_rq_deinstrument_by_id {
	struct user_if_client *client;
	uint64_t instrument_id;
};

struct psm_usr_rq_instrument_line {
	struct user_if_client *client;
	struct pstring *entry_class;
	struct pstring *exit_class;
	uint32_t entry_line;
	uint32_t exit_line;
};

struct ps_msg {
	enum ps_msg_type type;
	union {
		struct psm_class_loaded class_loaded;
		struct psm_usr_rq_loaded_classes usr_rq_loaded_classes;
		struct psm_usr_rq_class_methods usr_rq_class_methods;
		struct psm_usr_rq_instrument_method usr_rq_instrument_method;
		struct psm_usr_rq_get_stats usr_rq_get_stats;
		struct psm_shutdown_request shutdown_request;
		struct psm_usr_rq_deinstrument_method usr_rq_deinstrument_method;
		struct psm_usr_rq_resume usr_rq_resume;
		struct psm_usr_rq_pause_threads usr_rq_pause_threads;
		struct psm_usr_rq_list_instrumented usr_rq_list_instrumented;
		struct psm_usr_rq_get_async_errors usr_rq_get_async_errors;
		struct psm_usr_rq_deinstrument_by_id usr_rq_deinstrument_by_id;
		struct psm_usr_rq_instrument_line usr_rq_instrument_line;
	} body;
};

#endif /* _PROF_SERVER_MSG_H */
