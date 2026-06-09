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

#include "uif-response.h"

#include "util/pstring.h"
#include "util/hash-tab.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int uif_send_short_response(
	struct user_if *uif,
	struct user_if_client *client,
	enum user_msg_type type,
	const void *body,
	uint32_t body_size
) {
	size_t total = offsetof(struct user_msg, body) + body_size;
	uint8_t buf[total];
	struct user_msg *msg = (struct user_msg *)buf;

	msg->type = type;
	msg->size = body_size;
	if (body_size > 0) {
		memcpy(msg->body.raw, body, body_size);
	}

	uif_send(uif, client, msg);
	return 0;
}

int uif_respond_loaded_classes(
	struct user_if *uif,
	struct user_if_client *client,
	struct class_info_list *classes
) {
	struct user_msg *response;
	uint8_t *p;
	uint32_t body_size;
	uint32_t cnt;
	struct hash_tab_itr itr;
	struct class_info *ci;
	uint32_t count = ci_list_size(classes);


	body_size = sizeof(uint32_t);

	ci_list_itr_init(classes, &itr);
	while((ci = ci_list_iterate(classes, &itr)) != NULL) {
		body_size += pstring_total_size(ci->name);
	}

	response = malloc(offsetof(struct user_msg, body) + body_size);
	if (response == NULL) {
		return -1;
	}

	response->type = RESPONSE_LOADED_CLASSES;
	response->size = body_size;
	p = response->body.raw;

	cnt = (uint32_t)count;
	memcpy(p, &cnt, sizeof(cnt));
	p += sizeof(cnt);

	ci_list_itr_init(classes, &itr);
	while((ci = ci_list_iterate(classes, &itr)) != NULL) {
		size_t sz = pstring_total_size(ci->name);
		memcpy(p, ci->name, sz);
		p += sz;
	}

	uif_send(uif, client, response);
	free(response);
	return 0;
}

int uif_respond_class_methods(
	struct user_if *uif,
	struct user_if_client *client,
	const struct class_info *ci
) {
	struct user_msg *response;
	uint8_t *p;
	uint32_t body_size;
	uint32_t count;
	size_t i;

	body_size = sizeof(uint32_t);
	if (ci != NULL) {
		for (i = 0; i < ci->methods.len; i++) {
			body_size += pstring_total_size(ci->methods.arr[i]);
		}
	}
	count = (ci != NULL) ? (uint32_t)ci->methods.len : 0;

	response = malloc(offsetof(struct user_msg, body) + body_size);
	if (response == NULL) {
		return -1;
	}

	response->type = RESPONSE_CLASS_METHODS;
	response->size = body_size;
	p = response->body.raw;

	memcpy(p, &count, sizeof(count));
	p += sizeof(count);

	if (ci != NULL) {
		for (i = 0; i < ci->methods.len; i++) {
			size_t sz = pstring_total_size(ci->methods.arr[i]);
			memcpy(p, ci->methods.arr[i], sz);
			p += sz;
		}
	}

	uif_send(uif, client, response);
	free(response);
	return 0;
}

int uif_respond_instrument(
	struct user_if *uif,
	struct user_if_client *client,
	enum instrument_resp_status status,
	uint64_t instrument_id
) {
	struct user_msg_instr_resp body = {(uint32_t)status, instrument_id};

	return uif_send_short_response(
		uif, client, RESPONSE_INSTRUMENT_METHOD, &body, sizeof(body)
	);
}

int uif_respond_get_stats(
	struct user_if *uif,
	struct user_if_client *client,
	const uint8_t *stats_buf,
	size_t stats_len
) {
	struct user_msg *response;

	response = malloc(offsetof(struct user_msg, body) + stats_len);
	if (response == NULL) {
		return -1;
	}

	response->type = RESPONSE_GET_STATS;
	response->size = (uint32_t)stats_len;
	memcpy(response->body.raw, stats_buf, stats_len);

	uif_send(uif, client, response);
	free(response);
	return 0;
}

int uif_respond_deinstrument(
	struct user_if *uif,
	struct user_if_client *client,
	enum deinstrument_resp_status status
) {
	struct user_msg_deinstr_resp body = {(uint32_t)status};

	return uif_send_short_response(
		uif, client, RESPONSE_DEINSTRUMENT_METHOD, &body, sizeof(body)
	);
}

