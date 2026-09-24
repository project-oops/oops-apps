# oops-apps shared application Makefile helper
# Include this file in each application's Makefile:
#   OOPS_APPS_ROOT ?= $(abspath ../..)
#   include $(OOPS_APPS_ROOT)/common/app.mk

OOPS_APPS_ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
OOPS_SDK ?= $(abspath $(OOPS_APPS_ROOT)/../oops-sdk)
SELFISH ?= $(abspath $(OOPS_APPS_ROOT)/../selfish)

include $(OOPS_SDK)/oops-sdk.mk
OOPS_SDK_DIR := $(abspath $(OOPS_SDK))

# Object rules and the `-MMD` depfiles that make a header a prerequisite of what it is compiled
# into. Guarded, so a title that included it itself (neverball, for its own archive) is
# unaffected.
include $(OOPS_APPS_ROOT)/common/deps.mk

# **oops-gl's sources, listed once.**
#
# Apps that use OpenGL compile the library out of the SDK tree rather than linking its archive,
# because a host self-test needs the same code the payload runs and the archive is a
# freestanding cross-build. Each of them used to carry its own copy of this list.
#
# That went wrong the first time the library gained a file. On 2026-09-21 the GL 2.0 shader
# objects arrived as five new sources, and every app's link broke - **one at a time**, as each
# was next built, with an undefined-symbol error naming a function nobody had heard of rather
# than a missing file. One list is one place to add a source, and it is the same reasoning as
# `an app is a directory with a Makefile`: nothing to keep in step by hand.
#
# GLU, GLUT and the font are *not* here. They are separate libraries a program chooses, and an
# app that does not call them should not link them - `glut-demo` adds them itself.
#
# `math.c` **is** here, because the GL layer needs it: the GLSL interpreter's `sin`, `pow` and
# `floor` are the SDK's own, since a freestanding target has no libm. An app that also lists it
# would compile it twice and fail the link on duplicate symbols, so the apps that used to list
# it no longer do.
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

# The renderer a title links, chosen by one line: `OOPS_RENDERER = gl1 | gl2 | mesa`.
#
# This is the front door end users are meant to use. It hides two different mechanisms behind one
# name: a `gl1`/`gl2` title links oops-gl's sources (`OOPS_GL_SRCS`), while a `mesa` title brings up
# the hosted Mesa stack (`USE_MESA`, below). Both provide the same `oops/gfx.h` API (oops-sdk D012),
# so the title's own source is identical either way - only this line changes.
#
#   OOPS_RENDERER = gl1     fixed-function OpenGL 1.x, freestanding
#   OOPS_RENDERER = gl2     programmable OpenGL 2.0, freestanding
#   OOPS_RENDERER = mesa    OpenGL through upstream Mesa, hosted (own C runtime, oops-mesa D002)
#
# `mesa` implies a hosted title, which is a real difference in how it is built and packaged - the
# `USE_MESA` block below spells it out. A title that builds *only* a host self-test (FORMATS
# check-only, such as `gl2-cube`) does not build a payload, so it lists oops-gl in `HOST_TEST_SRCS`
# itself and leaves this unset. `USE_MESA = 1` still works on its own for anything this does not
# cover; `OOPS_RENDERER = mesa` is the documented spelling of it.
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

# **A title whose source is somebody else's** (`src/oops-titles/`), fetched rather than
# committed. `common/upstream.mk` is the whole of it, and it says why it is its own file: a
# title that names files under `upstream/` while its own Makefile is being read has to be able
# to ask for the fetch *before* this line, and two of them do. Guarded, so a title that already
# included it pays one `ifndef` here.
include $(OOPS_APPS_ROOT)/common/upstream.mk

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

    # The archives are inputs to the link, so the title has to relink when they change. Naming
    # them in LDFLAGS alone does not do that: make sees a flag, not a file, and a title whose own
    # sources are untouched is considered up to date however much Mesa underneath it moved.
    #
    # That is not theoretical. On 2026-09-17 a Mesa patch was built into the archives, the title
    # was rebuilt, packaged and deployed, and the console ran the *previous* module - the wrap
    # step had faithfully wrapped a stale ELF. The log came back identical, which read as "the
    # fix did nothing" when the fix had never been in the binary. A build that silently ships the
    # last one is worse than a build that fails.
    #
    # Flags are filtered out rather than assumed absent. `OOPS_MESA_LIBS` is mostly a list of
    # archive paths, but it also carries the `--whole-archive` pair that brackets the GL entry
    # points, and a flag named as a prerequisite is a target make has no rule for. Only the files
    # are dependencies; the flags are already in LDFLAGS above.
    PAYLOAD_EXTRA_DEPS   += $(filter-out -%,$(OOPS_MESA_LIBS))
