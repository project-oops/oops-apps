# The shared build for every app: sources, renderer, packaging and the host self-test.
# Each app's Makefile includes it:
#   OOPS_APPS_ROOT ?= $(abspath ../..)
#   include $(OOPS_APPS_ROOT)/common/app.mk

OOPS_APPS_ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
OOPS_SDK ?= $(abspath $(OOPS_APPS_ROOT)/../oops-sdk)
SELFISH ?= $(abspath $(OOPS_APPS_ROOT)/../selfish)

include $(OOPS_SDK)/oops-sdk.mk
OOPS_SDK_DIR := $(abspath $(OOPS_SDK))

# Object rules and `-MMD` depfiles. Guarded, so a title that includes it itself is unaffected.
include $(OOPS_APPS_ROOT)/common/deps.mk

# oops-gl's sources, compiled from the SDK tree so the host self-test runs the payload's code.
# GLU, GLUT and the font are separate libraries an app adds itself (as `glut-demo` does).
# `math.c` is included because the GLSL interpreter needs the SDK's own libm; an app must not
# list it again, or the link fails on duplicate symbols.
OOPS_GL_SRCS := \
    $(OOPS_SDK_DIR)/src/math/math.c \
    $(OOPS_SDK_DIR)/src/gl/gl_context.c \
    $(OOPS_SDK_DIR)/src/gl/gfx.c \
    $(OOPS_SDK_DIR)/src/gl/gl_state.c \
    $(OOPS_SDK_DIR)/src/gl/gl_matrix.c \
    $(OOPS_SDK_DIR)/src/gl/gl_draw.c \
    $(OOPS_SDK_DIR)/src/gl/gl_list.c \
    $(OOPS_SDK_DIR)/src/gl/gl_attrib.c \
    $(OOPS_SDK_DIR)/src/gl/gl_raster.c \
    $(OOPS_SDK_DIR)/src/gl/gl_eval.c \
    $(OOPS_SDK_DIR)/src/gl/gl_select.c \
    $(OOPS_SDK_DIR)/src/gl/gl_pixel.c \
    $(OOPS_SDK_DIR)/src/gl/gl_shader.c \
    $(OOPS_SDK_DIR)/src/gl/glsl_lex.c \
    $(OOPS_SDK_DIR)/src/gl/glsl_pp.c \
    $(OOPS_SDK_DIR)/src/gl/glsl_parse.c \
    $(OOPS_SDK_DIR)/src/gl/glsl_sema.c \
    $(OOPS_SDK_DIR)/src/gl/glsl_builtin.c \
    $(OOPS_SDK_DIR)/src/gl/glsl_link.c \
    $(OOPS_SDK_DIR)/src/gl/glsl_exec.c \
    $(OOPS_SDK_DIR)/src/gl/glsl_ps.c \
    $(OOPS_SDK_DIR)/src/gl/glsl_emit.c \
    $(OOPS_SDK_DIR)/src/gl/glsl_gen.c

# Load project-level configuration (app.env) if present
-include app.env

# The renderer a title links. `gl1` (fixed-function GL 1.x) and `gl2` (GL 2.0) link oops-gl's
# freestanding sources; `mesa` sets `USE_MESA` for hosted upstream Mesa (oops-mesa D002). All
# three provide the same `oops/gfx.h` API (oops-sdk D012), so title source does not change.
# A check-only title (such as `gl2-cube`) leaves this unset and lists oops-gl in
# `HOST_TEST_SRCS` itself.
ifdef OOPS_RENDERER
    ifeq ($(OOPS_RENDERER),mesa)
        USE_MESA := 1
    else ifeq ($(OOPS_RENDERER),gl1)
        PAYLOAD_SRCS += $(OOPS_GL_SRCS)
    else ifeq ($(OOPS_RENDERER),gl2)
        PAYLOAD_SRCS += $(OOPS_GL_SRCS)
    else
        $(error OOPS_RENDERER='$(OOPS_RENDERER)' is not one of: gl1, gl2, mesa)
    endif
endif

