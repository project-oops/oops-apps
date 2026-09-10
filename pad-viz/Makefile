# pad-viz - a live controller diagram.
#
#   make check      render several controller states into a host buffer and verify (any machine)
#   make skeleton   the drawing compiles freestanding for the target, object only
#   make elf        the plain-ELF payload a homebrew loader (elfldr) runs on the console
#
# The drawing (pad-viz.c) is shared between the host test and the payload. Only pad-viz_main.c
# is console-only: the display, the batched pad read, and the loop.

OOPS_SDK ?= $(abspath ../../oops-sdk)
include $(OOPS_SDK)/oops-sdk.mk

# The host test links only the drawing; the display is not touched (render takes a surface).
SDK_HOST_SRCS := $(OOPS_SDK_DIR)/src/draw/draw.c

# The SDK sources this payload uses - display, input, drawing, and the payload runtime.
SDK_ELF_SRCS := $(OOPS_SDK_DIR)/src/display.c \
                $(OOPS_SDK_DIR)/src/agc/agc_display.c $(OOPS_SDK_DIR)/src/agc/agc_tiler.c \
                $(OOPS_SDK_DIR)/src/gnm/gnm_display.c $(OOPS_SDK_DIR)/src/input/input.c \
                $(OOPS_SDK_DIR)/src/memory/memory.c $(OOPS_SDK_DIR)/src/system/system.c \
                $(OOPS_SDK_DIR)/src/system/offsets.c $(OOPS_SDK_DIR)/src/system/freestd.c \
                $(OOPS_SDK_DIR)/src/system/syscall.c $(OOPS_SDK_DIR)/src/system/krw.c \
                $(OOPS_SDK_DIR)/src/draw/draw.c

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

all: $(BUILD)/pad-viz_selftest skeleton elf

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/pad-viz_selftest: pad-viz_selftest.c pad-viz.c pad-viz.h $(SDK_HOST_SRCS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ pad-viz_selftest.c pad-viz.c $(SDK_HOST_SRCS)

check: $(BUILD)/pad-viz_selftest
	@./$(BUILD)/pad-viz_selftest

skeleton: | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $(BUILD)/pad-viz.o pad-viz.c
	@echo "pad-viz skeleton: compiles freestanding for the target (object only)"

# The full payload: the app plus the SDK sources it uses, freestanding for the target. The
# vendor imports (sce*) are left unresolved here and bound at load by the loader on the console.
$(BUILD)/pad-viz.elf: pad-viz.c pad-viz_main.c pad-viz.h $(SDK_ELF_SRCS) | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) -fuse-ld=lld -shared -Wl,-e,padviz_start \
	    -Wl,--unresolved-symbols=ignore-all -Wl,-z,noexecstack \
	    -Wl,-z,max-page-size=0x4000 -Wl,-z,common-page-size=0x4000 \
	    -o $@ pad-viz.c pad-viz_main.c $(SDK_ELF_SRCS)

elf: $(BUILD)/pad-viz.elf

# The release contract (see bin/oops-apps): put what ships under dist/.
# Staged artifacts explicitly encode target generation per CONVENTIONS.md section 2.
dist: $(BUILD)/pad-viz.elf
	@mkdir -p $(DIST)
	cp $(BUILD)/pad-viz.elf $(DIST)/pad-viz-$(TARGET).elf
	@$(call oops_verify_dist,$(DIST))

clean:
	rm -rf $(BUILD) $(DIST)
