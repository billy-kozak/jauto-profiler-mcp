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

struct user_if_client;

enum ps_msg_type {
	CLASS_LOADED,
	USR_RQ_LOADED_CLASSES,
	USR_RQ_CLASS_METHODS,
	PS_SHUTDOWN,
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

struct ps_msg {
	enum ps_msg_type type;
	union {
		struct psm_class_loaded class_loaded;
		struct psm_usr_rq_loaded_classes usr_rq_loaded_classes;
		struct psm_usr_rq_class_methods usr_rq_class_methods;
	} body;
};

#endif /* _PROF_SERVER_MSG_H */
