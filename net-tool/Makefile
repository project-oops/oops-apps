# net-tool - what the network is, and a socket you can talk to.
#
#   make check      render the panel and round-trip an address on a host (runs anywhere)
#   make skeleton   the drawing compiles freestanding for the target, object only
#
# `all` is the host test and the skeleton, both of which build today. A full on-console payload
# (the echo server, the display loop) links the SDK's oops/krw.h, which is blocked by an empty
# oops/freestd.h in the SDK - the base-layer promotion left it unpopulated. There is no `elf`
# target here until that header is filled in.

OOPS_SDK ?= $(abspath ../../oops-sdk)
include $(OOPS_SDK)/oops-sdk.mk

# The SDK sources the host test links: the drawing, and the inet helpers (pure arithmetic).
SDK_HOST_SRCS := $(OOPS_SDK_DIR)/src/draw/draw.c \
                 $(OOPS_SDK_DIR)/src/net/net.c

SDK_ELF_SRCS := $(OOPS_SDK_DIR)/src/display.c $(OOPS_SDK_DIR)/src/agc/agc_display.c \
                $(OOPS_SDK_DIR)/src/agc/agc_tiler.c $(OOPS_SDK_DIR)/src/gnm/gnm_display.c \
                $(OOPS_SDK_DIR)/src/input/input.c $(OOPS_SDK_DIR)/src/memory/memory.c \
                $(OOPS_SDK_DIR)/src/net/net.c $(OOPS_SDK_DIR)/src/net/netctl.c \
                $(OOPS_SDK_DIR)/src/system/system.c $(OOPS_SDK_DIR)/src/system/offsets.c \
                $(OOPS_SDK_DIR)/src/system/freestd.c $(OOPS_SDK_DIR)/src/system/syscall.c \
                $(OOPS_SDK_DIR)/src/system/krw.c $(OOPS_SDK_DIR)/src/draw/draw.c

CC ?= clang
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
          -Wstrict-prototypes -Wmissing-prototypes -O1 $(OOPS_SDK_INCLUDE)

TARGET_CC ?= clang
TARGET_CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
                 -Wstrict-prototypes -Wmissing-prototypes -Wvla \
                 -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib -fPIC \
                 -fno-stack-protector -fvisibility=hidden $(OOPS_SDK_INCLUDE)

BUILD ?= build
DIST ?= dist

.PHONY: all check skeleton elf dist clean

all: $(BUILD)/net_tool_selftest skeleton elf

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/net_tool_selftest: net-tool_selftest.c net-tool.c net-tool.h $(SDK_HOST_SRCS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ net-tool_selftest.c net-tool.c $(SDK_HOST_SRCS)

check: $(BUILD)/net_tool_selftest
	@./$(BUILD)/net_tool_selftest

skeleton: | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $(BUILD)/net-tool.o net-tool.c
	@echo "net-tool skeleton: compiles freestanding for the target (object only)"

$(BUILD)/net-tool.elf: net-tool.c net-tool_main.c net-tool.h $(SDK_ELF_SRCS) | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) -fuse-ld=lld -shared -Wl,-e,net_tool_start \
	    -Wl,--unresolved-symbols=ignore-all -Wl,-z,noexecstack \
	    -Wl,-z,max-page-size=0x4000 -Wl,-z,common-page-size=0x4000 \
	    -o $@ net-tool.c net-tool_main.c $(SDK_ELF_SRCS)

elf: $(BUILD)/net-tool.elf

# The release contract (see bin/oops-apps): put what ships under dist/.
# Staged artifacts explicitly encode target generation per CONVENTIONS.md section 2.
dist: $(BUILD)/net-tool.elf
	@mkdir -p $(DIST)
	cp $(BUILD)/net-tool.elf $(DIST)/net-tool-$(TARGET).elf
	@$(call oops_verify_dist,$(DIST))

clean:
	rm -rf $(BUILD) $(DIST)
