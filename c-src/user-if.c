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

#include "user-if.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdbool.h>

#include "dyn-arr.h"

#define UIF_LISTEN_BACKLOG  5
#define UIF_MAX_BODY_SIZE   (1024 * 1024)
#define UIF_INIT_CAP_CLIENTS 16
#define UIF_MAX_EVENTS      (16)
#define UIF_MSG_HDR_SIZE    offsetof(struct user_msg, body)

struct user_if_client {
	int fd;
};

struct uif_client_ctx {
	struct user_if_client client;
	uint8_t *buf;
	size_t buf_alloc;
	size_t buf_expected;
	size_t bytes_read;
};

struct user_if {
	int server_fd;
	int epoll_fd;
	int shutdown_fd;
	pthread_t listener;
	char *socket_path;
	int (*handler)(struct user_msg *, struct user_if_client *);
	DYNARR_TEMPLATE(struct uif_client_ctx) clients;
};

static int write_exact(int fd, const void *buf, size_t n)
{
	size_t done = 0;
	ssize_t w;

	while (done < n) {
		w = write(fd, (const char *)buf + done, n - done);
		if (w <= 0) {
			return -1;
		}
		done += (size_t)w;
	}
	return 0;
}

static struct uif_client_ctx *find_client(struct user_if *uif, int fd)
{
	int i;

	for (i = 0; i < DYNARR_LEN(uif->clients); i++) {
		if (uif->clients.arr[i].client.fd == fd) {
			return &uif->clients.arr[i];
		}
	}
	return NULL;
}

static void close_client(struct user_if *uif, struct uif_client_ctx *ctx)
{
	int i;

	epoll_ctl(uif->epoll_fd, EPOLL_CTL_DEL, ctx->client.fd, NULL);
	close(ctx->client.fd);
	free(ctx->buf);

	for (i = 0; i < DYNARR_LEN(uif->clients); i++) {
		if (&uif->clients.arr[i] == ctx) {
			DYNARR_REMOVE(uif->clients, i);
			break;
		}
	}
}

static void close_all_clients(struct user_if *uif)
{
	int i;
	for (i = 0; i < DYNARR_LEN(uif->clients); i++) {
		close(uif->clients.arr[i].client.fd);
		free(uif->clients.arr[i].buf);
	}
}

static int accept_client(struct user_if *uif)
{
	struct uif_client_ctx *ctx;
	struct epoll_event ev;
	int fd;
	int i = DYNARR_LEN(uif->clients);

	if(DYNARR_GROW(uif->clients)) {
		goto fail_0;
	}
	ctx = &DYNARR_AT(uif->clients, i);

	fd = accept(uif->server_fd, NULL, NULL);
	if (fd < 0) {
		goto fail_1;
	}

	if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
		goto fail_2;
	}

	ctx->buf = malloc(UIF_MSG_HDR_SIZE);
	if (ctx->buf == NULL) {
		goto fail_2;
	}

	ctx->client.fd = fd;
	ctx->buf_alloc = UIF_MSG_HDR_SIZE;
	ctx->buf_expected = UIF_MSG_HDR_SIZE;
	ctx->bytes_read = 0;

	ev.events = EPOLLIN;
	ev.data.fd = fd;

	if (epoll_ctl(uif->epoll_fd, EPOLL_CTL_ADD, fd, &ev) != 0) {
		goto fail_3;
	}

	return 0;

fail_3:
	free(ctx->buf);
fail_2:
	close(fd);
fail_1:
	DYNARR_REMOVE(uif->clients, i);
fail_0:
	return 1;
}

static void handle_client_data(
	struct user_if *uif,
	struct uif_client_ctx *ctx
) {
	struct user_msg *msg;
	uint8_t *new_buf;
	uint32_t body_size;
	ssize_t n;

	errno = 0;
	n = read(
		ctx->client.fd,
		ctx->buf + ctx->bytes_read,
		ctx->buf_expected - ctx->bytes_read
	);

	if (n <= 0) {
		if(errno != EAGAIN && errno != EWOULDBLOCK) {
			close_client(uif, ctx);
		}
		return;
	}

	ctx->bytes_read += (size_t)n;

	if (ctx->bytes_read == UIF_MSG_HDR_SIZE) {
		msg = (struct user_msg *)ctx->buf;
		body_size = msg->size;

		if (body_size > UIF_MAX_BODY_SIZE) {
			close_client(uif, ctx);
			return;
		}

		if (body_size > 0) {
			size_t needed = UIF_MSG_HDR_SIZE + body_size;
			if (needed > ctx->buf_alloc) {
				new_buf = realloc(ctx->buf, needed);
				if (new_buf == NULL) {
					close_client(uif, ctx);
					return;
				}
				ctx->buf = new_buf;
				ctx->buf_alloc = needed;
			}
			ctx->buf_expected = needed;
		}
	}

	if (ctx->bytes_read == ctx->buf_expected) {
		if (uif->handler != NULL) {
			uif->handler(
				(struct user_msg *)ctx->buf,
				&ctx->client
			);
		}
		ctx->bytes_read = 0;
		ctx->buf_expected = UIF_MSG_HDR_SIZE;
	}
}

