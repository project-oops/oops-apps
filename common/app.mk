# oops-apps shared application Makefile helper
# Include this file in each application's Makefile:
#   OOPS_APPS_ROOT ?= $(abspath ../..)
#   include $(OOPS_APPS_ROOT)/common/app.mk

OOPS_APPS_ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
OOPS_SDK ?= $(abspath $(OOPS_APPS_ROOT)/../oops-sdk)
SELFISH ?= $(abspath $(OOPS_APPS_ROOT)/../selfish)

include $(OOPS_SDK)/oops-sdk.mk
OOPS_SDK_DIR := $(abspath $(OOPS_SDK))

# Load project-level configuration (app.env) if present
-include app.env

# Application identity & metadata defaults
APP_NAME       ?= $(notdir $(CURDIR))
TITLE_NAME     ?= $(APP_NAME)
TITLE_CATEGORY ?= big-app
TITLE_VERSION  ?= 01.00
TITLE_ICON     ?= $(firstword $(wildcard sce_sys/icon0.png icon0.png assets/icon0.png))
FORMATS        ?= elf
ENTRY_POINT    ?= $(subst -,_,$(APP_NAME))_start

# Target-agnostic Title ID synthesis (reserving 3 characters for the target tag: ORB/NEO/PRO/TRI)
# Sony Title ID format requires strictly 9 characters: 4 letters + 5 digits ([A-Z]{4}[0-9]{5}).
# The 3-character target tag occupies indices 0..2, followed by 1 app letter and 5 digits.
ifeq ($(EXPLICIT_TITLE_ID),1)
    TITLE_ID ?= OOPS00001
else
    APP_RAW_ID  := $(strip $(if $(TITLE_CODE),$(TITLE_CODE),$(if $(filter-out OOPS00001,$(TITLE_ID)),$(TITLE_ID),$(APP_NAME)00001)))
    APP_CHAR    := $(shell echo "$(APP_RAW_ID)" | tr -dc 'A-Za-z' | cut -c1 | tr 'a-z' 'A-Z')
    APP_CHAR    := $(if $(APP_CHAR),$(APP_CHAR),X)
    APP_NUM_RAW := $(shell echo "$(APP_RAW_ID)" | tr -dc '0-9')
    APP_NUM     := $(shell echo "00000$(if $(APP_NUM_RAW),$(APP_NUM_RAW),00001)" | tail -c 6)
    TITLE_ID    := $(OOPS_TARGET_TAG)$(APP_CHAR)$(APP_NUM)
endif

# Application macro identifier for host builds (e.g. PORTHOLE_HOST_BUILD, WIPEOUT_HOST_BUILD)
APP_UPPER := $(shell echo $(APP_NAME) | tr a-z- A-Z_)

# Build & Dist output directories
BUILD ?= build
DIST  ?= dist

# Toolchains and compiler flags
CC = clang
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
          -Wstrict-prototypes -Wmissing-prototypes -O1 -DOOPS_HOST_BUILD \
          -D$(APP_UPPER)_HOST_BUILD -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L \
          $(OOPS_SDK_INCLUDE) $(EXTRA_CFLAGS)

TARGET_CC = clang
TARGET_CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
                 -Wstrict-prototypes -Wmissing-prototypes -Wvla \
                 -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib -fPIC \
                 -fno-stack-protector -fvisibility=hidden \
                 $(OOPS_SDK_INCLUDE) $(EXTRA_TARGET_CFLAGS)

LLD_AVAILABLE := $(shell which lld >/dev/null 2>&1 && echo yes || echo no)
LLD_FLAG := $(if $(filter yes,$(LLD_AVAILABLE)),-fuse-ld=lld,)

TARGET_LD_SCRIPT ?= $(if $(filter 1 2,$(OOPS_TARGET_NUM)),$(SELFISH)/link/eboot.ld,$(SELFISH)/link/native_eboot.ld)
TARGET_LD_FLAG   := $(if $(wildcard $(TARGET_LD_SCRIPT)),-T $(TARGET_LD_SCRIPT),)

TARGET_LDFLAGS ?= $(LLD_FLAG) -shared -Wl,-e,$(ENTRY_POINT) \
                  -Wl,--unresolved-symbols=ignore-all -Wl,-z,noexecstack \
                  $(TARGET_LD_FLAG) \
                  $(EXTRA_TARGET_LDFLAGS)

# Release artifact names encoding the four axes (OOPS CONVENTIONS.md section 2)
ifeq ($(APP_NAME),home)
    ELF_ARTIFACT ?= $(DIST)/$(APP_NAME)-launcher-$(TARGET).elf
else
    ELF_ARTIFACT ?= $(DIST)/$(APP_NAME)-$(TARGET).elf
endif
EBOOT_ARTIFACT     ?= $(DIST)/$(APP_NAME)-eboot-$(TARGET).bin
TITLE_ZIP_ARTIFACT ?= $(DIST)/$(APP_NAME)-title-$(TARGET).zip

# Sibling selfish toolchain (for eboot.bin, native title directory, and packages)
ifeq ($(OS),Windows_NT)
    SELFISH_BIN ?= $(firstword $(wildcard $(SELFISH)/target/debug/selfish.exe $(SELFISH)/target/release/selfish.exe $(SELFISH)/target/debug/selfish $(SELFISH)/target/release/selfish))