endif

# Application identity & metadata defaults
#
# `TITLE_CATEGORY` is the *platform* category the title is packaged with (`big-app`, ...). It is
# not the same thing as `KIND`, an optional app.env field - `game`, `demo`, `probe`, `utility`,
# `payload` - that says what the app *is*, for grouping in the oops-apps index (tools/
# build-apps-index.sh). `KIND` is metadata only; the build never reads it, and the index infers
# it from the app's directory group when it is unset.
APP_NAME       ?= $(notdir $(CURDIR))
TITLE_NAME     ?= $(APP_NAME)
TITLE_CATEGORY ?= big-app
TITLE_ICON     ?= $(firstword $(wildcard sce_sys/icon0.png icon0.png assets/icon0.png assets/icon.png))
TITLE_PIC0     ?= $(firstword $(wildcard sce_sys/pic0.png pic0.png assets/pic0.png assets/background.png))
TITLE_LOGO     ?= $(firstword $(wildcard sce_sys/logo.png logo.png assets/logo.png assets/badge.png))
TITLE_SUBTITLE ?= $(APP_SUBTITLE)
FORMATS        ?= elf
ENTRY_POINT    ?= $(subst -,_,$(APP_NAME))_start

# Target-agnostic Title ID synthesis (reserving 3 characters for the target tag: ORB/NEO/PRO/TRI)
# Sony Title ID format requires strictly 9 characters: 4 letters + 5 digits ([A-Z]{4}[0-9]{5}).
# The 3-character target tag occupies indices 0..2, followed by 1 app letter and 5 digits.
ifeq ($(EXPLICIT_TITLE_ID),1)
    TITLE_ID ?= OOPS00001

    # The four letters are the app's own mnemonic - GLCB, SCSH, GALR, MESA - and they must not be
    # one of the vendor's. A vendor prefix is not ours to use, and it is the sort of mistake that
    # is invisible in the build and expensive on the hardware: on 2026-09-17 two titles carrying
    # `PPSA` were refused by the loader at `sceSblAuthMgrAuthHeader` with nothing in the log
    # naming the identifier, and several hours went into the module before the prefix was noticed.
    #
    # Checked here rather than left to review, because the build is the only place that sees
    # every app.
    #
    # **This was a warning until 2026-09-17, and the reason it was a warning has been measured.**
    # It said: `gl1-probe` and `gl2-cube` carry GL1P and GL2C, which put digits in the letter
    # positions, and whether the loader minds is not known - neither has been deployed. That was
    # the right call to make on no evidence. There is evidence now. gl1-probe was deployed with
    # `GL1P00001`: every file staged onto the target, the directory was complete and correct, and
    # the console refused it in its own log and nowhere else -
    #
    #     20 Invalid TitleId : [GL1P00001]
    #     AppPromote Error [GL1P00001] ret = [0x80bd000a]
    #     AppInstallTitleDirMain GL1P00001 0x80bd000a
    #
    # while GLCB, MESA, TLSP and PLDM - four letters each - indexed normally alongside it. So the
    # loader does mind, the failure is silent on the host side, and the only symptom is a title
    # that stages perfectly and never appears. That is exactly the kind of thing this file exists
    # to stop, so it is an error.
    TITLE_ID_SHAPE := $(shell printf '%s' '$(TITLE_ID)' | grep -cE '^[A-Z]{4}[0-9]{5}$$')
    ifneq ($(TITLE_ID_SHAPE),1)
        $(error TITLE_ID "$(TITLE_ID)" is not four capital letters followed by five digits. \
                The console stages a malformed id without complaint and then never indexes it - \
                measured on GL1P00001, 2026-09-17. Pick a mnemonic of four letters, as the other \
                apps do: gl1-cube is GLCB, seashell is SCSH, gallery is GALR)
    endif

    # Reserved by the vendor for retail titles and system applications.
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