# The SDK subsystems a title uses (`OOPS_FEATURES = keyboard net http`), in place of
# hand-listed SDK sources. `oops-sdk.mk` defines the roster (`OOPS_FEATURES_AVAILABLE`) and
# each feature's `OOPS_FEATURE_<name>_SRCS` / `_SYMS`. Without the roster the line warns and
# is ignored; with it, an unknown name is an error.
ifdef OOPS_FEATURES
    ifdef OOPS_FEATURES_AVAILABLE
        $(foreach f,$(OOPS_FEATURES),\
            $(if $(filter $(f),$(OOPS_FEATURES_AVAILABLE)),,\
                $(error OOPS_FEATURES: '$(f)' is not a known feature; the SDK offers: $(OOPS_FEATURES_AVAILABLE))))
        PAYLOAD_SRCS += $(foreach f,$(OOPS_FEATURES),$(OOPS_FEATURE_$(f)_SRCS))
        # Collected for packaging; mkmodule's import claims come from common/symbols.txt.
        OOPS_FEATURE_SYMS += $(foreach f,$(OOPS_FEATURES),$(OOPS_FEATURE_$(f)_SYMS))
    else
        $(warning OOPS_FEATURES is set ($(OOPS_FEATURES)) but the SDK feature table (OOPS_FEATURES_AVAILABLE) is not available yet - ignoring it for now)
    endif
endif

# A ported title's fetched upstream source (`src/oops-titles/`). Guarded, so a title that
# includes it earlier in its own Makefile is unaffected.
include $(OOPS_APPS_ROOT)/common/upstream.mk

# OpenGL through Mesa, for a title that sets `USE_MESA = 1`: Mesa's headers, the winsys and
# runtime shims, and the archives in Mesa's link order. Such a title is hosted - Mesa calls
# the platform C library by name at load, while every other app links none (oops-mesa D002).
ifeq ($(USE_MESA),1)
    OOPS_MESA ?= $(abspath $(OOPS_APPS_ROOT)/../oops-mesa)
    ifeq ($(wildcard $(OOPS_MESA)/oops-mesa.mk),)
        $(error USE_MESA=1 but no oops-mesa at $(OOPS_MESA); clone it beside the collection or set OOPS_MESA)
    endif
    include $(OOPS_MESA)/oops-mesa.mk

    EXTRA_TARGET_CFLAGS  += $(OOPS_MESA_INCLUDE)
    EXTRA_TARGET_LDFLAGS += $(OOPS_MESA_LIBS) $(OOPS_MESA_SYSLIBS)
    PAYLOAD_SRCS         += $(OOPS_MESA_SRCS)

    # The archives are link prerequisites, so the title relinks when Mesa changes. The
    # `--whole-archive` flags in `OOPS_MESA_LIBS` are filtered out, since make has no rule
    # for a flag.
    PAYLOAD_EXTRA_DEPS   += $(filter-out -%,$(OOPS_MESA_LIBS))
endif

# Application identity and metadata defaults. `TITLE_CATEGORY` is the platform category the
# title is packaged with; the app.env `KIND` field (game, demo, probe, utility, payload) only
# groups the apps index (tools/build-apps-index.sh), and the build never reads it.
APP_NAME       ?= $(notdir $(CURDIR))
TITLE_NAME     ?= $(APP_NAME)
TITLE_CATEGORY ?= big-app
TITLE_ICON     ?= $(firstword $(wildcard sce_sys/icon0.png icon0.png assets/icon0.png assets/icon.png))
TITLE_PIC0     ?= $(firstword $(wildcard sce_sys/pic0.png pic0.png assets/pic0.png assets/background.png))
TITLE_LOGO     ?= $(firstword $(wildcard sce_sys/logo.png logo.png assets/logo.png assets/badge.png))
TITLE_SUBTITLE ?= $(APP_SUBTITLE)
FORMATS        ?= elf
ENTRY_POINT    ?= $(subst -,_,$(APP_NAME))_start

