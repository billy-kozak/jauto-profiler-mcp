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

#ifndef _EV_QUEUE_H
#define _EV_QUEUE_H

#include <stdlib.h>
#include <sys/types.h>

struct ev_queue;

struct ev_queue *evq_init(void);
void evq_destroy(struct ev_queue *q);
ssize_t evq_wait(struct ev_queue *q, void **data);
int evq_push(struct ev_queue *q, void *data, size_t size);

#endif /* _EV_QUEUE_H */
