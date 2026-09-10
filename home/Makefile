# home - an alternative home screen, XMB-shaped and skinnable.
#
#   make check      drive the model and render into memory, verifying both (runs anywhere)
#   make skeleton   the model and renderer compile freestanding for the target, object only
#   make elf        the plain-ELF payload a homebrew loader (elfldr) runs on the console
#
# The model, the skins and the drawing (home.c) are shared between the host test and the payload.
# Only home_main.c is console-only: the display, the pad, and the loop.

OOPS_SDK ?= $(abspath ../../oops-sdk)
include $(OOPS_SDK)/oops-sdk.mk

# The SDK sources the host test links. Only the drawing: the display stack never runs here,
# because the test renders into a plain memory surface of its own.
SDK_HOST_SRCS := $(OOPS_SDK_DIR)/src/draw/draw.c

# The SDK sources the payload uses - display, input, drawing, and the payload runtime. Only
# these, so the link does not drag in SDK modules this app never touches.
SDK_ELF_SRCS := $(OOPS_SDK_DIR)/src/display.c \
                $(OOPS_SDK_DIR)/src/agc/agc_display.c $(OOPS_SDK_DIR)/src/agc/agc_tiler.c \
                $(OOPS_SDK_DIR)/src/gnm/gnm_display.c \
                $(OOPS_SDK_DIR)/src/input/input.c \
                $(OOPS_SDK_DIR)/src/memory/memory.c \
                $(OOPS_SDK_DIR)/src/system/freestd.c \
                $(OOPS_SDK_DIR)/src/system/syscall.c \
                $(OOPS_SDK_DIR)/src/system/krw.c \
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
# Sibling selfish toolchain (for eboot.bin, native title directory, and packages)
SELFISH ?= $(abspath ../../selfish)
SELFISH_BIN ?= $(firstword $(wildcard $(SELFISH)/target/debug/selfish.exe $(SELFISH)/target/release/selfish.exe $(SELFISH)/target/debug/selfish $(SELFISH)/target/release/selfish))

TITLE_ID ?= HOME00001
TITLE_NAME ?= "HOME SHELL"
TITLE_CATEGORY ?= big-app

.PHONY: all check skeleton elf eboot title dist clean

all: $(BUILD)/home_selftest skeleton elf

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/home_selftest: home_selftest.c home.c home.h $(SDK_HOST_SRCS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ home_selftest.c home.c $(SDK_HOST_SRCS)

check: $(BUILD)/home_selftest
	@./$(BUILD)/home_selftest

skeleton: | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $(BUILD)/home.o home.c
	@echo "home skeleton: compiles freestanding for the target (object only)"

# The full payload: the app plus the SDK sources it uses, freestanding for the target. The vendor
# imports (sce*) are left unresolved here and bound at load by the loader on the console.
$(BUILD)/home.elf: home.c home_main.c home.h $(SDK_ELF_SRCS) | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) -fuse-ld=lld -shared -Wl,-e,home_start \
	    -Wl,--unresolved-symbols=ignore-all -Wl,-z,noexecstack \
	    -Wl,-z,max-page-size=0x4000 -Wl,-z,common-page-size=0x4000 \
	    -o $@ home.c home_main.c $(SDK_ELF_SRCS)

elf: $(BUILD)/home.elf

# Canonical artifact filenames encoding the four axes (OOPS/docs/CONVENTIONS.md section 2):
# Target (prospero), Role/Format (launcher.elf, eboot.bin, title.zip)
LAUNCHER_ELF := $(DIST)/home-launcher-$(TARGET).elf
EBOOT_BIN    := $(DIST)/home-eboot-$(TARGET).bin
TITLE_ZIP    := $(DIST)/home-title-$(TARGET).zip

# Wrap the ELF into a fake-signed executable container (eboot.bin) via selfish
eboot: $(BUILD)/home.elf
	@mkdir -p $(DIST)
	@if [ -n "$(SELFISH_BIN)" ]; then \
	    IN=$$(command -v wslpath >/dev/null 2>&1 && wslpath -m $(BUILD)/home.elf || echo $(BUILD)/home.elf); \
	    OUT=$$(command -v wslpath >/dev/null 2>&1 && wslpath -m $(EBOOT_BIN) || echo $(EBOOT_BIN)); \
	    $(SELFISH_BIN) --input "$$IN" --target $(TARGET) --format eboot --output "$$OUT"; \
	    echo "home: created $(EBOOT_BIN)"; \
	else \
	    echo "selfish not found at $(SELFISH) - build it with cargo build -p selfish-cli"; \
	fi

# Build a complete native PS5 Title directory and package into a shippable zip
title: $(BUILD)/home.elf
	@mkdir -p $(DIST) $(BUILD)/title
	@if [ -n "$(SELFISH_BIN)" ]; then \
	    IN=$$(command -v wslpath >/dev/null 2>&1 && wslpath -m $(BUILD)/home.elf || echo $(BUILD)/home.elf); \
	    OUT=$$(command -v wslpath >/dev/null 2>&1 && wslpath -m $(BUILD)/title || echo $(BUILD)/title); \
	    $(SELFISH_BIN) --input "$$IN" --target $(TARGET) --format title --title-id $(TITLE_ID) --title $(TITLE_NAME) --category $(TITLE_CATEGORY) --output "$$OUT"; \
	    ( cd $(BUILD)/title && zip -qr $(CURDIR)/$(TITLE_ZIP) $(TITLE_ID) ); \
	    echo "home: created $(TITLE_ZIP)"; \
	else \
	    echo "selfish not found at $(SELFISH) - build it with cargo build -p selfish-cli"; \
	fi

# The release contract (see bin/oops-apps): put what ships under dist/.
# Staged artifacts explicitly encode target generation and role per CONVENTIONS.md section 2.
dist: $(BUILD)/home.elf
	@mkdir -p $(DIST)
	cp $(BUILD)/home.elf $(LAUNCHER_ELF)
	@if [ -n "$(SELFISH_BIN)" ]; then \
	    $(MAKE) eboot title; \
	fi
	@$(call oops_verify_dist,$(DIST))

clean:
	rm -rf $(BUILD) $(DIST)

