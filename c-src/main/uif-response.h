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
#ifndef _UIF_RESPONSE_H
#define _UIF_RESPONSE_H

#include "user-if.h"
#include "class-info.h"
#include "class-info-list.h"
#include "queued-instrument.h"
#include "prof-err-log.h"

#include <stddef.h>
#include <stdint.h>

int uif_respond_loaded_classes(
	struct user_if *uif,
	struct user_if_client *client,
	struct class_info_list *classes
);

int uif_respond_class_methods(
	struct user_if *uif,
	struct user_if_client *client,
	const struct class_info *ci
);

int uif_respond_instrument(
	struct user_if *uif,
	struct user_if_client *client,
	enum instrument_resp_status status,
	uint64_t instrument_id
);

int uif_respond_get_stats(
	struct user_if *uif,
	struct user_if_client *client,
	const uint8_t *stats_buf,
	size_t stats_len
);

int uif_respond_deinstrument(
	struct user_if *uif,
	struct user_if_client *client,
	enum deinstrument_resp_status status
);

int uif_respond_resume(
	struct user_if *uif,
	struct user_if_client *client,
	enum resume_resp_status status
);

int uif_respond_pause_threads(
	struct user_if *uif,
	struct user_if_client *client,
	enum pause_threads_resp_status status
);

int uif_respond_list_instrumented(
	struct user_if *uif,
	struct user_if_client *client,
	const struct class_info_list *classes,
	const struct queued_instr_list *queued
);

int uif_respond_get_async_errors(
	struct user_if *uif,
	struct user_if_client *client,
	const struct prof_err_log *log
);

#endif /* _UIF_RESPONSE_H */
