# SPDX-License-Identifier: MIT
CLANG ?= clang
JAVAC ?= javac
JAVA ?= java
BUILD ?= build
CFLAGS ?= -std=c11 -O2 -g -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -Iinclude -Iadapters
JFLAGS ?= -Xlint:all -Werror

CORE_JAVA = $(wildcard android/core/src/main/java/io/github/p1r4n351/secondscreen/core/*.java)
C_BINS = $(BUILD)/t_companion_waist $(BUILD)/t_companion_verbs $(BUILD)/t_companion_probe $(BUILD)/t_companion_trail $(BUILD)/t_companion_seq

.PHONY: all test clean

all: $(C_BINS) $(BUILD)/classes/JvmShellTest.class

$(BUILD):
	mkdir -p $(BUILD)/classes

$(BUILD)/t_companion_waist: tests/t_companion_waist.c include/nx_companion/companion_state.h | $(BUILD)
	$(CLANG) $(CFLAGS) -o $@ $<

$(BUILD)/t_companion_verbs: tests/t_companion_verbs.c include/nx_companion/companion_verbs.h include/nx_companion/companion_state.h | $(BUILD)
	$(CLANG) $(CFLAGS) -o $@ $<

$(BUILD)/t_companion_probe: tests/t_companion_probe.c include/nx_companion/companion_probe.h include/nx_companion/companion_state.h adapters/oot_sw2/oot_sw2_adapter.h adapters/oot_sw2/oot_sw2_catalog.h adapters/ktgl/ktgl_adapter.h | $(BUILD)
	$(CLANG) $(CFLAGS) -o $@ $< -lm

$(BUILD)/t_companion_trail: tests/t_companion_trail.c include/nx_companion/companion_trail.h | $(BUILD)
	$(CLANG) $(CFLAGS) -o $@ $< -lm

$(BUILD)/t_companion_seq: tests/t_companion_seq.c include/nx_companion/companion_seq.h adapters/oot_sw2/oot_sw2_catalog.h | $(BUILD)
	$(CLANG) $(CFLAGS) -o $@ $<

$(BUILD)/classes/JvmShellTest.class: $(CORE_JAVA) tests/JvmShellTest.java | $(BUILD)
	$(JAVAC) $(JFLAGS) -d $(BUILD)/classes $(CORE_JAVA) tests/JvmShellTest.java

test: all
	$(BUILD)/t_companion_waist
	$(BUILD)/t_companion_verbs
	$(BUILD)/t_companion_probe
	$(BUILD)/t_companion_trail
	$(BUILD)/t_companion_seq
	$(JAVA) -ea -cp $(BUILD)/classes JvmShellTest

clean:
	rm -rf $(BUILD)