# **The paths baked into every `.d` file, and what happens when they move.**
#
# `-MMD` records an object's prerequisites as absolute paths, so a depfile written under WSL names
# the checkout at its WSL mount while one written under Docker names it `/w/oops-sdk/...`. Build
# the same app both ways - which this collection does by design, `silkeh/clang` being
# authoritative and `oops-builder` permitted - and the second `make` reads the first's depfiles,
# needs a source at a path that does not exist here, and stops:
#
#     make: *** No rule to make target '/w/oops-sdk/src/math/math.c',
#               needed by 'build/obj/oops/oops-sdk/src/math/math.o'.  Stop.
#
# **`-MP` does not save this.** It emits a phony target for every *header* in the list and not for
# the source itself, which is exactly the prerequisite that fails. That is why the symptom is
# always a `.c` and never a `.h`.
#
# It is neither hypothetical nor rare: on 2026-09-24 ten build directories across the collection
# were poisoned this way, twice in one afternoon, and every affected app failed until somebody
# deleted `build/` by hand. The diagnosis costs far more than the fix, because the error names a
# file nobody wrote at a path nobody configured, and it appears in an app whose own sources are
# untouched.
#
# So the roots that get written into depfiles are recorded beside the objects, and a build whose
# roots differ from the recorded ones throws the objects away first. Nothing is lost by that:
# different roots mean different `-I` paths, so every object was going to be rebuilt anyway - the
# only casualty is a tree make has already proved it cannot read.
#
# The stamp lives inside `$(BUILD)`, so it is removed by `clean` along with what it describes.
OOPS_BUILD_ROOTS := $(OOPS_SDK_DIR)|$(OOPS_APPS_ROOT)|$(SELFISH)
OOPS_BUILD_ROOTS_STAMP := $(BUILD)/.oops-build-roots
ifneq ($(strip $(shell cat $(OOPS_BUILD_ROOTS_STAMP) 2>/dev/null)),$(strip $(OOPS_BUILD_ROOTS)))
ifneq ($(wildcard $(BUILD)),)
$(info $(APP_NAME): build roots moved since this tree was compiled - clearing $(BUILD))
endif
$(shell rm -rf $(BUILD))
$(shell mkdir -p $(BUILD) && echo '$(OOPS_BUILD_ROOTS)' > $(OOPS_BUILD_ROOTS_STAMP))
endif

# Toolchains and compiler flags
#
# **Bare `clang` is not the pin, and used to be mistaken for one.** Which compiler this
# resolves to depends on the runner - it was clang 21 under WSL `oops-builder` and clang 18
# under Docker `silkeh/clang:18` on the same day. `toolchain.mk`, included below once
# `TARGET_CC` is also set, turns the pin from a spelling into a check. (oops-mesa#D013)
CC = clang
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
          -Wstrict-prototypes -Wmissing-prototypes -O1 -DOOPS_HOST_BUILD \
          -D$(APP_UPPER)_HOST_BUILD -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L \
          $(OOPS_SDK_INCLUDE) $(EXTRA_CFLAGS)

TARGET_CC = clang

# Both compilers are now set, so the pin can be checked. See oops-mesa#D013.
include $(OOPS_APPS_ROOT)/toolchain.mk

# **The target gets oops-sdk's `include/libc` as well** (2026-09-20): <math.h>, <string.h> and
# <stdlib.h> under the names a port's own code calls. It is on the *target* path only - the host
# build above must keep the real C library, or a host test that includes <string.h> gets a
# freestanding one instead.
#
# **Freestanding titles only.** A hosted title (USE_MESA) takes its target C library from the Mesa
# sysroot, whose <time.h> and the rest are the real ones; oops-sdk's freestanding libc headers on
# top of them collide - the sysroot's `__clock_t` is `int`, this libc's `clock_t` is `int64_t`, and
# the redefinition breaks every target compile. So a hosted title does not get this path.
#
# **`-nostdlibinc` rides the same fork, and for the same reason turned around.**
#
# `-nostdlib` above is the *linker* flag. Without `-nostdlibinc` beside it the header search path
# is untouched, so a freestanding target compile falls through to the build machine's
# `/usr/include` - and a program that is not allowed to link the host's C library spends the whole
# compile reading its headers.
#
# **The mild failure is a confusing error.** Craft's `client.c`, `sqlite3.c` and `tinycthread.c`
# each want a header oops-sdk does not have, and without this flag all three died on
# `'bits/wordsize.h' file not found` - a glibc internal, three levels below the missing header,
# reached because clang had already accepted glibc's `netdb.h`, `fcntl.h` and `signal.h` and was
# following them inward. With it, each one names the header it actually asked for.
#
# **The failure that matters makes no noise at all**: a source whose headers glibc happens to
# satisfy compiles cleanly against declarations this target will never link, and nothing says so.
# A freestanding build that can reach the host's libc headers is not freestanding; it is a build
# that happens to agree with its host.
#
# **Freestanding only, and that is not a hedge.** A hosted title compiles with
# `--sysroot=oops-mesa/toolchain/sysroot`, and `-nostdlibinc` suppresses the standard include
# search *inside the sysroot* as well - so it removes precisely the C library a hosted title is
# meant to use. Measured before this landed: the three `src/oops-mesa` apps lost 62 of 87 target
# translation units to `'stdio.h' file not found` and friends, while every freestanding app was
# untouched - 509 target translation units across 17 apps, zero failures. Neverball had already
# reached this conclusion alone and put `-nostdlibinc` in its own archive flags, though not on the
# shim sources beside it; those are covered from here.
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