# Title ID synthesis. A title ID is four capital letters and five digits ([A-Z]{4}[0-9]{5});
# the synthesised one is the 3-character target tag (ORB/NEO/PRO/TRI), one app letter and
# five digits.
ifeq ($(EXPLICIT_TITLE_ID),1)
    TITLE_ID ?= OOPS00001

    # The four letters are the app's own mnemonic (GLCB, SCSH, GALR). The console stages an id
    # of any other shape without complaint and never indexes it (its log reports "Invalid
    # TitleId", ret 0x80bd000a), so a malformed id is a build error.
    TITLE_ID_SHAPE := $(shell printf '%s' '$(TITLE_ID)' | grep -cE '^[A-Z]{4}[0-9]{5}$$')
    ifneq ($(TITLE_ID_SHAPE),1)
        $(error TITLE_ID "$(TITLE_ID)" is not four capital letters followed by five digits. \
                The console stages a malformed id without complaint and then never indexes it - \
                measured on GL1P00001, 2026-09-17. Pick a mnemonic of four letters, as the other \
                apps do: gl1-cube is GLCB, seashell is SCSH, gallery is GALR)
    endif

    # Reserved by the vendor for retail titles and system applications; the loader refuses a
    # homebrew title carrying one at `sceSblAuthMgrAuthHeader`.
    TITLE_ID_PREFIX := $(shell printf '%s' '$(TITLE_ID)' | cut -c1-4)
    ifneq ($(filter $(TITLE_ID_PREFIX),PPSA PPSC CUSA PCSA PCSB PCSC PCSD PCSE PCSF PCSG NPXS),)
        $(error TITLE_ID "$(TITLE_ID)" uses "$(TITLE_ID_PREFIX)", which the vendor reserves. \
                Pick a mnemonic for this app instead, as the other apps do: gl1-cube is GLCB, \
                seashell is SCSH, gallery is GALR)
    endif
else
    APP_RAW_ID  := $(strip $(if $(TITLE_CODE),$(TITLE_CODE),$(if $(filter-out OOPS00001,$(TITLE_ID)),$(TITLE_ID),$(APP_NAME)00001)))
    APP_CHAR    := $(shell echo "$(APP_RAW_ID)" | tr -dc 'A-Za-z' | cut -c1 | tr 'a-z' 'A-Z')
    APP_CHAR    := $(if $(APP_CHAR),$(APP_CHAR),X)
    APP_NUM_RAW := $(shell echo "$(APP_RAW_ID)" | tr -dc '0-9')
    APP_NUM     := $(shell echo "00000$(if $(APP_NUM_RAW),$(APP_NUM_RAW),00001)" | tail -c 6)
    TITLE_ID    := $(OOPS_TARGET_TAG)$(APP_CHAR)$(APP_NUM)
endif

# Application macro identifier for host builds (e.g. PORTHOLE_HOST_BUILD, GL1_PROBE_HOST_BUILD)
APP_UPPER := $(shell echo $(APP_NAME) | tr a-z- A-Z_)

# Build & Dist output directories
BUILD ?= build
DIST  ?= dist

# Depfiles record absolute paths, which differ between the Docker and WSL runners, and `-MP`
# covers headers but not the source itself. The roots are stamped inside `$(BUILD)`, and a
# build under different roots clears `$(BUILD)` first; different roots mean different `-I`
# paths, so every object rebuilds anyway.
OOPS_BUILD_ROOTS := $(OOPS_SDK_DIR)|$(OOPS_APPS_ROOT)|$(SELFISH)
OOPS_BUILD_ROOTS_STAMP := $(BUILD)/.oops-build-roots
ifneq ($(strip $(shell cat $(OOPS_BUILD_ROOTS_STAMP) 2>/dev/null)),$(strip $(OOPS_BUILD_ROOTS)))
ifneq ($(wildcard $(BUILD)),)
$(info $(APP_NAME): build roots moved since this tree was compiled - clearing $(BUILD))
endif
$(shell rm -rf $(BUILD))
$(shell mkdir -p $(BUILD) && echo '$(OOPS_BUILD_ROOTS)' > $(OOPS_BUILD_ROOTS_STAMP))
endif

