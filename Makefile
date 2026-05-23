#!/usr/bin/make -f
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
#                                    VARS                                     #
###############################################################################
CC := gcc
LD := gcc

CFLAGS += -Wall -std=gnu11
LDFLAGS += -Wall

CFLAGS += -fvisibility=hidden -fPIC -ffunction-sections
LDFLAGS += -shared -fPIC -Wl,--gc-sections

NO_DEPS_TARGETS += clean directories dir_clean

JAVA_HOME = $(shell build-src/find-java-home)
JAVA_INC = -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux

JAVAC := $(JAVA_HOME)/bin/javac
JAVACFLAGS :=
JAR := $(JAVA_HOME)/bin/jar

LIBS := -lpthread
###############################################################################
#                                 BUILD DIRS                                  #
###############################################################################
BUILD_DIR := build
OBJ_DIR := build/c/obj
DEP_DIR := build/c/deps
JAVA_BUILD_DIR := build/java
JAR_FILE := $(BUILD_DIR)/jauto-prof-lib.jar

OUTPUT_DIR := bin
OUT_LIB := $(OUTPUT_DIR)/jauto-profiler.so

CSRC_ROOT = c-src
CSRC_DIRS = $(shell find $(SRC_ROOT) -type d)

NO_DEPS_TARGETS += clean directories
###############################################################################
#                                 BUILD FILES                                 #
###############################################################################
C_FILES = $(shell find $(CSRC_ROOT) -name "*.c")
O_FILES = $(C_FILES:$(CSRC_ROOT)/%.c=$(OBJ_DIR)/%.o)
D_FILES = $(C_FILES:$(CSRC_ROOT)/%.c=$(DEP_DIR)/%.d)

JSRC_ROOT = java-src/main/java
JAVA_FILES = $(shell find $(JSRC_ROOT) -name "*.java")
CLASS_FILES = $(JAVA_FILES:$(JSRC_ROOT)/%.java=$(JAVA_BUILD_DIR)/%.class)

vpath %.c $(CSRC_DIRS)

INC = $(JAVA_INC) -I$(CSRC_ROOT)

CLEAN_FILES += $(wildcard $(BUILD_DIR)/*)
CLEAN_FILES += $(wildcard $(OUTPUT_DIR)/*)
###############################################################################
#                                   MACROS                                    #
###############################################################################
define d_to_o
$(patsubst $(DEP_DIR)/%.d,$(OBJ_DIR)/%.o,$(1))
endef

define d_to_c
$(patsubst $(DEP_DIR)/%.d,$(OBJ_DIR)/%.c,$(1))
endef
###############################################################################
#                                 BUILD FILES                                 #
###############################################################################
all: optimized

optimized: CFLAGS +=-Os -flto=auto
optomized: LDFLAGS += -Os -flto=auto
optimized: shared java

debug: CFLAGS +=-O0
debug: LDFLAGS += -O0
debug: shared java

shared: $(D_FILES) $(OUT_LIB)

$(D_FILES): $(DEP_DIR)/%.d: $(CSRC_ROOT)/%.c
	@mkdir -p $(dir $(@))
	$(CC) $(INC) -MF $@ -M -MT "$(call d_to_o,$(@))" $<

$(O_FILES): $(OBJ_DIR)/%.o: $(CSRC_ROOT)/%.c
	@mkdir -p $(dir $(@))
	$(CC) $(INC) $(CFLAGS) -c $< -o $@

$(OUT_LIB): $(O_FILES) $(OUTPUT_DIR)/.dir_dummy
	$(LD) $(LDFLAGS) $(O_FILES) $(LIBS) -o $@

%.dir_dummy:
	mkdir -p $(dir $(@))
	@touch $(@)

java: $(JAR_FILE)

$(JAR_FILE): $(CLASS_FILES) $(OUTPUT_DIR)/.dir_dummy
	$(JAR) cf $@ -C $(JAVA_BUILD_DIR) .
	cp $@ $(OUTPUT_DIR)/

$(CLASS_FILES): $(JAVA_BUILD_DIR)/%.class: $(JSRC_ROOT)/%.java
	@mkdir -p $(dir $(@))
	$(JAVAC) $(JAVACFLAGS) -d $(JAVA_BUILD_DIR) -sourcepath $(JSRC_ROOT) $<

.PHONY: clean
clean:
	rm -rf $(CLEAN_FILES)

ifeq (,$(filter $(NO_DEPS_TARGETS), $(MAKECMDGOALS)))
#next two conditonals prevent make from running on dry run or when
#invoked for tab-completion
ifneq (n,$(findstring n,$(firstword $(MAKEFLAGS))))
ifneq (p,$(findstring p,$(firstword $(MAKEFLAGS))))
-include $(D_FILES)
endif
endif
endif
