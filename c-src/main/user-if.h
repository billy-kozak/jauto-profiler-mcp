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

#include "meta/cc.h"
#include "util/pstring.h"

#if __STDC_VERSION__ >= 202300L
	#define A_FLEX
#else
	#define A_FLEX 1
#endif

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
	REQUEST_DEINSTRUMENT_METHOD = 8,
	RESPONSE_DEINSTRUMENT_METHOD = 9,
	REQUEST_SHUTDOWN = 10,
	REQUEST_RESUME = 11,
	RESPONSE_RESUME = 12,
	REQUEST_PAUSE_THREADS = 13,
	RESPONSE_PAUSE_THREADS = 14,
	REQUEST_LIST_INSTRUMENTED = 15,
	RESPONSE_LIST_INSTRUMENTED = 16,
	REQUEST_GET_ASYNC_ERRORS = 17,
	RESPONSE_GET_ASYNC_ERRORS = 18,
};

enum instrument_resp_status {
	INSTRUMENT_RP_OK = 0,
	INSTRUMENT_RP_DOUBLE_INSTRUMENT = 1,
	INSTRUMENT_RP_ERROR = 2,
	INSTRUMENT_RP_DEFERRED = 3,
};

enum deinstrument_resp_status {
	DEINSTRUMENT_RP_OK = 0,
	DEINSTRUMENT_RP_FAIL = 1,
};

enum resume_resp_status {
	RESUME_RP_UNBLOCKED = 0,
	RESUME_RP_NOCHANGE = 1,
	RESUME_RP_ERROR = 2,
};

enum pause_threads_resp_status {
	PAUSE_THREADS_RP_OK = 0,
	PAUSE_THREADS_RP_ALREADY_PAUSED = 1,
	PAUSE_THREADS_RP_ERROR = 2,
	PAUSE_THREADS_RP_RACE_FAILURE = 3,
};

enum listed_instr_status {
	LISTED_INSTR_ACTIVE   = 0,
	LISTED_INSTR_DEFERRED = 1,
};

/*
 * Body of RESPONSE_LIST_INSTRUMENTED:
 *   uint32_t count
 *   count × { uint32_t status, pstring class_name, pstring method_sig }
 * Entries are variable-length and must be walked sequentially.
 */
struct user_msg_instr_list_entry {
	uint32_t status;
	/* packed pstrings follow: class_name, then method_sig */
};


struct PACKED user_msg_instr_resp {
	uint32_t status;
	uint64_t instrument_id;
};

struct user_msg_deinstr_resp {
	uint32_t status;
};

struct user_msg_resume_resp {
	uint32_t status;
};

struct user_msg_pause_threads_resp {
	uint32_t status;
};


struct user_msg_class_list {
	uint32_t len;
	struct pstring classes[];
};

struct user_msg_err_list {
	int64_t timestamp;
	struct pstring msg;
};

struct PACKED user_msg_err_resp {
	uint32_t len;
	struct user_msg_err_list list[A_FLEX];
};

struct PACKED user_msg {
	uint32_t type;
	uint32_t size;
	union {
		struct user_msg_class_list class_list;
		struct pstring class_request;
		uint8_t raw[A_FLEX];
		struct user_msg_instr_resp instr_rep;
		struct user_msg_deinstr_resp deinstr_resp;
		struct user_msg_resume_resp resume_resp;
		struct user_msg_pause_threads_resp pause_threads_resp;
		struct user_msg_err_resp err_list_resp;
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