# Toolchains and compiler flags. Bare `clang` resolves per runner; `toolchain.mk`, included
# once `TARGET_CC` is set, checks the pin (oops-mesa#D013).
CC = clang
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
          -Wstrict-prototypes -Wmissing-prototypes -O1 -DOOPS_HOST_BUILD \
          -D$(APP_UPPER)_HOST_BUILD -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L \
          $(OOPS_SDK_INCLUDE) $(EXTRA_CFLAGS)

TARGET_CC = clang

# Both compilers are set, so the pin is checked here.
include $(OOPS_APPS_ROOT)/toolchain.mk

# A freestanding target compile gets oops-sdk's `include/libc` and `-nostdlibinc`, so it never
# reads the build machine's `/usr/include` and a missing header is named as missing. The host
# build keeps the real C library. A hosted title (USE_MESA) gets neither: its C library headers
# come from the Mesa sysroot, which `-nostdlibinc` would hide and oops-sdk's headers collide
# with (`clock_t`).
ifeq ($(USE_MESA),1)
OOPS_SDK_LIBC_INCLUDE ?=
OOPS_TARGET_NOSTDLIBINC ?=
else
OOPS_SDK_LIBC_INCLUDE ?= -I$(OOPS_SDK_DIR)/include/libc
OOPS_TARGET_NOSTDLIBINC ?= -nostdlibinc
endif
TARGET_CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
                 -Wstrict-prototypes -Wmissing-prototypes -Wvla \
                 -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib -fPIC \
                 -fno-stack-protector -fvisibility=hidden $(OOPS_TARGET_NOSTDLIBINC) \
                 $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE) $(EXTRA_TARGET_CFLAGS)

# Application identity macros for telemetry and logs.
ifeq ($(strip $(BUILD_VERSION)),)
    ifneq ($(strip $(CI)$(GITHUB_ACTIONS)),)
        GIT_COMMIT := $(shell git -C $(OOPS_APPS_ROOT) rev-parse --short HEAD 2>/dev/null)
        BUILD_VERSION := $(if $(GIT_COMMIT),$(GIT_COMMIT),$(shell echo "$${GITHUB_SHA:-ci}" | cut -c1-7))
    else
        BUILD_VERSION := $(shell date +'%Y-%m-%d %H:%M')
    endif
endif

TARGET_CFLAGS += -DOOPS_APP_ID=\"$(TITLE_ID)\" -DOOPS_APP_NAME=\"$(APP_NAME)\" -D'OOPS_APP_VERSION="$(BUILD_VERSION)"'
CFLAGS        += -DOOPS_APP_ID=\"$(TITLE_ID)\" -DOOPS_APP_NAME=\"$(APP_NAME)\" -D'OOPS_APP_VERSION="$(BUILD_VERSION)"'

# A hosted title drops `-ffreestanding` and `-fno-builtin`, which Mesa cannot build under.
# `-nostdlib` stays: the platform C library resolves at load, and the linker must not pull in
# the build machine's crt files or libc. Filtered, so an overridden `TARGET_CFLAGS` is covered.
ifeq ($(USE_MESA),1)
    TARGET_CFLAGS := $(filter-out -ffreestanding -fno-builtin,$(TARGET_CFLAGS))
endif

LLD_AVAILABLE := $(shell which lld >/dev/null 2>&1 && echo yes || echo no)
LLD_FLAG := $(if $(filter yes,$(LLD_AVAILABLE)),-fuse-ld=lld,)

TARGET_LD_SCRIPT ?= $(if $(filter 1 2,$(OOPS_TARGET_NUM)),$(SELFISH)/link/eboot.ld,$(SELFISH)/link/native_eboot.ld)
TARGET_LD_FLAG   := $(if $(wildcard $(TARGET_LD_SCRIPT)),-T $(TARGET_LD_SCRIPT),)