# Standardized application telemetry & log identity macros
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

# A link map beside every module.
#
# The module-fixup step leaves no symbol table a reader can use, so a crash on the console reports
# its backtrace as bare addresses and there is nothing on this side to turn them into names. That
# is not a small loss: on 2026-09-17 a `PRX_RUNTIME_ERROR` gave five perfectly good frames and the
# only way to read them would have been to relink and hope the layout matched.
#
# The map is written at link time, so it describes the module that was actually built rather than
# one reconstructed afterwards. A crash address is the load base (`0x400000`) plus the offset the
# map lists. It costs a file next to the ELF and nothing at run time.
TARGET_LDFLAGS ?= $(LLD_FLAG) -shared -Wl,-Bsymbolic -Wl,-e,$(ENTRY_POINT) \
                  -Wl,--unresolved-symbols=ignore-all \
                  -Wl,-z,norelro -Wl,-z,noexecstack \
                  -Wl,-z,max-page-size=0x4000 -Wl,-z,common-page-size=0x4000 \
                  -Wl,-Map=$(BUILD)/$(APP_NAME).map \
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

ifneq ($(strip $(PAYLOAD_SRCS)),)
all: $(if $(HOST_TEST_SRCS),$(BUILD)/$(APP_NAME)_selftest) skeleton $(BUILD)/$(APP_NAME).elf
else
all: $(if $(HOST_TEST_SRCS),$(BUILD)/$(APP_NAME)_selftest)
endif

# **`make` does not package, and the packaged output is what gets deployed.**
#
# `all` builds the ELF. `eboot.bin` and the title directory are made by `eboot`, `title` and
# `dist`, so a `make` that relinks leaves a *correct* ELF beside an eboot built from the previous
# one - and says nothing, because from `all`'s point of view nothing is wrong. The next
# `pros restore` then stages the old payload and reports `0 files ... unchanged`, which is the
# same silence this file spent 2026-09-23 removing one layer down.
#
# Making `all` package would be the wrong fix: it would run `selfish` and zip a title directory
# on every build, including the many that are only a `check`. So it says so instead, and only
# when there is something to say - the warning needs a packaged file that already exists and is
# strictly older than the module beside it.
#
# `find -newer` rather than `test -nt`, which is not in POSIX `test` and this recipe runs under
# whatever `/bin/sh` is. Strictly older, so a package written in the same coarse-grained second
# as the link that fed it is not reported.
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

# Host test gate
#
# The self-test compiles the same SDK sources the payload does, so it has the same header
# problem and the same reason to care: this is the gate that is supposed to vouch for the code,
# and a gate that reran an unchanged binary would pass on the *previous* version of a fix. See
# `common/deps.mk`.
#
# Its objects live in `hostobj/` rather than beside the payload's, because most of these sources
# are the *same files* compiled under a different compiler and a different set of flags - one
# freestanding for the console, one hosted for this machine. Sharing a directory would mean
# whichever built last decided what the other linked.
ifneq ($(strip $(HOST_TEST_SRCS)),)
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

# Freestanding target skeleton compilation (object-only)
skeleton: | $(BUILD)
ifneq ($(strip $(PAYLOAD_SRCS)),)
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $(BUILD)/$(APP_NAME).o $(firstword $(PAYLOAD_SRCS))
	@echo "$(APP_NAME) skeleton: compiles freestanding for the target (object only)"
