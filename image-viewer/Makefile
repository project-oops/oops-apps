# image-viewer - show an image, fit to the screen.
#
#   make check      fit a made-up image into a host buffer and verify the placement
#   make skeleton   the fit-and-blit compiles freestanding for the target, object only
#
# `all` is the host test and the skeleton. A full on-console payload reads and decodes a file
# and links the SDK's oops/krw.h, which is blocked by an empty oops/freestd.h in the SDK - the
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

all: $(BUILD)/image_viewer_selftest skeleton

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/image_viewer_selftest: image-viewer_selftest.c image-viewer.c image-viewer.h $(SDK_HOST_SRCS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ image-viewer_selftest.c image-viewer.c $(SDK_HOST_SRCS)

check: $(BUILD)/image_viewer_selftest
	@./$(BUILD)/image_viewer_selftest

skeleton: | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $(BUILD)/image-viewer.o image-viewer.c
	@echo "image-viewer skeleton: compiles freestanding for the target (object only)"

clean:
	rm -rf $(BUILD)
