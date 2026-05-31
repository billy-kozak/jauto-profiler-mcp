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

#include "prof-server-ev.h"
#include "prof-server.h"
#include "prof-server-msg.h"
#include "user-if.h"

#include <stdlib.h>
#include <string.h>

struct ps_msg *ps_send_class_ev_alloc(
	const char *name,
	const unsigned char *bytecode,
	size_t bytecode_len
) {
	struct ps_msg *msg;
	char *name_copy;
	unsigned char *bytecode_copy;

	name_copy = strdup(name);
	if (name_copy == NULL) {
		return NULL;
	}

	bytecode_copy = malloc(bytecode_len);
	if (bytecode_copy == NULL) {
		free(name_copy);
		return NULL;
	}
	memcpy(bytecode_copy, bytecode, bytecode_len);

	msg = malloc(sizeof(*msg));
	if (msg == NULL) {
		free(name_copy);
		free(bytecode_copy);
		return NULL;
	}

	msg->type = CLASS_LOADED;
	msg->body.class_loaded.name = name_copy;
	msg->body.class_loaded.bytecode = bytecode_copy;
	msg->body.class_loaded.bytecode_len = bytecode_len;

	return msg;
}

void ps_send_class_ev_dealloc(struct ps_msg *msg)
{
	free(msg->body.class_loaded.name);
	free(msg->body.class_loaded.bytecode);
	free(msg);
}

int ps_send_class_loaded(
	struct prof_server *ps,
	const char *name,
	const unsigned char *bytecode,
	size_t bytecode_len
) {
	struct ps_msg *msg = ps_send_class_ev_alloc(
		name, bytecode, bytecode_len
	);

	if (msg == NULL) {
		return -1;
	}

	if (ps_send_ev(ps, msg, sizeof(*msg)) != 0) {
		ps_send_class_ev_dealloc(msg);
		return -1;
	}

	return 0;
}

struct ps_msg *ps_usr_rq_class_methods_alloc(
	struct user_if_client *client, const char *class_name
) {
	char *name_copy;
	struct ps_msg *msg;

	name_copy = strdup(class_name);
	if (name_copy == NULL) {
		return NULL;
	}

	msg = malloc(sizeof(*msg));
	if (msg == NULL) {
		free(name_copy);
		return NULL;
	}

	uif_client_acquire(client);
	msg->type = USR_RQ_CLASS_METHODS;
	msg->body.usr_rq_class_methods.client = client;
	msg->body.usr_rq_class_methods.class_name = name_copy;
	return msg;
}

void ps_usr_rq_class_methods_dealloc(struct ps_msg *msg)
{
	uif_client_release(msg->body.usr_rq_class_methods.client);
	free(msg->body.usr_rq_class_methods.class_name);
	free(msg);
}

int ps_send_usr_rq_class_methods(
	struct prof_server *ps,
	struct user_if_client *client,
	const char *class_name
) {
	struct ps_msg *msg = ps_usr_rq_class_methods_alloc(client, class_name);

	if (msg == NULL) {
		return -1;
	}

	if (ps_send_ev(ps, msg, sizeof(*msg)) != 0) {
		ps_usr_rq_class_methods_dealloc(msg);
		return -1;
	}

	return 0;
}

struct ps_msg *ps_usr_rq_instrument_method_alloc(
	struct user_if_client *client,
	const char *class_name,
	const char *method_sig
) {
	char *class_copy;
	char *sig_copy;
	struct ps_msg *msg;

	class_copy = strdup(class_name);
	if (class_copy == NULL) {
		return NULL;
	}

	sig_copy = strdup(method_sig);
	if (sig_copy == NULL) {
		free(class_copy);
		return NULL;
	}

	msg = malloc(sizeof(*msg));
	if (msg == NULL) {
		free(class_copy);
		free(sig_copy);
		return NULL;
	}

	uif_client_acquire(client);
	msg->type = USR_RQ_INSTRUMENT_METHOD;
	msg->body.usr_rq_instrument_method.client = client;
	msg->body.usr_rq_instrument_method.class_name = class_copy;
	msg->body.usr_rq_instrument_method.method_sig = sig_copy;
	return msg;
}

void ps_usr_rq_instrument_method_dealloc(struct ps_msg *msg)
{
	uif_client_release(msg->body.usr_rq_instrument_method.client);
	free(msg->body.usr_rq_instrument_method.class_name);
	free(msg->body.usr_rq_instrument_method.method_sig);
	free(msg);
}

