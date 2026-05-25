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

#include "bytecode.h"
#include "class-info.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CP_UTF8            1
#define CP_INTEGER         3
#define CP_FLOAT           4
#define CP_LONG            5
#define CP_DOUBLE          6
#define CP_CLASS           7
#define CP_STRING          8
#define CP_FIELDREF        9
#define CP_METHODREF      10
#define CP_IFACE_MREF     11
#define CP_NAME_AND_TYPE  12
#define CP_METHOD_HANDLE  15
#define CP_METHOD_TYPE    16
#define CP_DYNAMIC        17
#define CP_INVOKE_DYNAMIC 18
#define CP_MODULE         19
#define CP_PACKAGE        20

static uint16_t read_u2(const unsigned char *p)
{
	return ((uint16_t)p[0] << 8) | p[1];
}

static uint32_t read_u4(const unsigned char *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
		((uint32_t)p[2] << 8) | p[3];
}

/*
 * Parse one constant pool entry body (after the tag byte already consumed).
 * If tag is CP_UTF8 and utf8_out is non-NULL, stores a malloc'd copy there.
 * Returns bytes consumed, or -1 on error.
 */
static int parse_cp_body(
	const unsigned char *data,
	size_t remaining,
	uint8_t tag,
	const char **utf8_out
) {
	if (utf8_out != NULL) {
		*utf8_out = NULL;
	}

	switch (tag) {
	case CP_UTF8: {
		uint16_t len;

		if (remaining < 2) {
			return -1;
		}
		len = read_u2(data);
		if (remaining < (size_t)(2 + len)) {
			return -1;
		}
		if (utf8_out != NULL) {
			char *s = malloc(len + 1);
			if (s == NULL) {
				return -1;
			}
			memcpy(s, data + 2, len);
			s[len] = '\0';
			*utf8_out = s;
		}
		return 2 + len;
	}
	case CP_INTEGER:
	case CP_FLOAT:
		if (remaining < 4) {
			return -1;
		}
		return 4;
	case CP_LONG:
	case CP_DOUBLE:
		if (remaining < 8) {
			return -1;
		}
		return 8;
	case CP_CLASS:
	case CP_STRING:
	case CP_METHOD_TYPE:
	case CP_MODULE:
	case CP_PACKAGE:
		if (remaining < 2) {
			return -1;
		}
		return 2;
	case CP_FIELDREF:
	case CP_METHODREF:
	case CP_IFACE_MREF:
	case CP_NAME_AND_TYPE:
	case CP_DYNAMIC:
	case CP_INVOKE_DYNAMIC:
		if (remaining < 4) {
			return -1;
		}
		return 4;
	case CP_METHOD_HANDLE:
		if (remaining < 3) {
			return -1;
		}
		return 3;
	default:
		return -1;
	}
}

static int skip_attributes(
	const unsigned char *data,
	size_t data_len,
	size_t *pos
) {
	uint16_t attr_count;
	uint16_t i;

	if (*pos + 2 > data_len) {
		return -1;
	}
	attr_count = read_u2(data + *pos);
	*pos += 2;

	for (i = 0; i < attr_count; i++) {
		uint32_t attr_len;

		/* attribute_name_index(2) + attribute_length(4) */
		if (*pos + 6 > data_len) {
			return -1;
		}
		attr_len = read_u4(data + *pos + 2);
		*pos += 6 + attr_len;
		if (*pos > data_len) {
			return -1;
		}
	}
	return 0;
}