else
    SELFISH_BIN ?= $(firstword $(wildcard $(SELFISH)/target/debug/selfish $(SELFISH)/target/release/selfish $(SELFISH)/target/debug/selfish.exe $(SELFISH)/target/release/selfish.exe))
endif

.DEFAULT_GOAL := all

.PHONY: all check skeleton elf eboot title dist clean

all: $(if $(HOST_TEST_SRCS),$(BUILD)/$(APP_NAME)_selftest) skeleton $(BUILD)/$(APP_NAME).elf

$(BUILD):
	@mkdir -p $(BUILD)

# Host test gate
ifneq ($(strip $(HOST_TEST_SRCS)),)
$(BUILD)/$(APP_NAME)_selftest: $(HOST_TEST_SRCS) $(HOST_TEST_EXTRA_DEPS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(HOST_TEST_SRCS) $(HOST_TEST_LIBS)

check: $(BUILD)/$(APP_NAME)_selftest
	@./$(BUILD)/$(APP_NAME)_selftest
else
check:
	@echo "$(APP_NAME): no host selftest defined"
endif

# Freestanding target skeleton compilation (object-only)
skeleton: | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $(BUILD)/$(APP_NAME).o $(firstword $(PAYLOAD_SRCS))
	@echo "$(APP_NAME) skeleton: compiles freestanding for the target (object only)"

TARGET_SYS_SRCS ?= $(wildcard $(OOPS_SDK_DIR)/src/system/procparam.c)

# Full freestanding target payload ELF
$(BUILD)/$(APP_NAME).elf: $(PAYLOAD_SRCS) $(TARGET_SYS_SRCS) $(PAYLOAD_EXTRA_DEPS) | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -o $@ $(PAYLOAD_SRCS) $(TARGET_SYS_SRCS)

elf: $(BUILD)/$(APP_NAME).elf

# Wrap target ELF into a signed executable container (eboot.bin) via selfish
eboot: $(BUILD)/$(APP_NAME).elf
	@mkdir -p $(DIST)
	@if [ -n "$(SELFISH_BIN)" ]; then \
	    IN=$$(command -v wslpath >/dev/null 2>&1 && wslpath -m "$<" || echo "$<"); \
	    OUT=$$(command -v wslpath >/dev/null 2>&1 && wslpath -m "$(EBOOT_ARTIFACT)" || echo "$(EBOOT_ARTIFACT)"); \
	    $(SELFISH_BIN) --input "$$IN" --target $(TARGET) --format eboot --sdk $(TARGET) --output "$$OUT"; \
	    echo "$(APP_NAME): created $(EBOOT_ARTIFACT)"; \
	else \
	    echo "selfish not found at $(SELFISH) - build it with cargo build -p selfish-cli"; \
	fi

# Package target ELF into a native PS5 title directory (.zip) via selfish
title: $(BUILD)/$(APP_NAME).elf
	@mkdir -p $(DIST) $(BUILD)/title
	@if [ -n "$(SELFISH_BIN)" ]; then \
	    IN=$$(command -v wslpath >/dev/null 2>&1 && wslpath -m "$<" || echo "$<"); \
	    OUT=$$(command -v wslpath >/dev/null 2>&1 && wslpath -m "$(BUILD)/title" || echo "$(BUILD)/title"); \
	    ICON_FLAG=""; \
	    if [ -n "$(TITLE_ICON)" ] && [ -f "$(TITLE_ICON)" ]; then \
	        ICON_WIN=$$(command -v wslpath >/dev/null 2>&1 && wslpath -m "$(TITLE_ICON)" || echo "$(TITLE_ICON)"); \
	        ICON_FLAG="--icon $$ICON_WIN"; \
	    fi; \
	    $(SELFISH_BIN) --input "$$IN" --target $(TARGET) --format title \
	        --title-id $(TITLE_ID) --title $(TITLE_NAME) --category $(TITLE_CATEGORY) \
	        --title-version $(TITLE_VERSION) --sdk $(TARGET) $$ICON_FLAG --output "$$OUT"; \
	    ZIP_CMD=$$(command -v zip 2>/dev/null && echo "zip -qr" || echo "tar -a -cf"); \
	    ( cd $(BUILD)/title && $$ZIP_CMD $(CURDIR)/$(TITLE_ZIP_ARTIFACT) $(TITLE_ID) ); \
	    echo "$(APP_NAME): created $(TITLE_ZIP_ARTIFACT)"; \
	else \
	    echo "selfish not found at $(SELFISH) - build it with cargo build -p selfish-cli"; \
	fi

# Release staging: stages artifacts for each requested format in $(FORMATS)
dist: $(BUILD)/$(APP_NAME).elf
	@mkdir -p $(DIST)
	@for fmt in $(FORMATS); do \
	    case "$$fmt" in \
	        elf) \
	            cp $(BUILD)/$(APP_NAME).elf $(ELF_ARTIFACT); \
	            echo "$(APP_NAME): staged $(ELF_ARTIFACT)"; \
	            ;; \
	        eboot) \
	            $(MAKE) eboot; \
	            ;; \
	        title) \
	            $(MAKE) title; \
	            ;; \
	        *) \
	            echo "warning: unknown format '$$fmt' in FORMATS" >&2; \
	            ;; \
	    esac; \
	done
	@$(call oops_verify_dist,$(DIST))

clean:
	rm -rf $(BUILD) $(DIST)
