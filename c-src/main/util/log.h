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
#ifndef _LOG_H
#define _LOG_H

#define LOG_PATH_ENV_VAR  "JAUTO_PROF_LOG"
#define LOG_LEVEL_ENV_VAR "JAUTO_PROF_LOG_LEVEL"

enum log_level {
	LOG_LEVEL_OFF = -1,
	LOG_LEVEL_ERROR = 0,
	LOG_LEVEL_WARN  = 1,
	LOG_LEVEL_INFO  = 2,
	LOG_LEVEL_DEBUG = 3,
};

void log_init(void);
void log_write(enum log_level level, const char *tag, const char *fmt, ...)
	__attribute__((format(printf, 3, 4)));

#define LOG_ERROR(fmt, ...) \
	log_write(LOG_LEVEL_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) \
	log_write(LOG_LEVEL_WARN,  LOG_TAG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) \
	log_write(LOG_LEVEL_INFO,  LOG_TAG, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) \
	log_write(LOG_LEVEL_DEBUG, LOG_TAG, fmt, ##__VA_ARGS__)

#endif /* _LOG_H */
