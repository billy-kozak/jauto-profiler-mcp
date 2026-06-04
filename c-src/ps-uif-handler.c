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

static const char *const USR_SHUTDOWN_MSG = "User requested shutdown";

static int handle_class_methods(
	struct prof_server *ps,
	struct user_if_client *client,
	struct user_msg *msg
) {
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

static int handle_deinstrument_method(
	struct prof_server *ps,
	struct user_if_client *client,
	struct user_msg *msg
) {
	uint8_t *p = msg->body.raw;
	uint16_t class_len;
	uint16_t sig_len;
	size_t off;
	char *class_name;
	char *method_sig;
	int ret;

	if (msg->size < sizeof(uint16_t)) {
		return -1;
	}
	memcpy(&class_len, p, sizeof(class_len));
	off = sizeof(uint16_t) + class_len;

	if (msg->size < (uint32_t)(off + sizeof(uint16_t))) {
		return -1;
	}
	memcpy(&sig_len, p + off, sizeof(sig_len));
	off += sizeof(uint16_t);

	if (msg->size < (uint32_t)(off + sig_len)) {
		return -1;
	}

	class_name = malloc((size_t)class_len + 1);
	if (class_name == NULL) {
		return -1;
	}
	memcpy(class_name, p + sizeof(uint16_t), class_len);
	class_name[class_len] = '\0';

	method_sig = malloc((size_t)sig_len + 1);
	if (method_sig == NULL) {
		free(class_name);
		return -1;
	}
	memcpy(method_sig, p + off, sig_len);
	method_sig[sig_len] = '\0';

	ret = ps_send_usr_rq_deinstrument_method(
		ps, client, class_name, method_sig
	);
	free(class_name);
	free(method_sig);
	return ret;
}

static int handle_instrument_method(
	struct prof_server *ps,
	struct user_if_client *client,
	struct user_msg *msg
) {
	uint8_t *p = msg->body.raw;
	uint16_t class_len;
	uint16_t sig_len;
	size_t off;
	char *class_name;
	char *method_sig;
	int ret;

	if (msg->size < sizeof(uint16_t)) {
		return -1;
	}
	memcpy(&class_len, p, sizeof(class_len));
	off = sizeof(uint16_t) + class_len;

	if (msg->size < (uint32_t)(off + sizeof(uint16_t))) {
		return -1;
	}
	memcpy(&sig_len, p + off, sizeof(sig_len));
	off += sizeof(uint16_t);

	if (msg->size < (uint32_t)(off + sig_len)) {
		return -1;
	}

	class_name = malloc(class_len + 1);
	if (class_name == NULL) {
		return -1;
	}
	memcpy(class_name, p + sizeof(uint16_t), class_len);
	class_name[class_len] = '\0';

	method_sig = malloc(sig_len + 1);
	if (method_sig == NULL) {
		free(class_name);
		return -1;
	}
	memcpy(method_sig, p + off, sig_len);
	method_sig[sig_len] = '\0';

	ret = ps_send_usr_rq_instrument_method(
		ps, client, class_name, method_sig
	);
	free(class_name);
	free(method_sig);
	return ret;
}

int ps_uif_handler(
	void *data, struct user_msg *msg, struct user_if_client *client
) {
	struct prof_server *ps = (struct prof_server *)data;

	switch (msg->type) {
	case REQUEST_LOADED_CLASSES:
		return ps_send_usr_rq_loaded_classes(ps, client);
	case REQUEST_CLASS_METHODS:
		return handle_class_methods(ps, client, msg);
	case REQUEST_INSTRUMENT_METHOD:
		return handle_instrument_method(ps, client, msg);
	case REQUEST_GET_STATS:
		return ps_send_usr_rq_get_stats(ps, client);
	case REQUEST_DEINSTRUMENT_METHOD:
		return handle_deinstrument_method(ps, client, msg);
	case REQUEST_SHUTDOWN:
		return ps_send_shutdown_request(ps, 0, USR_SHUTDOWN_MSG);
	case REQUEST_RESUME:
		return ps_send_usr_rq_resume(ps, client);
	case REQUEST_PAUSE_THREADS:
		return ps_send_usr_rq_pause_threads(ps, client);
	case REQUEST_LIST_INSTRUMENTED:
		return ps_send_usr_rq_list_instrumented(ps, client);
	default:
		return -1;
	}
}