# A link map beside every module, since the packaged module has no usable symbol table. A
# console crash address is the load base (`0x400000`) plus the offset the map lists.
TARGET_LDFLAGS ?= $(LLD_FLAG) -shared -Wl,-Bsymbolic -Wl,-e,$(ENTRY_POINT) \
                  -Wl,--unresolved-symbols=ignore-all \
                  -Wl,-z,norelro -Wl,-z,noexecstack \
                  -Wl,-z,max-page-size=0x4000 -Wl,-z,common-page-size=0x4000 \
                  -Wl,-Map=$(BUILD)/$(APP_NAME).map \
                  $(TARGET_LD_FLAG) \
                  $(EXTRA_TARGET_LDFLAGS)

# Release artifact names encoding the four axes (OOPS CONVENTIONS.md section 2).
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

ifneq ($(strip $(PAYLOAD_SRCS)),)
all: $(if $(HOST_TEST_SRCS),$(BUILD)/$(APP_NAME)_selftest) skeleton $(BUILD)/$(APP_NAME).elf
else
all: $(if $(HOST_TEST_SRCS),$(BUILD)/$(APP_NAME)_selftest)
endif

# `all` builds the ELF but does not package it (`eboot`, `title` and `dist` do). It notes any
# existing package strictly older than the ELF, since restoring that deploys the previous code.
# `find -newer`, because `test -nt` is not POSIX.
ifneq ($(strip $(PAYLOAD_SRCS)),)
all:
	@elf="$(BUILD)/$(APP_NAME).elf"; stale=""; \
	for p in $(BUILD)/title/$(TITLE_ID)/eboot.bin $(EBOOT_ARTIFACT) $(TITLE_ZIP_ARTIFACT); do \
	    [ -f "$$p" ] || continue; \
	    if [ -n "$$(find "$$elf" -newer "$$p" 2>/dev/null)" ]; then stale="$$stale $$p"; fi; \
	done; \
	if [ -n "$$stale" ]; then \
	    echo "$(APP_NAME): NOTE - $$elf is newer than what was packaged from it:"; \
	    for p in $$stale; do echo "    $$p"; done; \
	    echo "  'make' builds the module, not the package. Run 'make title' (or 'make eboot',"; \
	    echo "  'make dist') before restoring, or the console keeps running the previous code."; \
	fi
endif

$(BUILD):
	@mkdir -p $(BUILD)

# The core runtime every payload links, from oops-sdk.mk's `OOPS_FEATURE_base_SRCS` when it
# is defined.
CORE_SDK_SRCS := $(if $(OOPS_FEATURE_base_SRCS),$(OOPS_FEATURE_base_SRCS),\
                 $(OOPS_SDK_DIR)/src/system/system.c \
                 $(OOPS_SDK_DIR)/src/system/freestd.c \
                 $(OOPS_SDK_DIR)/src/system/syscall.c \
                 $(OOPS_SDK_DIR)/src/system/offsets.c \
                 $(OOPS_SDK_DIR)/src/system/procparam.c \
                 $(OOPS_SDK_DIR)/src/system/fs.c \
                 $(OOPS_SDK_DIR)/src/system/sysmodule.c \
                 $(OOPS_SDK_DIR)/src/memory/memory.c \
                 $(OOPS_SDK_DIR)/src/memory/heap.c \
                 $(OOPS_SDK_DIR)/src/time/time.c)
# The freestanding C library. A hosted title (USE_MESA) takes its own from the Mesa sysroot.
ifneq ($(USE_MESA),1)
CORE_SDK_SRCS += $(OOPS_SDK_DIR)/src/system/libc.c \
                 $(OOPS_SDK_DIR)/src/system/scanf.c \
                 $(OOPS_SDK_DIR)/src/math/math.c
endif

