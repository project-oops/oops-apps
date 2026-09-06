# save-browser - the saves on the machine, listed.
#
#   make check      draw a made-up list and move the cursor within bounds (runs anywhere)
#   make skeleton   the list logic compiles freestanding for the target, object only
#
# `all` is the host test and the skeleton. A full on-console payload mounts a real disk and
# links the SDK's oops/krw.h, which is blocked by an empty oops/freestd.h in the SDK - the
# base-layer promotion left it unpopulated. No `elf` target here until that header is filled.

OOPS_SDK ?= $(abspath ../../oops-sdk)
include $(OOPS_SDK)/oops-sdk.mk

SDK_HOST_SRCS := $(OOPS_SDK_DIR)/src/draw/draw.c

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

all: $(BUILD)/save_browser_selftest skeleton

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/save_browser_selftest: save-browser_selftest.c save-browser.c save-browser.h $(SDK_HOST_SRCS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ save-browser_selftest.c save-browser.c $(SDK_HOST_SRCS)

check: $(BUILD)/save_browser_selftest
	@./$(BUILD)/save_browser_selftest

skeleton: | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $(BUILD)/save-browser.o save-browser.c
	@echo "save-browser skeleton: compiles freestanding for the target (object only)"

clean:
	rm -rf $(BUILD)
