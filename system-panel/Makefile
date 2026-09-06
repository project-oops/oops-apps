# System panel - what the machine is, drawn to the screen.
#
# The smallest thing that proves the SDK reaches hardware end to end: it asks the system what
# it is, draws the answer, and waits for a button. It depends on oops-sdk and nothing else.
#
#   make check      render the panel into a host buffer and verify it drew (runs anywhere)
#   make skeleton   the drawing compiles freestanding for the target, object only
#
# The full on-console payload - a display opened, an input loop, an exit - waits on the base
# runtime (a crt0, freestanding libc, syscalls) landing in oops-sdk, the same promotion every
# app here is waiting for. See ../docs/DECISIONS.md (D002).

OOPS_SDK ?= $(abspath ../../oops-sdk)
include $(OOPS_SDK)/oops-sdk.mk

# The SDK sources this app actually links: the drawing, and the system query. Named rather
# than pulling the whole SDK, so the host test stays small and its link is obvious.
SDK_HOST_SRCS := $(OOPS_SDK_DIR)/src/draw/draw.c \
                 $(OOPS_SDK_DIR)/src/system/system.c

CC ?= clang
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
          -Wstrict-prototypes -Wmissing-prototypes -O1 $(OOPS_SDK_INCLUDE)

# The target skeleton cross-compiles for freebsd, which needs clang specifically.
TARGET_CC ?= clang
TARGET_CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
                 -Wstrict-prototypes -Wmissing-prototypes -Wvla \
                 -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib -fPIC \
                 -fno-stack-protector -fvisibility=hidden $(OOPS_SDK_INCLUDE)

BUILD ?= build

.PHONY: all check skeleton clean

all: $(BUILD)/system_panel_selftest skeleton

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/system_panel_selftest: system_panel_selftest.c system_panel.c system_panel.h $(SDK_HOST_SRCS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ system_panel_selftest.c system_panel.c $(SDK_HOST_SRCS)

# The gate: the panel renders into a host buffer and something actually drew.
check: $(BUILD)/system_panel_selftest
	@./$(BUILD)/system_panel_selftest

# Proves the drawing compiles freestanding for the target, object-only (no CRT/link yet).
skeleton: | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $(BUILD)/system_panel.o system_panel.c
	@echo "system-panel skeleton: compiles freestanding for the target (object only)"

clean:
	rm -rf $(BUILD)