else
	@echo "$(APP_NAME) skeleton: no target payload defined"
endif

# Every payload gets these whether it lists them or not. `libc.c` joined them on 2026-09-20: it
# is the C a port's own code calls - `sqrtf`, `malloc`, `strcpy` - and leaving it to each app to
# remember would be the wrong way round, because a payload link passes
# `--unresolved-symbols=ignore-all` and an app that forgot it would link clean and fault on the
# console. `math.c` comes with it, since that is what its float functions stand on.
CORE_SDK_SRCS := $(OOPS_SDK_DIR)/src/system/procparam.c \
                 $(OOPS_SDK_DIR)/src/system/fs.c \
                 $(OOPS_SDK_DIR)/src/system/sysmodule.c \
                 $(OOPS_SDK_DIR)/src/memory/heap.c \
                 $(OOPS_SDK_DIR)/src/time/time.c
# `libc.c`, `math.c` and `scanf.c` are the freestanding C library a non-Mesa port's own code
# stands on. A hosted title (USE_MESA) gets that C library from the Mesa sysroot and links
# FreeBSD's own libm, so adding oops-sdk's would duplicate and collide - they are for
# freestanding titles only.
#
# `scanf.c` is listed here rather than left to each app for the reason `libc.c` is: `libc.c`
# names `obs_vsscanf` whether or not the app calls `sscanf`, so an app that omitted it would
# **link cleanly** with an undefined symbol and fault on the console. That is exactly the trap
# `docs/PORTING.md` describes, and `nm -u` on a fresh link is what caught it here.
ifneq ($(USE_MESA),1)
CORE_SDK_SRCS += $(OOPS_SDK_DIR)/src/system/libc.c \
                 $(OOPS_SDK_DIR)/src/system/scanf.c \
                 $(OOPS_SDK_DIR)/src/math/math.c
endif
TARGET_SYS_SRCS ?= $(filter-out $(PAYLOAD_SRCS), $(wildcard $(CORE_SDK_SRCS)))

# ---------------------------------------------------------------------------
# The undefined-symbol check, run on every payload link
#
# **A payload link ignores unresolved symbols and has to.** The console's own modules resolve
# `sce*` imports when the payload loads, so `--unresolved-symbols=ignore-all` is not optional -
# and the consequence is that a call to a function nobody defines **links cleanly** and faults on
# the console, which is the most expensive place to find out.
#
# `docs/PORTING.md` has told a porter to run `nm -u` by hand since that guide existed. Three
# times in one day it caught something here that nothing else would have: `libc.c` missing from a
# source list, then `glut_font.c`, then `scanf.c`. A check that has to be remembered is a check
# that is not run, so it runs here, and a symbol that nothing defines now fails the build.
#
# The ELF is deleted when it fails. A payload that faults on the console should not be sitting in
# `build/` looking finished.
#
# `UNDEF_ALLOW` is the imports the loader really does resolve; an app with a module of its own
# adds to `EXTRA_UNDEF_ALLOW` rather than editing this.
# `__error` joined the list on 2026-09-21. It is the POSIX errno accessor, not a `sce*` name, and
# the platform exports it from the same FreeBSD-derived set as the socket calls `oops-sdk`'s
# `net.c` already binds weakly - obSCEne measured it callable on firmware 12.40 (sweep
# 20260909-083918). The socket and signal calls joined alongside it on 2026-09-22: `net.c` and
# signal traps bind them weakly, and `common/symbols.txt` maps them to libkernel.
#
# `oops_keyboard_poll_buttons` is the one **first-party** name here, and it is deliberate. `input.c`
# folds a keyboard's buttons into the pad poll (oops-sdk `REQ-...b4d7`) by a *weak* reference to it,
# so keyboard.c satisfies it when a title links keyboard.c and it resolves to null (skipped) when a
# title does not. Allow-listing the weak undef is what lets a pad-only title link input.c without
# dragging keyboard.c in - the alternative was every such title carrying keyboard.c by hand, or a
# hosted title (undef-check off) faulting on the first poll. A title that calls the symbol *itself*
# still gets a strong reference through `<oops/keyboard.h>`, which this does not exempt.
UNDEF_ALLOW ?= ^sce[A-Z]|^sysctlbyname$$|^__error$$|^__errno$$|^__sys_socketex$$|^oops_keyboard_poll_buttons$$|^_?(accept|bind|close|connect|listen|recv|sendto|setsockopt|sigaction|sigprocmask)$$
NM ?= nm

