# notifier - a line from homebrew on the system notification overlay.
#
#   make check      compose the status line on a host and verify its fields and bounds
#   make skeleton   compose compiles freestanding for the target, object only
#
# `all` is the host test and the skeleton. A full on-console payload fires the notification and
# the dialog and links the SDK's oops/krw.h, which is blocked by an empty oops/freestd.h in the
# SDK - the base-layer promotion left it unpopulated. No `elf` target here until that is filled.

OOPS_SDK ?= $(abspath ../../oops-sdk)
include $(OOPS_SDK)/oops-sdk.mk

CC ?= clang
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
          -Wstrict-prototypes -Wmissing-prototypes -O1 $(OOPS_SDK_INCLUDE)

TARGET_CC ?= clang
TARGET_CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
                 -Wstrict-prototypes -Wmissing-prototypes -Wvla \
                 -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib -fPIC \
                 -fno-stack-protector -fvisibility=hidden $(OOPS_SDK_INCLUDE)

BUILD ?= build

.PHONY: all check skeleton clean

all: $(BUILD)/notifier_selftest skeleton

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/notifier_selftest: notifier_selftest.c notifier.c notifier.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ notifier_selftest.c notifier.c

check: $(BUILD)/notifier_selftest
	@./$(BUILD)/notifier_selftest

skeleton: | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $(BUILD)/notifier.o notifier.c
	@echo "notifier skeleton: compiles freestanding for the target (object only)"

clean:
	rm -rf $(BUILD)
