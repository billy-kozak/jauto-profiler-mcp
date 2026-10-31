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
#include "testing.h"

#include "master-instruments.h"
#include "queued-instrument.h"

#include <string.h>

TEST_CASE(mi_var_bind_add_test)
{
	struct master_instruments *mi = mi_alloc();
	TEST_ASSERT(mi != NULL);

	struct mi_val v = mi_add_var_bind(mi, "com/Foo", "bar", 42);
	TEST_ASSERT(v.id != 0);

	const struct mi_entry *e = mi_find(mi, v.id);
	TEST_ASSERT(e != NULL);
	TEST_ASSERT(e->type == MI_VAR_BIND);
	TEST_ASSERT(strcmp((char *)e->entry_class_name->str, "com/Foo") == 0);
	TEST_ASSERT(strcmp((char *)e->var_bind.var_name->str, "bar") == 0);
	TEST_ASSERT(e->var_bind.line_number == 42);
	TEST_ASSERT(e->var_bind.deferred);

	TEST_ASSERT(mi_mark_var_bind_applied(mi, v.id) == 0);
	TEST_ASSERT(!e->var_bind.deferred);

	mi_remove(mi, v.id);
	TEST_ASSERT(mi_find(mi, v.id) == NULL);

	mi_free(mi);
	return 0;
}

TEST_CASE(mi_var_bind_mark_wrong_type_test)
{
	struct master_instruments *mi = mi_alloc();
	TEST_ASSERT(mi != NULL);

	struct mi_val m = mi_add_method(mi, "com/Foo", "com/Foo", "run()V");
	struct mi_val v = mi_add_var_bind(mi, "com/Foo", "bar", 42);

	TEST_ASSERT(mi_mark_var_bind_applied(mi, m.id) != 0);
	TEST_ASSERT(mi_mark_method_applied(mi, v.id) != 0);
	TEST_ASSERT(mi_mark_line_entry_applied(mi, v.id) != 0);
	TEST_ASSERT(mi_mark_line_exit_applied(mi, v.id) != 0);

	mi_free(mi);
	return 0;
}

TEST_CASE(mi_var_bind_not_visible_test)
{
	struct master_instruments *mi = mi_alloc();
	TEST_ASSERT(mi != NULL);

	mi_add_method(mi, "com/Foo", "com/Foo", "bar");
	mi_add_line(mi, "com/Foo", "com/Foo", 10, 20);
	mi_add_var_bind(mi, "com/Foo", "bar", 42);

	TEST_ASSERT(mi_type_is_user_visible(MI_METHOD));
	TEST_ASSERT(mi_type_is_user_visible(MI_LINE));
	TEST_ASSERT(!mi_type_is_user_visible(MI_VAR_BIND));

	/* A var bind sharing a method's name must not match a method lookup */
	struct mi_val found = mi_find_method_by_name(mi, "com/Foo", "bar");
	TEST_ASSERT(found.entry != NULL);
	TEST_ASSERT(found.entry->type == MI_METHOD);

	struct mi_itr itr;
	struct mi_val r;
	int visible = 0;
	mi_itr_init(mi, &itr);
	while ((r = mi_iterate(mi, &itr)).entry != NULL) {
		if (mi_type_is_user_visible(r.entry->type)) {
			visible++;
		}
	}
	TEST_ASSERT(visible == 2);

	mi_free(mi);
	return 0;
}

TEST_CASE(qi_var_bind_add_test)
{
	struct queued_instr_list list;
	TEST_ASSERT(queued_instr_list_init(&list, 0) == 0);

	queued_instr_method_add(&list, "com/Foo", "run()V", 3, 1);
	struct queued_instr *qi = queued_instr_var_bind_add(
		&list, "com/Bar", "baz", 7, 2
	);
	TEST_ASSERT(qi != NULL);
	TEST_ASSERT(qi->type == QI_VAR_BIND);
	TEST_ASSERT(qi->profiler_id == -1);
	TEST_ASSERT(strcmp((char *)qi->var_bind.var_name->str, "baz") == 0);
	TEST_ASSERT(qi->var_bind.line_number == 7);

	TEST_ASSERT(queued_instr_list_find_by_class(&list, "com/Bar") == 1);
	TEST_ASSERT(queued_instr_list_find_by_instrument_id(&list, 2) == 1);

	queued_instr_list_remove_and_destroy(&list, 1);
	TEST_ASSERT(queued_instr_list_find_by_instrument_id(&list, 2) < 0);

	queued_instr_var_bind_add(&list, "com/Bar", "baz", 7, 2);
	queued_instr_list_deep_destroy(&list);
	return 0;
}

TEST_SUITE(
	instrument_ds_suite,
	mi_var_bind_add_test,
	mi_var_bind_mark_wrong_type_test,
	mi_var_bind_not_visible_test,
	qi_var_bind_add_test
)