# Host test gate. Its objects live in `hostobj/`, since the same sources compile hosted here
# and freestanding for the payload.
#
# A shared source (oops-sdk or common/) the self-test links and the payload does not is an
# error at parse time, so `make check` cannot pass on a payload that fails to link. Paths are
# compared via `$(abspath)`; an intended host-only source goes in `OOPS_HOST_ONLY_SRCS_OK`.
ifneq ($(strip $(HOST_TEST_SRCS)),)
ifneq ($(strip $(PAYLOAD_SRCS)),)
OOPS_SHARED_PREFIXES := $(OOPS_SDK_DIR)/% $(OOPS_APPS_ROOT)/common/%
OOPS_HOST_SHARED := $(filter $(OOPS_SHARED_PREFIXES),$(abspath $(HOST_TEST_SRCS)))
OOPS_PAY_SHARED  := $(filter $(OOPS_SHARED_PREFIXES),$(abspath $(PAYLOAD_SRCS) $(CORE_SDK_SRCS)))
OOPS_HOST_ONLY   := $(filter-out $(OOPS_PAY_SHARED) $(abspath $(OOPS_HOST_ONLY_SRCS_OK)),$(OOPS_HOST_SHARED))
ifneq ($(strip $(OOPS_HOST_ONLY)),)
$(info $(APP_NAME): these shared sources are in HOST_TEST_SRCS and not in PAYLOAD_SRCS:)
$(foreach s,$(OOPS_HOST_ONLY),$(info     $(s)))
$(info   The self-test would link them and the payload would not, so `make check` can pass while)
$(info   `make title` fails on an undefined symbol. Add them to PAYLOAD_SRCS, or name them in)
$(info   OOPS_HOST_ONLY_SRCS_OK if the host really is meant to have something the payload lacks.)
$(error $(APP_NAME): host and payload disagree about $(words $(OOPS_HOST_ONLY)) shared source(s))
endif
endif
OOPS_HOST_OBJS := $(call oops_objs,$(BUILD)/hostobj,$(HOST_TEST_SRCS))
-include $(OOPS_HOST_OBJS:.o=.d)
$(call oops_obj_rules,$(BUILD)/hostobj,CC,CFLAGS,$(HOST_TEST_SRCS))