int ps_send_usr_rq_instrument_method(
	struct prof_server *ps,
	struct user_if_client *client,
	const char *class_name,
	const char *method_sig
) {
	struct ps_msg *msg = ps_usr_rq_instrument_method_alloc(
		client, class_name, method_sig
	);

	if (msg == NULL) {
		return -1;
	}

	if (ps_send_ev(ps, msg, sizeof(*msg)) != 0) {
		ps_usr_rq_instrument_method_dealloc(msg);
		return -1;
	}

	return 0;
}

int ps_send_usr_rq_loaded_classes(
	struct prof_server *ps, struct user_if_client *client
) {
	struct ps_msg *msg;

	msg = malloc(sizeof(*msg));
	if (msg == NULL) {
		return -1;
	}

	uif_client_acquire(client);

	msg->type = USR_RQ_LOADED_CLASSES;
	msg->body.usr_rq_loaded_classes.client = client;

	if (ps_send_ev(ps, msg, sizeof(*msg)) != 0) {
		uif_client_release(client);
		free(msg);
		return -1;
	}

	return 0;
}

struct ps_msg *ps_usr_rq_deinstrument_method_alloc(
	struct user_if_client *client,
	const char *class_name,
	const char *method_sig
) {
	char *name_copy;
	char *sig_copy;
	struct ps_msg *msg;

	name_copy = strdup(class_name);
	if (name_copy == NULL) {
		return NULL;
	}

	sig_copy = strdup(method_sig);
	if (sig_copy == NULL) {
		free(name_copy);
		return NULL;
	}

	msg = malloc(sizeof(*msg));
	if (msg == NULL) {
		free(name_copy);
		free(sig_copy);
		return NULL;
	}

	uif_client_acquire(client);
	msg->type = USR_RQ_DEINSTRUMENT_METHOD;
	msg->body.usr_rq_deinstrument_method.client = client;
	msg->body.usr_rq_deinstrument_method.class_name = name_copy;
	msg->body.usr_rq_deinstrument_method.method_sig = sig_copy;
	return msg;
}

void ps_usr_rq_deinstrument_method_dealloc(struct ps_msg *msg)
{
	uif_client_release(msg->body.usr_rq_deinstrument_method.client);
	free(msg->body.usr_rq_deinstrument_method.class_name);
	free(msg->body.usr_rq_deinstrument_method.method_sig);
	free(msg);
}

int ps_send_usr_rq_deinstrument_method(
	struct prof_server *ps,
	struct user_if_client *client,
	const char *class_name,
	const char *method_sig
) {
	struct ps_msg *msg = ps_usr_rq_deinstrument_method_alloc(
		client, class_name, method_sig
	);

	if (msg == NULL) {
		return -1;
	}

	if (ps_send_ev(ps, msg, sizeof(*msg)) != 0) {
		ps_usr_rq_deinstrument_method_dealloc(msg);
		return -1;
	}

	return 0;
}

int ps_send_shutdown_request(
	struct prof_server *ps, int exit_code, const char *msg
) {
	struct ps_msg *ev;
	struct psm_shutdown_request *body;

	ev = malloc(sizeof(*ev));
	if (ev == NULL) {
		return -1;
	}

	body = &ev->body.shutdown_request;

	ev->type = PS_SHUTDOWN_REQUEST;
	body->exit_code = exit_code;

	if (msg != NULL) {
		strncpy(
			body->msg,
			msg,
			PSM_SHUTDOWN_REQUEST_MSG_MAX - 1
		);
		body->msg[PSM_SHUTDOWN_REQUEST_MSG_MAX - 1] = '\0';
	} else {
		body->msg[0] = '\0';
	}

	if (ps_send_ev(ps, ev, sizeof(*ev)) != 0) {
		free(ev);
		return -1;
	}

	return 0;
}

int ps_send_usr_rq_get_stats(
	struct prof_server *ps, struct user_if_client *client
) {
	struct ps_msg *msg;

	msg = malloc(sizeof(*msg));
	if (msg == NULL) {
		return -1;
	}

	uif_client_acquire(client);

	msg->type = USR_RQ_GET_STATS;
	msg->body.usr_rq_get_stats.client = client;

	if (ps_send_ev(ps, msg, sizeof(*msg)) != 0) {
		uif_client_release(client);
		free(msg);
		return -1;
	}

	return 0;
}

int ps_send_usr_rq_resume(
	struct prof_server *ps, struct user_if_client *client
) {
	struct ps_msg *msg;

	msg = malloc(sizeof(*msg));
	if (msg == NULL) {
		return -1;
	}

	uif_client_acquire(client);

	msg->type = USR_RQ_RESUME;
	msg->body.usr_rq_resume.client = client;

	if (ps_send_ev(ps, msg, sizeof(*msg)) != 0) {
		uif_client_release(client);
		free(msg);
		return -1;
	}

	return 0;
}
