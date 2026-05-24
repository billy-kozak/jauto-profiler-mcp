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

#include "prof-server.h"

#include "ev-queue.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define PS_CLASSES_INITIAL_CAP 16

enum ps_msg_type {
	CLASS_LOADED,
	PS_SHUTDOWN,
};

struct psm_class_loaded {
	const char *name;
};

struct ps_msg {
	enum ps_msg_type type;
	union {
		struct psm_class_loaded class_loaded;
	} body;
};

struct class_info {
	char *name;
};

struct prof_server {
	struct ev_queue *ev_q;
	pthread_t thread;
	size_t num_loaded_classes;
	size_t classes_capacity;
	struct class_info *loaded_classes;
};

static int add_class(struct prof_server *ps, char *name)
{
	if (ps->num_loaded_classes == ps->classes_capacity) {
		size_t new_cap = ps->classes_capacity * 2;
		struct class_info *arr = realloc(
			ps->loaded_classes,
			new_cap * sizeof(*arr)
		);
		if (arr == NULL) {
			return -1;
		}
		ps->loaded_classes = arr;
		ps->classes_capacity = new_cap;
	}

	ps->loaded_classes[ps->num_loaded_classes].name = name;
	ps->num_loaded_classes += 1;

	return 0;
}

static void handle_class_loaded(
	struct prof_server *ps,
	struct ps_msg *msg
) {
	char *name = (char *)msg->body.class_loaded.name;

	if (add_class(ps, name) != 0) {
		free(name);
	}
}

static int dispatch(struct prof_server *ps, void *raw)
{
	struct ps_msg *msg = (struct ps_msg *)raw;
	int ret = 1;

	switch (msg->type) {
	case CLASS_LOADED:
		handle_class_loaded(ps, msg);
	case PS_SHUTDOWN:
		free(msg);
		ret = 0;
	default:
		free(msg);
	}

	return ret;
}

static void *event_loop(void *arg)
{
	struct prof_server *ps = arg;
	void *msg;

	for (;;) {
		evq_wait(ps->ev_q, &msg);
		if (!dispatch(ps, msg)) {
			break;
		}
	}

	return NULL;
}

struct prof_server *ps_init(void)
{
	struct prof_server *ps;

	ps = malloc(sizeof(*ps));
	if (ps == NULL) {
		return NULL;
	}

	ps->num_loaded_classes = 0;
	ps->classes_capacity = PS_CLASSES_INITIAL_CAP;
	ps->loaded_classes = malloc(
		PS_CLASSES_INITIAL_CAP * sizeof(*ps->loaded_classes)
	);
	if (ps->loaded_classes == NULL) {
		goto fail;
	}

	ps->ev_q = evq_init();
	if (ps->ev_q == NULL) {
		goto fail_classes;
	}

	if (pthread_create(&ps->thread, NULL, event_loop, ps) != 0) {
		goto fail_queue;
	}

	return ps;

fail_queue:
	evq_destroy(ps->ev_q);
fail_classes:
	free(ps->loaded_classes);
fail:
	free(ps);
	return NULL;
}

void ps_destroy(struct prof_server *ps)
{
	struct ps_msg *shutdown;
	size_t i;

	shutdown = malloc(sizeof(*shutdown));
	shutdown->type = PS_SHUTDOWN;

	if (evq_push(ps->ev_q, shutdown, sizeof(*shutdown)) != 0) {
		free(shutdown);
		pthread_cancel(ps->thread);
	}

	pthread_join(ps->thread, NULL);

	for (i = 0; i < ps->num_loaded_classes; i++) {
		free(ps->loaded_classes[i].name);
	}
	free(ps->loaded_classes);

	evq_destroy(ps->ev_q);
	free(ps);
}

int ps_send_class_loaded(struct prof_server *ps, const char *name)
{
	struct ps_msg *msg;
	char *name_copy;

	name_copy = strdup(name);
	if (name_copy == NULL) {
		return -1;
	}

	msg = malloc(sizeof(*msg));
	if (msg == NULL) {
		free(name_copy);
		return -1;
	}

	msg->type = CLASS_LOADED;
	msg->body.class_loaded.name = name_copy;

	if (evq_push(ps->ev_q, msg, sizeof(*msg)) != 0) {
		free(name_copy);
		free(msg);
		return -1;
	}

	return 0;
}