int uif_respond_resume(
	struct user_if *uif,
	struct user_if_client *client,
	enum resume_resp_status status
) {
	struct user_msg_resume_resp body = {(uint32_t)status};

	return uif_send_short_response(
		uif, client, RESPONSE_RESUME, &body, sizeof(body)
	);
}

int uif_respond_pause_threads(
	struct user_if *uif,
	struct user_if_client *client,
	enum pause_threads_resp_status status
) {
	struct user_msg_pause_threads_resp body = {(uint32_t)status};

	return uif_send_short_response(
		uif, client, RESPONSE_PAUSE_THREADS, &body, sizeof(body)
	);
}

int uif_respond_list_instrumented(
	struct user_if *uif,
	struct user_if_client *client,
	const struct master_instruments *mi
) {
	struct user_msg *response;
	struct mi_itr itr;
	struct mi_itr_result result;
	uint8_t *p;
	uint32_t body_size;
	uint32_t count;

	body_size = sizeof(uint32_t);
	count = 0;

	mi_itr_init(mi, &itr);
	while ((result = mi_iterate(mi, &itr)).entry != NULL) {
		if (result.entry->type != MI_METHOD) {
			continue;
		}
		body_size += sizeof(struct user_msg_instr_list_entry);
		body_size += pstring_total_size(result.entry->entry_class_name);
		body_size += pstring_total_size(result.entry->method.method_name);
		count++;
	}

	response = malloc(offsetof(struct user_msg, body) + body_size);
	if (response == NULL) {
		return -1;
	}

	response->type = RESPONSE_LIST_INSTRUMENTED;
	response->size = body_size;
	p = response->body.raw;

	memcpy(p, &count, sizeof(count));
	p += sizeof(count);

	mi_itr_init(mi, &itr);
	while ((result = mi_iterate(mi, &itr)).entry != NULL) {
		if (result.entry->type != MI_METHOD) {
			continue;
		}
		struct user_msg_instr_list_entry hdr = {
			result.entry->method.deferred
				? LISTED_INSTR_DEFERRED : LISTED_INSTR_ACTIVE,
			result.id
		};
		memcpy(p, &hdr, sizeof(hdr));
		p += sizeof(hdr);
		p = pstring_memcpy_to(p, result.entry->entry_class_name);
		p = pstring_memcpy_to(p, result.entry->method.method_name);
	}

	uif_send(uif, client, response);
	free(response);
	return 0;
}

int uif_respond_deinstrument_by_id(
	struct user_if *uif,
	struct user_if_client *client,
	enum deinstrument_resp_status status
) {
	struct user_msg_deinstr_resp body = {(uint32_t)status};

	return uif_send_short_response(
		uif, client, RESPONSE_DEINSTRUMENT_BY_ID, &body, sizeof(body)
	);
}

int uif_respond_get_async_errors(
	struct user_if *uif,
	struct user_if_client *client,
	const struct prof_err_log *log
) {
	struct user_msg *response;
	uint8_t *p;
	uint32_t body_size = 0;
	struct prof_err_log_size elog_size;
	struct prof_err_log_itr itr = PROF_ERR_LOG_ITR_INIT;
	struct user_msg_err_resp *body = NULL;

	elog_size = prof_err_log_size(log);
	body_size += sizeof(body->len);
	body_size += elog_size.overhead_size;

	response = malloc(offsetof(struct user_msg, body) + body_size);
	if (response == NULL) {
		return -1;
	}

	body = &(response->body.err_list_resp);
	body->len = elog_size.count;

	response->type = RESPONSE_GET_ASYNC_ERRORS;
	response->size = body_size;

	p = (uint8_t*)(body->list);

	const struct prof_err_entry *e;
	while((e = prof_err_log_iterate(&itr, log)) != NULL){
		memcpy(p, &e->timestamp, sizeof(e->timestamp));
		p += sizeof(e->timestamp);
		p = pstring_memcpy_to(p, e->msg);
	}

	uif_send(uif, client, response);
	free(response);
	return 0;
}