$(BUILD)/$(APP_NAME)_selftest: $(OOPS_HOST_OBJS) $(HOST_TEST_EXTRA_DEPS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(OOPS_HOST_OBJS) $(HOST_TEST_LIBS)

check: $(BUILD)/$(APP_NAME)_selftest
	@./$(BUILD)/$(APP_NAME)_selftest
else
check:
	@echo "$(APP_NAME): no host selftest defined"
endif

# Freestanding target skeleton compilation (object only).
skeleton: | $(BUILD)
ifneq ($(strip $(PAYLOAD_SRCS)),)
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $(BUILD)/$(APP_NAME).o $(firstword $(PAYLOAD_SRCS))
	@echo "$(APP_NAME) skeleton: compiles freestanding for the target (object only)"
else
	@echo "$(APP_NAME) skeleton: no target payload defined"
endif

TARGET_SYS_SRCS ?= $(filter-out $(PAYLOAD_SRCS), $(wildcard $(CORE_SDK_SRCS)))

# The undefined-symbol check, run on every payload link. The link must ignore unresolved
# symbols, because the console's modules resolve `sce*` imports at load; so a symbol nothing
# defines fails the build here, and the ELF is deleted.
#
# `UNDEF_ALLOW` is the imports the loader resolves; an app with its own module adds to
# `EXTRA_UNDEF_ALLOW`. Beyond `sce*` it holds `__error` and the socket and signal calls that
# oops-sdk binds weakly, mapped to libkernel in `common/symbols.txt` and present in
# `obscene/data/obscene-report.txt`. `oops_keyboard_poll_buttons` is a weak reference from
# `input.c` that resolves to null when a title does not link keyboard.c.
UNDEF_ALLOW ?= ^sce[A-Z]|^sysctlbyname$$|^__error$$|^__errno$$|^__sys_socketex$$|^oops_keyboard_poll_buttons$$|^_?(accept|bind|close|connect|listen|recv|recvfrom|sendto|setsockopt|sigaction|sigprocmask)$$
NM ?= nm

# Off for a hosted title (USE_MESA), whose libc names resolve at load, and reported as off on
# every link. Such a title may set `UNDEF_CHECK=1` and list its sysroot's exports in
# `EXTRA_UNDEF_ALLOW`.
UNDEF_CHECK ?= $(if $(filter 1,$(USE_MESA)),0,1)

# Full freestanding target payload ELF. Its prerequisites are the objects (each naming its
# source and, through `-MMD` depfiles, its headers) and the makefiles, so a change to any of
# them relinks it (`common/deps.mk`).
ifneq ($(strip $(PAYLOAD_SRCS)),)
OOPS_PAYLOAD_OBJS := $(call oops_objs,$(BUILD)/obj,$(PAYLOAD_SRCS) $(TARGET_SYS_SRCS))
-include $(OOPS_PAYLOAD_OBJS:.o=.d)
$(call oops_obj_rules,$(BUILD)/obj,TARGET_CC,TARGET_CFLAGS,$(PAYLOAD_SRCS) $(TARGET_SYS_SRCS))

$(BUILD)/$(APP_NAME).elf: $(OOPS_PAYLOAD_OBJS) $(PAYLOAD_EXTRA_DEPS) \
                          $(oops_makefiles) | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -o $@ $(OOPS_PAYLOAD_OBJS)
	@nm_tool=$$(command -v $(NM) 2>/dev/null || command -v llvm-nm 2>/dev/null || true); \
	if [ "$(UNDEF_CHECK)" != "1" ]; then \
	  echo "$(APP_NAME): the undefined-symbol check is off - a hosted title's C library is"; \
	  echo "  resolved at load, so an undefined libc name there is correct (see common/app.mk)"; \
	elif [ -z "$$nm_tool" ]; then \
	  echo "$(APP_NAME): WARNING - no nm, so the undefined-symbol check did not run"; \
	else \
	  undef=$$("$$nm_tool" -u $@ 2>/dev/null \
	    | sed 's/^[[:space:]]*//;s/^w //;s/^U //' \
	    | grep -vE '$(UNDEF_ALLOW)$(if $(EXTRA_UNDEF_ALLOW),|$(EXTRA_UNDEF_ALLOW))' || true); \
	  if [ -n "$$undef" ]; then \
	    echo "$(APP_NAME): these functions are called and nothing defines them:"; \
	    echo "$$undef" | sed 's/^/    /'; \
	    echo "  A payload link ignores unresolved symbols, so this would have faulted on the"; \
	    echo "  console instead of failing here. Add the file that defines them to PAYLOAD_SRCS,"; \
	    echo "  or to CORE_SDK_SRCS in common/app.mk when every payload needs it."; \
	    rm -f $@; \
	    exit 1; \
	  fi; \
	fi

elf: $(BUILD)/$(APP_NAME).elf
endif

# Tag the target ELF with module metadata via obscene-tool mkmodule. An absent tool is skipped;
# an absent symbols file is fatal, because an untagged ELF has no `PT_SCE_DYNLIBDATA` and the
# loader refuses it with "found illegal segment header".
$(BUILD)/.mkmodule-fixed.stamp: $(BUILD)/$(APP_NAME).elf $(SYMBOLS_FILE)
	@if [ -n "$(MKMODULE_BIN)" ] && $(MKMODULE_BIN) --help >/dev/null 2>&1; then \
	    if [ ! -f "$(SYMBOLS_FILE)" ]; then \
	        echo "$(APP_NAME): $(SYMBOLS_FILE) does not exist, so mkmodule cannot say where this" >&2; \
	        echo "  module's imports resolve. Without it the container loads nothing on hardware." >&2; \
	        echo "  If this project generates it, generate it - mesa-winsys-probe uses 'make imports'." >&2; \
	        exit 1; \
	    fi; \
	    IN="$<"; SYM="$(SYMBOLS_FILE)"; \
	    case "$(MKMODULE_BIN)" in *.exe) command -v wslpath >/dev/null 2>&1 && { IN=$$(wslpath -m "$$IN"); SYM=$$(wslpath -m "$$SYM"); } ;; esac; \
	    $(MKMODULE_BIN) mkmodule --symbols "$$SYM" --generation $(MKMODULE_GEN) --table $(MKMODULE_TABLE) --kind $(MKMODULE_KIND) "$$IN"; \
	fi
	@touch $@