int bc_extract_methods(
	const unsigned char *data,
	size_t data_len,
	struct method_list *methods
) {
	size_t pos;
	uint16_t cp_count;
	const char **utf8_pool = NULL;
	uint16_t iface_count;
	uint16_t field_count;
	uint16_t method_count;
	int i;

	/* magic(4) + minor_version(2) + major_version(2) */
	if (data_len < 8) {
		return -1;
	}
	pos = 8;

	if (pos + 2 > data_len) {
		return -1;
	}
	cp_count = read_u2(data + pos);
	pos += 2;

	if (cp_count < 1) {
		return -1;
	}

	utf8_pool = calloc(cp_count, sizeof(*utf8_pool));
	if (utf8_pool == NULL) {
		return -1;
	}

	/* constant pool entries are indexed 1..cp_count-1 */
	for (i = 1; i < (int)cp_count; i++) {
		uint8_t tag;
		int consumed;

		if (pos >= data_len) {
			goto fail;
		}
		tag = data[pos++];

		consumed = parse_cp_body(
			data + pos,
			data_len - pos,
			tag,
			(tag == CP_UTF8) ? &utf8_pool[i] : NULL
		);
		if (consumed < 0) {
			goto fail;
		}
		pos += (size_t)consumed;

		/* Long and Double occupy two constant pool slots */
		if (tag == CP_LONG || tag == CP_DOUBLE) {
			i++;
		}
	}

	/* access_flags(2) + this_class(2) + super_class(2) */
	if (pos + 6 > data_len) {
		goto fail;
	}
	pos += 6;

	/* interfaces_count + interfaces */
	if (pos + 2 > data_len) {
		goto fail;
	}
	iface_count = read_u2(data + pos);
	pos += 2;
	if (pos + (size_t)(2 * iface_count) > data_len) {
		goto fail;
	}
	pos += 2 * iface_count;

	/* fields: access(2) + name(2) + descriptor(2) + attributes */
	if (pos + 2 > data_len) {
		goto fail;
	}
	field_count = read_u2(data + pos);
	pos += 2;
	for (i = 0; i < (int)field_count; i++) {
		if (pos + 6 > data_len) {
			goto fail;
		}
		pos += 6;
		if (skip_attributes(data, data_len, &pos) != 0) {
			goto fail;
		}
	}

	/* methods: access(2) + name(2) + descriptor(2) + attributes */
	if (pos + 2 > data_len) {
		goto fail;
	}
	method_count = read_u2(data + pos);
	pos += 2;

	for (i = 0; i < (int)method_count; i++) {
		uint16_t name_idx;
		uint16_t desc_idx;
		const char *name;
		const char *desc;
		size_t name_len;
		size_t desc_len;

		if (pos + 6 > data_len) {
			goto fail_methods;
		}
		pos += 2; /* access_flags */
		name_idx = read_u2(data + pos);
		pos += 2;
		desc_idx = read_u2(data + pos);
		pos += 2;

		if (skip_attributes(data, data_len, &pos) != 0) {
			goto fail_methods;
		}

		if (name_idx >= cp_count || desc_idx >= cp_count) {
			continue;
		}
		name = utf8_pool[name_idx];
		desc = utf8_pool[desc_idx];
		if (name == NULL || desc == NULL) {
			continue;
		}

		name_len = strlen(name);
		desc_len = strlen(desc);
		{
			size_t str_len = name_len + 1 + desc_len;
			struct pstring *ps = malloc(sizeof(*ps) + str_len);
			struct pstring **slot;

			if (ps == NULL) {
				goto fail_methods;
			}
			ps->size = (uint16_t)str_len;
			memcpy(ps->str, name, name_len);
			ps->str[name_len] = ':';
			memcpy(ps->str + name_len + 1, desc, desc_len);

			slot = method_list_add(methods);
			if (slot == NULL) {
				free(ps);
				goto fail_methods;
			}
			*slot = ps;
		}
	}

	for (i = 0; i < (int)cp_count; i++) {
		free((char *)utf8_pool[i]);
	}
	free(utf8_pool);
	return 0;

fail_methods:
	{
		size_t j;
		for (j = 0; j < methods->len; j++) {
			free(methods->arr[j]);
		}
		methods->len = 0;
	}
fail:
	for (i = 0; i < (int)cp_count; i++) {
		free((char *)utf8_pool[i]);
	}
	free(utf8_pool);
	return -1;
}
