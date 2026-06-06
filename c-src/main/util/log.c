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

#include "log.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "meta/cc.h"

#define LOG_TAG "log"
#define LOG_INITIAL_BUF 1024

static int log_fd = STDERR_FILENO;
static enum log_level log_threshold = LOG_LEVEL_INFO;

static THREAD_LOCAL char *log_buf = NULL;
static THREAD_LOCAL size_t log_buf_cap = 0;

static const char *const LOG_PATH_DEFAULT = "/tmp/jauto-prof.log";

static const char *level_str(enum log_level level)
{
	switch (level) {
	case LOG_LEVEL_OFF:   return "OFF";
	case LOG_LEVEL_ERROR: return "ERROR";
	case LOG_LEVEL_WARN:  return "WARN ";
	case LOG_LEVEL_INFO:  return "INFO ";
	case LOG_LEVEL_DEBUG: return "DEBUG";
	default:              return "?    ";
	}
}

static enum log_level parse_level(const char *s)
{
	if (strcmp(s, "OFF") == 0)   { return LOG_LEVEL_OFF; }
	if (strcmp(s, "ERROR") == 0) { return LOG_LEVEL_ERROR; }
	if (strcmp(s, "WARN")  == 0) { return LOG_LEVEL_WARN; }
	if (strcmp(s, "INFO")  == 0) { return LOG_LEVEL_INFO; }
	if (strcmp(s, "DEBUG") == 0) { return LOG_LEVEL_DEBUG; }
	return LOG_LEVEL_INFO;
}

static int grow_buf(void)
{
	size_t new_cap = (log_buf_cap == 0) ? LOG_INITIAL_BUF : log_buf_cap * 2;
	char *p = realloc(log_buf, new_cap);
	if (p == NULL) {
		return -1;
	}
	log_buf = p;
	log_buf_cap = new_cap;
	return 0;
}

void log_init(void)
{
	const char *path  = getenv(LOG_PATH_ENV_VAR);
	const char *level = getenv(LOG_LEVEL_ENV_VAR);
	int fd;

	if (level != NULL && level[0] != '\0') {
		log_threshold = parse_level(level);
	}

	if(log_threshold <= LOG_LEVEL_OFF) {
		return;
	}

	if(path == NULL) {
		path = LOG_PATH_DEFAULT;
	}

	if (path[0] != '\0') {
		fd = open(
			path,
			O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC,
			0644
		);
		if (fd >= 0) {
			log_fd = fd;
		} else {
			LOG_WARN(
				"Failed to open log file at '%s'. Defaulting "
				"logging to stderr." ,
				path
			);
		}
	}
}

void log_write(
	enum log_level level, const char *tag, const char *fmt, ...
) {
	struct timespec ts;
	struct tm tm;
	char timebuf[32];
	va_list ap;
	int prefix_len, msg_len;

	if (level > log_threshold) {
		return;
	}

	if (log_buf == NULL && grow_buf() != 0) {
		return;
	}

	clock_gettime(CLOCK_REALTIME, &ts);
	localtime_r(&ts.tv_sec, &tm);
	strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%S", &tm);

	for (;;) {
		prefix_len = snprintf(
			log_buf, log_buf_cap,
			"%s.%03ld %s [%s] ",
			timebuf, ts.tv_nsec / 1000000,
			level_str(level), tag
		);
		if (prefix_len < 0) {
			return;
		}
		if ((size_t)prefix_len < log_buf_cap) {
			break;
		}
		if (grow_buf() != 0) {
			return;
		}
	}

	for (;;) {
		/* reserve one byte past the null for the newline */
		size_t msg_cap = log_buf_cap - (size_t)prefix_len - 1;

		va_start(ap, fmt);
		msg_len = vsnprintf(
			log_buf + prefix_len, msg_cap + 1, fmt, ap
		);
		va_end(ap);

		if (msg_len < 0) {
			return;
		}
		if ((size_t)msg_len <= msg_cap) {
			break;
		}
		if (grow_buf() != 0) {
			return;
		}
	}

	int total = prefix_len + msg_len;
	log_buf[total] = '\n';

	ssize_t n = write(log_fd, log_buf, (size_t)(total + 1));
	(void)n;
}
