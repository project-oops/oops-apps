# gallery - one page per SDK subsystem, driven by hand.
#
#   make check      render every page into a host buffer and verify it drew (runs anywhere)
#   make skeleton   the drawing compiles freestanding for the target, object only
#   make elf        the plain-ELF payload a homebrew loader (elfldr) runs on the console
#
# The drawing (gallery.c) is shared between the host test and the payload. Only gallery_main.c
# is console-only: the display, the pad, and the loop.

OOPS_SDK ?= $(abspath ../../oops-sdk)
include $(OOPS_SDK)/oops-sdk.mk

# The SDK sources the host test links: the drawing and the system query. The display stack is
# stubbed in the test, so it is not linked here.
SDK_HOST_SRCS := $(OOPS_SDK_DIR)/src/draw/draw.c \
                 $(OOPS_SDK_DIR)/src/system/system.c

# The SDK sources this payload actually uses - display, input, drawing, system, and the
# payload runtime. Only these, so the link does not drag in SDK sources unrelated to a
# gallery (and the ones still being written).
SDK_ELF_SRCS := $(OOPS_SDK_DIR)/src/display.c \
                $(OOPS_SDK_DIR)/src/agc/agc_display.c $(OOPS_SDK_DIR)/src/agc/agc_tiler.c \
                $(OOPS_SDK_DIR)/src/gnm/gnm_display.c $(OOPS_SDK_DIR)/src/input/input.c \
                $(OOPS_SDK_DIR)/src/input/keyboard.c $(OOPS_SDK_DIR)/src/input/mouse.c \
                $(OOPS_SDK_DIR)/src/audio/audiodec.c $(OOPS_SDK_DIR)/src/videodec/videodec.c \
                $(OOPS_SDK_DIR)/src/system/sysmodule.c \
                $(OOPS_SDK_DIR)/src/memory/memory.c $(OOPS_SDK_DIR)/src/system/system.c \
                $(OOPS_SDK_DIR)/src/system/offsets.c $(OOPS_SDK_DIR)/src/system/freestd.c \
                $(OOPS_SDK_DIR)/src/system/syscall.c $(OOPS_SDK_DIR)/src/system/krw.c \
                $(OOPS_SDK_DIR)/src/draw/draw.c

CC ?= clang
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
          -Wstrict-prototypes -Wmissing-prototypes -O1 -DOOPS_HOST_BUILD $(OOPS_SDK_INCLUDE)

TARGET_CC ?= clang
TARGET_CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
                 -Wstrict-prototypes -Wmissing-prototypes -Wvla \
                 -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib -fPIC \
                 -fno-stack-protector -fvisibility=hidden $(OOPS_SDK_INCLUDE)

BUILD ?= build
DIST ?= dist

.PHONY: all check skeleton elf dist clean

# check and skeleton run on any machine; elf is the on-console payload. All three build.
# The elf links only the SDK sources a gallery uses (SDK_ELF_SRCS), so it does not drag in the
# SDK modules still being written elsewhere.
all: $(BUILD)/gallery_selftest skeleton elf

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/gallery_selftest: gallery_selftest.c gallery.c gallery.h $(SDK_HOST_SRCS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ gallery_selftest.c gallery.c $(SDK_HOST_SRCS)

check: $(BUILD)/gallery_selftest
	@./$(BUILD)/gallery_selftest

skeleton: | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $(BUILD)/gallery.o gallery.c
	@echo "gallery skeleton: compiles freestanding for the target (object only)"

# The full payload: the app plus the SDK sources it uses, freestanding for the target. The
# vendor imports (sce*) are left unresolved here and bound at load by the loader on the
# console, which is why an on-console payload links against no vendor library.
$(BUILD)/gallery.elf: gallery.c gallery_main.c gallery.h $(SDK_ELF_SRCS) | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) -fuse-ld=lld -shared -Wl,-e,gallery_start \
	    -Wl,--unresolved-symbols=ignore-all -Wl,-z,noexecstack \
	    -Wl,-z,max-page-size=0x4000 -Wl,-z,common-page-size=0x4000 \
	    -o $@ gallery.c gallery_main.c $(SDK_ELF_SRCS)

elf: $(BUILD)/gallery.elf

# The release contract (see bin/oops-apps): put what ships under dist/.
# Staged artifacts explicitly encode target generation per CONVENTIONS.md section 2.
dist: $(BUILD)/gallery.elf
	@mkdir -p $(DIST)
	cp $(BUILD)/gallery.elf $(DIST)/gallery-$(TARGET).elf
	@$(call oops_verify_dist,$(DIST))

clean:
	rm -rf $(BUILD) $(DIST)