# **A hosted title is the case this cannot judge.** `USE_MESA` links against the Mesa sysroot and
# the console's FreeBSD C library resolves `strtoul`, `vsnprintf`, `syslog` and a hundred others
# at load, exactly as it resolves `sce*` - so for those titles an undefined libc name is correct
# and the check has nothing to tell them apart by. It is off there, and says so on every link
# rather than passing quietly: a check that looks like it ran and did not is worse than none.
# A hosted title that wants it back sets `UNDEF_CHECK=1` and lists its sysroot's exports in
# `EXTRA_UNDEF_ALLOW`.
UNDEF_CHECK ?= $(if $(filter 1,$(USE_MESA)),0,1)

# Full freestanding target payload ELF.
#
# **The makefiles are prerequisites**, which they were not until 2026-09-20. A payload's ELF
# depends on its sources, so adding a file to `CORE_SDK_SRCS` did not relink what was already
# built - and the undefined-symbol check then answered about the previous build, which is exactly
# how it looked like a fix had not worked.
#
# **The headers are prerequisites too**, which they were not until 2026-09-23, and the symptom
# was the same one a level quieter: an SDK *header* change relinked nothing, `selfish` wrapped
# the ELF that was already there, and `pros restore` reported `0 files ... unchanged` while the
# console went on running the previous code. They cannot be listed here, so each source is
# compiled to its own object with `-MMD -MP` and the depfiles are read back in. The link is
# otherwise the command it always was: the same flags, the same order, and - checked with
# `BUILD_VERSION` pinned so the embedded timestamp could not hide a difference - the same bytes
# out. `common/deps.mk` is the whole of the reasoning.
#
# The sources are no longer named here as prerequisites, because the objects are, and each
# object names its source. A relink now costs one compile rather than forty-five.
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

# Tag target ELF with fixed module metadata via obscene-tool mkmodule.
#
# A missing symbols file is fatal, not a reason to skip. It used to be part of the same guard as
# the tool itself, so a project that names a symbols file it has not generated yet built, packaged
# and deployed an ELF that mkmodule never touched - no `PT_SCE_DYNLIBDATA`, so the loader refuses
# it with "found illegal segment header" and nothing before the console says a word. mesa-winsys-probe
# generates its symbols file with `make imports`, and skipping that step cost a launch.
#
# An absent *tool* is still tolerated, because a checkout without obSCEne built is a real state and
# the target ELF is still worth having. An absent *input the project asked for* is not.
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
	    ZIP_CMD=$$(command -v zip >/dev/null 2>&1 && echo "zip -qr" || echo "tar -a -cf"); \
	    ( cd $(BUILD)/title && $$ZIP_CMD $(CURDIR)/$(TITLE_ZIP_ARTIFACT) $(TITLE_ID) ); \
	    echo "$(APP_NAME): created $(TITLE_ZIP_ARTIFACT)"; \
	else \
	    echo "selfish not found at $(SELFISH) - build it with cargo build -p selfish-cli"; \
	fi

ifneq ($(filter-out check-only,$(FORMATS)),)
ifneq ($(strip $(PAYLOAD_SRCS)),)
# Release staging: stages artifacts for each requested format in $(FORMATS)
#
# **The stamp is a prerequisite because `mkmodule` rewrites the ELF in place.** Without it, the
# `elf` case below copied `$(BUILD)/$(APP_NAME).elf` *before* the `eboot` and `title` cases had
# caused it to be tagged, so `make dist` staged a different `.elf` on its first run than on its
# second, from the same tree with no source change - measured on 2026-09-23, md5 `c7e1f856d879`
# then `8eb9be684ac3`.
#
# The first-run one is the untouched link. Its `PT_SCE_DYNLIBDATA` is *present and empty* -
# offset 0, size 0, because the linker script reserves the segment and `mkmodule` is what fills
# it (0x7e80 bytes here) and rewrites `PT_DYNAMIC` down to the loader's form. That is the
# container the stamp rule above describes: staged perfectly, refused at load, nothing said on
# this side. So the release artifact was the right one only on a *rebuild* and the wrong one on
# a clean build, which is the wrong way round for the copy that gets published.
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