# Wrap the target ELF into a signed executable container (eboot.bin) via selfish.
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

# Package the target ELF into a native PS5 title directory (.zip) via selfish. python3 is
# probed by running `import zipfile`, because Windows' App Execution Alias stub is found on
# PATH whether or not Python is installed.
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
	    PIC0_FLAG=""; \
	    if [ -n "$(TITLE_PIC0)" ] && [ -f "$(TITLE_PIC0)" ]; then \
	        PIC0_PATH="$(TITLE_PIC0)"; \
	        case "$(SELFISH_BIN)" in *.exe) command -v wslpath >/dev/null 2>&1 && PIC0_PATH=$$(wslpath -m "$$PIC0_PATH") ;; esac; \
	        PIC0_FLAG="--pic0 $$PIC0_PATH"; \
	    fi; \
	    LOGO_FLAG=""; \
	    if [ -n "$(TITLE_LOGO)" ] && [ -f "$(TITLE_LOGO)" ]; then \
	        LOGO_PATH="$(TITLE_LOGO)"; \
	        case "$(SELFISH_BIN)" in *.exe) command -v wslpath >/dev/null 2>&1 && LOGO_PATH=$$(wslpath -m "$$LOGO_PATH") ;; esac; \
	        LOGO_FLAG="--logo $$LOGO_PATH"; \
	    fi; \
	    PRIV_FLAG=""; \
	    if [ -n "$(PRIVILEGE)" ]; then \
	        PRIV_FLAG="--privilege $(PRIVILEGE)"; \
	    fi; \
	    $(SELFISH_BIN) --input "$$IN" --target $(TARGET) --format title \
	        --title-id $(TITLE_ID) --title "$(subst ",,$(TITLE_NAME))" --category $(TITLE_CATEGORY) \
	        --title-version $(TITLE_VERSION) --sdk $(TARGET) $$ICON_FLAG $$PIC0_FLAG $$LOGO_FLAG \
	        $(if $(strip $(TITLE_SUBTITLE)),--subtitle "$(subst ",,$(strip $(TITLE_SUBTITLE)))") \
	        $$PRIV_FLAG --output "$$OUT"; \
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
	    ZIP_OUT="$(CURDIR)/$(TITLE_ZIP_ARTIFACT)"; rm -f "$$ZIP_OUT"; \
	    if command -v zip >/dev/null 2>&1; then \
	        ( cd $(BUILD)/title && zip -qr "$$ZIP_OUT" $(TITLE_ID) ); \
	    elif python3 -c 'import zipfile' >/dev/null 2>&1; then \
	        ( cd $(BUILD)/title && python3 -m zipfile -c "$$ZIP_OUT" $(TITLE_ID) ); \
	    elif tar --version 2>/dev/null | grep -qiE 'bsdtar|libarchive'; then \
	        ( cd $(BUILD)/title && tar -a -cf "$$ZIP_OUT" $(TITLE_ID) ); \
	    else \
	        echo "$(APP_NAME): cannot package title - GNU tar would write a tar named .zip; install 'zip' or 'python3' (or bsdtar)" >&2; exit 1; \
	    fi; \
	    echo "$(APP_NAME): created $(TITLE_ZIP_ARTIFACT)"; \
	else \
	    echo "selfish not found at $(SELFISH) - build it with cargo build -p selfish-cli"; \
	fi

ifneq ($(filter-out check-only,$(FORMATS)),)
ifneq ($(strip $(PAYLOAD_SRCS)),)
# Release staging for each format in $(FORMATS). The mkmodule stamp is a prerequisite
# because mkmodule rewrites the ELF in place (it fills the empty `PT_SCE_DYNLIBDATA` the linker
# script reserves), so the staged `.elf` is always the tagged one.
dist: $(BUILD)/$(APP_NAME).elf $(BUILD)/.mkmodule-fixed.stamp
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
endif
endif

clean:
	rm -rf $(BUILD) $(DIST)
