###############################################################################
# Copyright (C) 2026  Billy Kozak                                             #
#                                                                             #
# This file is part of the jauto-profiler program                             #
#                                                                             #
# This program is free software: you can redistribute it and/or modify        #
# it under the terms of the GNU Lesser General Public License as published by #
# the Free Software Foundation, either version 3 of the License, or           #
# (at your option) any later version.                                         #
#                                                                             #
# This program is distributed in the hope that it will be useful,             #
# but WITHOUT ANY WARRANTY; without even the implied warranty of              #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               #
# GNU Lesser General Public License for more details.                         #
#                                                                             #
# You should have received a copy of the GNU Lesser General Public License    #
# along with this program.  If not, see <http://www.gnu.org/licenses/>.       #
###############################################################################
###############################################################################
#                                 TEST VARS                                   #
###############################################################################
CSRC_TEST = c-src/test
OUT_TEST := $(OUTPUT_DIR)/prof-server-tests

LDTEST_FLAGS += -Wall -fPIC

C_TEST_FILES = $(shell find $(CSRC_TEST) -name "*.c")
O_TEST_FILES = $(C_TEST_FILES:$(CSRC_ROOT)/%.c=$(OBJ_DIR)/%.o)
D_TEST_FILES = $(C_TEST_FILES:$(CSRC_ROOT)/%.c=$(DEP_DIR)/%.d)

CTEST_INC = $(INC) -I$(CSRC_TEST)
###############################################################################
#                               TEST TARGETS                                  #
###############################################################################
test: $(D_TEST_FILES) $(OUT_TEST)

$(D_TEST_FILES): $(DEP_DIR)/%.d: $(CSRC_ROOT)/%.c
	@mkdir -p $(dir $(@))
	$(CC) $(CTEST_INC) -MF $@ -M -MT "$(call d_to_o,$(@))" $<

$(O_TEST_FILES): $(OBJ_DIR)/%.o: $(CSRC_ROOT)/%.c
	@mkdir -p $(dir $(@))
	$(CC) $(CTEST_INC) $(CFLAGS) -c $< -o $@

$(OUT_TEST): $(O_TEST_FILES) $(O_FILES) $(OUTPUT_DIR)/.dir_dummy
	$(LD) $(LDTEST_FLAGS) $(O_TEST_FILES) $(O_FILES) $(LIBS) -o $@

ifeq (,$(filter $(NO_DEPS_TARGETS), $(MAKECMDGOALS)))
#next two conditonals prevent make from running on dry run or when
#invoked for tab-completion
ifneq (n,$(findstring n,$(firstword $(MAKEFLAGS))))
ifneq (p,$(findstring p,$(firstword $(MAKEFLAGS))))
-include $(D_TEST_FILES)
endif
endif
endif
