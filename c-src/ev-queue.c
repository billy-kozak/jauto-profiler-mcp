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

#include "ev-queue.h"

#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>

#define EVQ_INITIAL_FREE_NODES 16

struct ev_queue_node {
	void *data;
	size_t size;
	struct ev_queue_node *next;
};

struct ev_queue {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	struct ev_queue_node *head;
	struct ev_queue_node *tail;
	struct ev_queue_node *free_list;
};

static struct ev_queue_node *alloc_node(struct ev_queue *q)
{
	struct ev_queue_node *node;

	if (q->free_list != NULL) {
		node = q->free_list;
		q->free_list = node->next;
		return node;
	}
	return malloc(sizeof(*node));
}

static void return_node(struct ev_queue *q, struct ev_queue_node *node)
{
	node->next = q->free_list;
	q->free_list = node;
}

struct ev_queue *evq_init(void)
{
	struct ev_queue *q;

	q = malloc(sizeof(*q));
	if (q == NULL) {
		return NULL;
	}

	if (pthread_mutex_init(&q->mutex, NULL) != 0) {
		goto fail;
	}

	if (pthread_cond_init(&q->cond, NULL) != 0) {
		pthread_mutex_destroy(&q->mutex);
		goto fail;
	}

	q->head = NULL;
	q->tail = NULL;
	q->free_list = NULL;

	for (int i = 0; i < EVQ_INITIAL_FREE_NODES; i++) {
		struct ev_queue_node *node = malloc(sizeof(*node));
		if (node == NULL) {
			break;
		}
		node->next = q->free_list;
		q->free_list = node;
	}

	return q;

fail:
	free(q);
	return NULL;
}

int evq_push(struct ev_queue *q, void *data, size_t size)
{
	struct ev_queue_node *node;

	pthread_mutex_lock(&q->mutex);

	node = alloc_node(q);
	if (node == NULL) {
		pthread_mutex_unlock(&q->mutex);
		return -1;
	}

	node->data = data;
	node->size = size;
	node->next = NULL;

	if (q->tail != NULL) {
		q->tail->next = node;
	} else {
		q->head = node;
	}
	q->tail = node;

	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex);

	return 0;
}

ssize_t evq_wait(struct ev_queue *q, void **data)
{
	struct ev_queue_node *node;
	ssize_t size;

	pthread_mutex_lock(&q->mutex);

	while (q->head == NULL) {
		pthread_cond_wait(&q->cond, &q->mutex);
	}

	node = q->head;
	q->head = node->next;
	if (q->head == NULL) {
		q->tail = NULL;
	}

	*data = node->data;
	size = (ssize_t)node->size;

	return_node(q, node);

	pthread_mutex_unlock(&q->mutex);

	return size;
}
