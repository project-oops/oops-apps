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

# OpenGL through Mesa, for a title that asks for it.
#
# A title opts in with `USE_MESA = 1` in its own Makefile or app.env. Everything it brings is
# additive: Mesa's headers, the winsys and runtime shims, and the archives in the order Mesa
# links them. Nothing changes for a title that does not ask.
#
# **A title that sets this is hosted and is not freestanding.** Mesa calls `malloc`, `snprintf`
# and their kin by their published names, and the platform's own C library answers at load. The
# rest of oops-apps links no C library at all. That divergence is oops-mesa's D002, confined to
# titles that set this switch, and `OOPS_MESA_HOSTED` is defined so the title and its packaging
# can both say which kind they are.
ifeq ($(USE_MESA),1)
    OOPS_MESA ?= $(abspath $(OOPS_APPS_ROOT)/../oops-mesa)
    ifeq ($(wildcard $(OOPS_MESA)/oops-mesa.mk),)
        $(error USE_MESA=1 but no oops-mesa at $(OOPS_MESA); clone it beside the collection or set OOPS_MESA)
    endif
    include $(OOPS_MESA)/oops-mesa.mk

    EXTRA_TARGET_CFLAGS  += $(OOPS_MESA_INCLUDE)
    EXTRA_TARGET_LDFLAGS += $(OOPS_MESA_LIBS) $(OOPS_MESA_SYSLIBS)
    PAYLOAD_SRCS         += $(OOPS_MESA_SRCS)
endif

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

# Standardized application telemetry & log identity macros
TARGET_CFLAGS += -DOOPS_APP_ID=\"$(TITLE_ID)\" -DOOPS_APP_NAME=\"$(APP_NAME)\"
CFLAGS        += -DOOPS_APP_ID=\"$(TITLE_ID)\" -DOOPS_APP_NAME=\"$(APP_NAME)\"

# A hosted title drops the freestanding flags, and only those.
#
# `-ffreestanding` tells the compiler there is no standard library and `-fno-builtin` stops it
# recognising `memcpy` and its kin. Mesa is written for a hosted environment and needs both gone.
#
# `-nostdlib` stays, and the difference matters. It is a *link* flag, and nothing here links a C
# library: the platform's own answers `malloc` and its kin at load, exactly as oops-sdk's vendor
# calls are answered. Dropping it too makes the linker go looking for `crtbeginS.o`, `libgcc` and
# `libc` on the build machine, none of which belong in this binary.
#
# Filtered here rather than by a different default above, so a title that overrode
# `TARGET_CFLAGS` itself gets the same treatment.
ifeq ($(USE_MESA),1)
    TARGET_CFLAGS := $(filter-out -ffreestanding -fno-builtin,$(TARGET_CFLAGS))
endif

LLD_AVAILABLE := $(shell which lld >/dev/null 2>&1 && echo yes || echo no)
LLD_FLAG := $(if $(filter yes,$(LLD_AVAILABLE)),-fuse-ld=lld,)

TARGET_LD_SCRIPT ?= $(if $(filter 1 2,$(OOPS_TARGET_NUM)),$(SELFISH)/link/eboot.ld,$(SELFISH)/link/native_eboot.ld)
TARGET_LD_FLAG   := $(if $(wildcard $(TARGET_LD_SCRIPT)),-T $(TARGET_LD_SCRIPT),)

TARGET_LDFLAGS ?= $(LLD_FLAG) -shared -Wl,-Bsymbolic -Wl,-e,$(ENTRY_POINT) \
                  -Wl,--unresolved-symbols=ignore-all \
                  -Wl,-z,norelro -Wl,-z,noexecstack \
                  -Wl,-z,max-page-size=0x4000 -Wl,-z,common-page-size=0x4000 \
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
    SELFISH_BIN ?= $(firstword $(wildcard $(SELFISH)/target/debug/selfish.exe $(SELFISH)/target/release/selfish.exe $(SELFISH)/target-win/debug/selfish.exe $(SELFISH)/target-win/release/selfish.exe $(SELFISH)/target/debug/selfish $(SELFISH)/target/release/selfish))
    MKMODULE_BIN ?= $(firstword $(wildcard $(OOPS_APPS_ROOT)/../obscene/tool/target/release/obscene-tool.exe $(OOPS_APPS_ROOT)/../obscene/tool/target/debug/obscene-tool.exe $(OOPS_APPS_ROOT)/../obscene/tool/target-win/release/obscene-tool.exe $(OOPS_APPS_ROOT)/../obscene/tool/target-win/debug/obscene-tool.exe $(OOPS_APPS_ROOT)/../obscene/tool/target/release/obscene-tool $(OOPS_APPS_ROOT)/../obscene/tool/target/debug/obscene-tool))