static void *listener_thread(void *arg)
{
	struct user_if *uif = (struct user_if *)arg;
	struct epoll_event events[UIF_MAX_EVENTS];
	struct uif_client_ctx *ctx;
	uint64_t val;
	int n, i, fd;

	for (;;) {
		n = epoll_wait(uif->epoll_fd, events, UIF_MAX_EVENTS, -1);
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			break;
		}

		for (i = 0; i < n; i++) {
			fd = events[i].data.fd;

			if (fd == uif->shutdown_fd) {
				while(true) {
					int read_ret = read(
							fd, &val, sizeof(val)
					);
					if(read_ret != -1 || errno != EINTR) {
						break;
					}
				}
				close_all_clients(uif);
				return NULL;
			}

			if (fd == uif->server_fd) {
				accept_client(uif);
			} else {
				ctx = find_client(uif, fd);
				if (ctx != NULL) {
					handle_client_data(uif, ctx);
				}
			}
		}
	}

	close_all_clients(uif);
	return NULL;
}

struct user_if *uif_init(const char *path)
{
	struct user_if *uif;
	struct sockaddr_un addr;
	struct epoll_event ev;
	size_t path_len = strlen(path);

	if (path_len >= sizeof(addr.sun_path)) {
		return NULL;
	}

	uif = malloc(sizeof(*uif));
	if (uif == NULL) {
		return NULL;
	}
	if(DYNARR_INIT(uif->clients, UIF_INIT_CAP_CLIENTS)) {
		goto fail;
	}

	uif->handler = NULL;

	uif->socket_path = strdup(path);
	if (uif->socket_path == NULL) {
		goto fail_clients;
	}

	uif->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	if (uif->epoll_fd < 0) {
		goto fail_path;
	}

	uif->shutdown_fd = eventfd(0, EFD_CLOEXEC);
	if (uif->shutdown_fd < 0) {
		goto fail_epoll;
	}

	uif->server_fd = socket(
		AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0
	);
	if (uif->server_fd < 0) {
		goto fail_shutdown;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	memcpy(addr.sun_path, path, path_len + 1);

	unlink(path);

	int bind_ret = bind(
		uif->server_fd, (struct sockaddr *)&addr, sizeof(addr)
	);

	if (bind_ret != 0) {
		goto fail_server;
	}

	if (listen(uif->server_fd, UIF_LISTEN_BACKLOG) != 0) {
		goto fail_server;
	}

	ev.events = EPOLLIN;
	ev.data.fd = uif->server_fd;
	if (epoll_ctl(
		uif->epoll_fd, EPOLL_CTL_ADD, uif->server_fd, &ev
	) != 0) {
		goto fail_server;
	}

	ev.events = EPOLLIN;
	ev.data.fd = uif->shutdown_fd;

	int epc_ret = epoll_ctl(
		uif->epoll_fd, EPOLL_CTL_ADD, uif->shutdown_fd, &ev
	);
	if (epc_ret != 0) {
		goto fail_server;
	}

	if (pthread_create( &uif->listener, NULL, listener_thread, uif) != 0) {
		goto fail_server;
	}
	return uif;

fail_server:
	close(uif->server_fd);
	unlink(path);
fail_shutdown:
	close(uif->shutdown_fd);
fail_epoll:
	close(uif->epoll_fd);
fail_path:
	free(uif->socket_path);
fail_clients:
	DYNARR_DESTROY(uif->clients);
fail:
	free(uif);
	return NULL;
}

struct user_if *uif_destroy(struct user_if *uif)
{
	uint64_t val = 1;

	while (write(
		uif->shutdown_fd, &val, sizeof(val)
	) == -1 && errno == EINTR) {}
	pthread_join(uif->listener, NULL);

	close(uif->server_fd);
	close(uif->shutdown_fd);
	close(uif->epoll_fd);
	unlink(uif->socket_path);
	free(uif->socket_path);
	DYNARR_DESTROY(uif->clients);
	free(uif);
	return NULL;
}

void uif_register_handler(
	struct user_if *uif,
	int (*handler)(struct user_msg *, struct user_if_client *)
) {
	uif->handler = handler;
}

int uif_send(
	struct user_if *uif,
	struct user_if_client *client,
	const struct user_msg *msg
) {
	(void)uif;
	return write_exact(
		client->fd, msg, UIF_MSG_HDR_SIZE + msg->size
	);
}