else
    SELFISH_BIN ?= $(firstword $(wildcard $(SELFISH)/target/debug/selfish $(SELFISH)/target/release/selfish $(SELFISH)/target/debug/selfish.exe $(SELFISH)/target/release/selfish.exe $(SELFISH)/target-win/debug/selfish.exe $(SELFISH)/target-win/release/selfish.exe))
    MKMODULE_BIN ?= $(firstword $(wildcard $(OOPS_APPS_ROOT)/../obscene/tool/target/release/obscene-tool $(OOPS_APPS_ROOT)/../obscene/tool/target/debug/obscene-tool $(OOPS_APPS_ROOT)/../obscene/tool/target/release/obscene-tool.exe $(OOPS_APPS_ROOT)/../obscene/tool/target/debug/obscene-tool.exe $(OOPS_APPS_ROOT)/../obscene/tool/target-win/release/obscene-tool.exe $(OOPS_APPS_ROOT)/../obscene/tool/target-win/debug/obscene-tool.exe))
endif
SYMBOLS_FILE ?= $(OOPS_APPS_ROOT)/common/symbols.txt
MKMODULE_GEN ?= $(if $(filter 1 2,$(OOPS_TARGET_NUM)),4,5)
MKMODULE_TABLE ?= orbis
MKMODULE_KIND ?= executable

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

CORE_SDK_SRCS := $(OOPS_SDK_DIR)/src/system/procparam.c \
                 $(OOPS_SDK_DIR)/src/system/fs.c \
                 $(OOPS_SDK_DIR)/src/memory/heap.c \
                 $(OOPS_SDK_DIR)/src/time/time.c
TARGET_SYS_SRCS ?= $(filter-out $(PAYLOAD_SRCS), $(wildcard $(CORE_SDK_SRCS)))

# Full freestanding target payload ELF
$(BUILD)/$(APP_NAME).elf: $(PAYLOAD_SRCS) $(TARGET_SYS_SRCS) $(PAYLOAD_EXTRA_DEPS) | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -o $@ $(PAYLOAD_SRCS) $(TARGET_SYS_SRCS)

elf: $(BUILD)/$(APP_NAME).elf

# Tag target ELF with fixed module metadata via obscene-tool mkmodule.
#
# A missing symbols file is fatal, not a reason to skip. It used to be part of the same guard as
# the tool itself, so a project that names a symbols file it has not generated yet built, packaged
# and deployed an ELF that mkmodule never touched - no `PT_SCE_DYNLIBDATA`, so the loader refuses
# it with "found illegal segment header" and nothing before the console says a word. mesa-probe
# generates its symbols file with `make imports`, and skipping that step cost a launch.
#
# An absent *tool* is still tolerated, because a checkout without obSCEne built is a real state and
# the target ELF is still worth having. An absent *input the project asked for* is not.
$(BUILD)/.mkmodule-fixed.stamp: $(BUILD)/$(APP_NAME).elf
	@if [ -n "$(MKMODULE_BIN)" ] && $(MKMODULE_BIN) --help >/dev/null 2>&1; then \
	    if [ ! -f "$(SYMBOLS_FILE)" ]; then \
	        echo "$(APP_NAME): $(SYMBOLS_FILE) does not exist, so mkmodule cannot say where this" >&2; \
	        echo "  module's imports resolve. Without it the container loads nothing on hardware." >&2; \
	        echo "  If this project generates it, generate it - mesa-probe uses 'make imports'." >&2; \
	        exit 1; \
	    fi; \
	    IN="$<"; SYM="$(SYMBOLS_FILE)"; \
	    case "$(MKMODULE_BIN)" in *.exe) command -v wslpath >/dev/null 2>&1 && { IN=$$(wslpath -m "$$IN"); SYM=$$(wslpath -m "$$SYM"); } ;; esac; \
	    $(MKMODULE_BIN) mkmodule --symbols "$$SYM" --generation $(MKMODULE_GEN) --table $(MKMODULE_TABLE) --kind $(MKMODULE_KIND) "$$IN"; \
	fi
	@touch $@

# Wrap target ELF into a signed executable container (eboot.bin) via selfish
eboot: $(BUILD)/$(APP_NAME).elf $(BUILD)/.mkmodule-fixed.stamp
	@mkdir -p $(DIST)
	@if [ -n "$(SELFISH_BIN)" ]; then \
	    IN="$<"; OUT="$(EBOOT_ARTIFACT)"; \
	    case "$(SELFISH_BIN)" in *.exe) command -v wslpath >/dev/null 2>&1 && { IN=$$(wslpath -m "$$IN"); OUT=$$(wslpath -m "$$OUT"); } ;; esac; \
	    $(SELFISH_BIN) --input "$$IN" --target $(TARGET) --format eboot --sdk $(TARGET) --output "$$OUT"; \
	    echo "$(APP_NAME): created $(EBOOT_ARTIFACT)"; \
	else \
	    echo "selfish not found at $(SELFISH) - build it with cargo build -p selfish-cli"; \
	fi

# Package target ELF into a native PS5 title directory (.zip) via selfish
title: $(BUILD)/$(APP_NAME).elf $(BUILD)/.mkmodule-fixed.stamp
	@mkdir -p $(DIST) $(BUILD)/title
	@if [ -n "$(SELFISH_BIN)" ]; then \
	    IN="$<"; OUT="$(BUILD)/title"; \
	    case "$(SELFISH_BIN)" in *.exe) command -v wslpath >/dev/null 2>&1 && { IN=$$(wslpath -m "$$IN"); OUT=$$(wslpath -m "$$OUT"); } ;; esac; \
	    ICON_FLAG=""; \
	    if [ -n "$(TITLE_ICON)" ] && [ -f "$(TITLE_ICON)" ]; then \
	        ICON_PATH="$(TITLE_ICON)"; \
	        case "$(SELFISH_BIN)" in *.exe) command -v wslpath >/dev/null 2>&1 && ICON_PATH=$$(wslpath -m "$$ICON_PATH") ;; esac; \
	        ICON_FLAG="--icon $$ICON_PATH"; \
	    fi; \
	    PRIV_FLAG=""; \
	    if [ -n "$(PRIVILEGE)" ]; then \
	        PRIV_FLAG="--privilege $(PRIVILEGE)"; \
	    fi; \
	    $(SELFISH_BIN) --input "$$IN" --target $(TARGET) --format title \
	        --title-id $(TITLE_ID) --title $(TITLE_NAME) --category $(TITLE_CATEGORY) \
	        --title-version $(TITLE_VERSION) --sdk $(TARGET) $$ICON_FLAG $$PRIV_FLAG --output "$$OUT"; \
	    if [ "$(TITLE_CATEGORY)" = "big-app" ] || [ -z "$(TITLE_CATEGORY)" ]; then \
	        mkdir -p $(BUILD)/title/$(TITLE_ID)/sce_module; \
	        $(TARGET_CC) -std=c11 -target x86_64-unknown-freebsd -ffreestanding -fno-builtin \
	            -nostdlib -fPIC -fno-stack-protector -fvisibility=hidden -shared \
	            -Wl,-e,module_start -Wl,-T,$(SELFISH)/link/library.ld \
	            -Wl,-z,noexecstack -Wl,-z,max-page-size=0x4000 -Wl,-z,common-page-size=0x4000 -Wl,-z,norelro \
	            -o $(BUILD)/libc.module.elf $(OOPS_SDK_DIR)/src/system/sce_module.c; \
	        if [ -n "$(MKMODULE_BIN)" ] && [ -f "$(SYMBOLS_FILE)" ] && $(MKMODULE_BIN) --help >/dev/null 2>&1; then \
	            LIB_IN="$(BUILD)/libc.module.elf"; SYM="$(SYMBOLS_FILE)"; \
	            PRX_OUT="$(BUILD)/title/$(TITLE_ID)/sce_module/libc.prx"; \
	            case "$(MKMODULE_BIN)" in *.exe) command -v wslpath >/dev/null 2>&1 && { LIB_IN=$$(wslpath -m "$$LIB_IN"); SYM=$$(wslpath -m "$$SYM"); PRX_OUT=$$(wslpath -m "$$PRX_OUT"); } ;; esac; \
	            $(MKMODULE_BIN) mkmodule --symbols "$$SYM" --module-name libc --kind shared --generation $(MKMODULE_GEN) --table orbis "$$LIB_IN"; \
	            $(MKMODULE_BIN) mkself "$$LIB_IN" --generation 4 --privilege app --out "$$PRX_OUT"; \
	        fi; \
	    fi; \
	    ZIP_CMD=$$(command -v zip >/dev/null 2>&1 && echo "zip -qr" || echo "tar -a -cf"); \
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
