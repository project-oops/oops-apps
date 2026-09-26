# Spaghetti Kart, through oops-sdk.
#
#   make               the survey - what this port still needs before it can link
#   make census        compile every source this title needs, for the target, and report
#   make census-one F=<path> [X=1]   every error in one source, C or C++
#   make title         the payload, packaged - once SPGK_ARMED=1 is possible
#
# The payload is behind SPGK_ARMED: two dependencies are unvendored and libultraship has no build
# here, which `make survey` reports. app.env carries FORMATS=title and app.mk is included either way,
# so `make title` is a real target and only the source lists are conditional.
#
# ../ship-of-harkinian/README.md covers the shared runtime, shader dialect and dependency stack.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Before the app.mk include, because this file names files under OOPS_SDK_DIR below.
OOPS_SDK ?= $(abspath $(OOPS_APPS_ROOT)/../oops-sdk)
OOPS_SDK_DIR := $(abspath $(OOPS_SDK))

# A libultraship port makes no direct GL calls: the graphics are inside the library, which carries its
# own backends and generates shaders through prism-processor. So this names libultraship's backend
# rather than anything in src/.
OOPS_RENDERER = gl2

# First, because this file names files under upstream/ while it is being read.
include $(OOPS_APPS_ROOT)/common/upstream.mk

SPGK_UP := $(CURDIR)/upstream
SPGK_LUS := $(SPGK_UP)/libultraship

# --- the source list, transcribed from upstream's globs ---------------------
#
# CMakeLists.txt:237-295 is an explicit list of directories rather than one recursive glob, so this is
# transcribed: wildcarding over src/ would pick up trees upstream leaves out.
#
# src/os/ is ten named files, not the directory: upstream lists the gu matrix helpers and nothing else
# there.
SPGK_OS_SRCS := $(addprefix $(SPGK_UP)/src/os/, \
    guLookAtF.c guPerspectiveF.c guMtxF2L.c guRotateF.c guOrthoF.c guScaleF.c \
    guTranslateF.c guMtxCatF.c guMtxCatL.c guNormalize.c)

SPGK_C_DIRS  := src src/buffers src/data src/debug src/ending src/racing src/audio src/port \
                src/mods src/enhancements src/enhancements/freecam src/engine/tracks \
                assets/code courses
SPGK_CXX_DIRS := src/port src/mods src/engine src/engine/actors src/engine/objects \
                 src/engine/sky src/engine/particles src/enhancements/freecam

# Two exclusions, both upstream's (CMakeLists.txt:298-299): `*.inc.c` files are `#include`d
# into other translation units rather than compiled, and `crash_screen_enhancement.c` is filtered out
# by name.
SPGK_EXCLUDE := %.inc.c $(SPGK_UP)/src/debug/crash_screen_enhancement.c

SPGK_C_SRCS := $(filter-out $(SPGK_EXCLUDE), \
    $(sort $(SPGK_OS_SRCS) $(foreach d,$(SPGK_C_DIRS),$(wildcard $(SPGK_UP)/$(d)/*.c))))
SPGK_CXX_SRCS := $(sort $(foreach d,$(SPGK_CXX_DIRS),$(wildcard $(SPGK_UP)/$(d)/*.cpp)))

# --- what is still missing -------------------------------------------------
#
# `SPGK_DEPS` is checked by `make survey` against `src/oops-deps`, so the list stays honest rather
# than going stale in a comment. `prism` is vendored as `prism-processor`, which is why it is named
# that way here.
SPGK_DEPS := sdl2 sdl2-net zlib libzip stb nlohmann-json thread-pool tinyxml2 spdlog \
             imgui prism-processor stormlib libgfxd tomlplusplus libogg libvorbis

include $(OOPS_APPS_ROOT)/common/posix.mk

# The two header-only libraries upstream fetches at configure time, vendored and pinned instead: a
# build here pins every dependency by commit, and toml++ came from a tag while semver came from a
# raw URL on a branch. sse2neon, the third, is only reached under __aarch64__; this target takes the
# <emmintrin.h> arm beside it.
OOPS_TOMLPP ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/tomlplusplus)
include $(OOPS_TOMLPP)/oops-tomlplusplus.mk
OOPS_SEMVER ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/semver)
include $(OOPS_SEMVER)/oops-semver.mk

# libultraship reads and writes its configuration as JSON, so its headers reach into this title.
OOPS_JSON ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/nlohmann-json)
include $(OOPS_JSON)/oops-nlohmann-json.mk
# libultraship reads its archives through libzip, and its headers name <zip.h> directly.
OOPS_LIBZIP ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libzip)
include $(OOPS_LIBZIP)/oops-libzip.mk
# libultraship dispatches resource loads across a pool, and its headers name BS_thread_pool.hpp.
OOPS_THREADPOOL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/thread-pool)
include $(OOPS_THREADPOOL)/oops-thread-pool.mk
# SDL2 for the window, the GL context and the audio device, as every other title here.
OOPS_SDL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2)
include $(OOPS_SDL)/oops-sdl.mk
# libultraship logs through spdlog, and its own headers include it.
OOPS_SPDLOG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/spdlog)
include $(OOPS_SPDLOG)/oops-spdlog.mk
# The resource system reads XML manifests, and prism generates its shaders.
OOPS_TINYXML2 ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/tinyxml2)
include $(OOPS_TINYXML2)/oops-tinyxml2.mk
OOPS_PRISM ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/prism-processor)
include $(OOPS_PRISM)/oops-prism.mk
OOPS_STB ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/stb)
include $(OOPS_STB)/oops-stb.mk
OOPS_IMGUI ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/imgui)
include $(OOPS_IMGUI)/oops-imgui.mk
OOPS_STORMLIB ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/stormlib)
include $(OOPS_STORMLIB)/oops-stormlib.mk
OOPS_GFXD ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libgfxd)
include $(OOPS_GFXD)/oops-libgfxd.mk

# `libultraship/include` rather than its `libultraship/` subdirectory: sources write
# `#include <libultraship/libultraship.h>` and `<libultraship/libultra/types.h>`. torch supplies
# `Companion.h`, which the resource types name directly.
# The port's own trees are on the path by name, not just their root: sources include `math_util.h`
# and `actors.h` bare, which upstream gets from its per-directory target_include_directories.
SPGK_INCLUDE := $(OOPS_TOMLPP_INCLUDE) $(OOPS_SEMVER_INCLUDE) $(OOPS_JSON_INCLUDE) \
                $(OOPS_LIBZIP_INCLUDE) $(OOPS_THREADPOOL_INCLUDE) $(OOPS_SDL_INCLUDE) \
                $(OOPS_SPDLOG_INCLUDE) $(OOPS_TINYXML2_INCLUDE) $(OOPS_PRISM_INCLUDE) \
                $(OOPS_STB_INCLUDE) $(OOPS_IMGUI_INCLUDE) $(OOPS_STORMLIB_INCLUDE) \
                $(OOPS_GFXD_INCLUDE) \
                -Ishim/include \
                -I$(SPGK_UP) -I$(SPGK_UP)/include -I$(SPGK_UP)/src \
                -I$(SPGK_UP)/src/racing -I$(SPGK_UP)/src/engine -I$(SPGK_UP)/src/port \
                -I$(SPGK_LUS)/include -I$(SPGK_LUS)/include/libultraship \
                -I$(SPGK_LUS)/src -I$(SPGK_UP)/torch/src \
                -DOOPS_POSIX_HOME=\"/data/spaghetti-kart\" \
                $(OOPS_POSIX_INCLUDE)

# `include/actor_types.h` writes `static_assert` in C without including <assert.h>, which is where
# C11 spells it. Upstream builds at C11 and gets the header some other way; forcing it in is
# cheaper than a patch, and it is where the macro belongs.
SPGK_INCLUDE += -include assert.h

SPGK_WARNINGS := -Wno-strict-prototypes -Wno-missing-prototypes -Wno-conversion \
                 -Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-comment \
                 -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-shadow \
                 -Wno-sign-compare -Wno-char-subscripts -Wno-unused-function \
                 -Wno-unused-const-variable -Wno-missing-field-initializers \
                 -Wno-unused-variable -Wno-multichar \
                 -Wno-macro-redefined -Wno-missing-braces -Wno-missing-declarations \
                 -Wno-unused-but-set-parameter -Wno-incompatible-pointer-types \
                 -Wno-return-type -Wno-unknown-pragmas

EXTRA_TARGET_CFLAGS += $(SPGK_INCLUDE) $(SPGK_WARNINGS)

# --- the link --------------------------------------------------------------
#
# The blocker is dependencies rather than this title's sources, which `make survey` names. Arming it
# before they land gives a link whose undefined list is the whole of libultraship.
#
# main is at src/port/Game.cpp:945, so the shim is C++: in a freestanding C++ translation unit main is
# an ordinary function and comes out as _Z4mainiPPc, which a C shim misses silently, the payload link
# ignoring unresolved symbols.
# Bugdom paid for that lesson.
# libc++ is included either way: the census compiles C++ and needs its headers whether or not there
# is a payload to link. OOPS_CXX_EXCEPTIONS is above the include, which reads it at include time to
# pick between the exceptions and no-exceptions archives.
OOPS_CXX_EXCEPTIONS = 1
OOPS_CXX_STD        = c++20
OOPS_LIBCXX ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libcxx)
include $(OOPS_LIBCXX)/oops-libcxx.mk
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk
# libc++ owns new/delete for any title linking the full archive - see bugdom/docs/PORTING.md.
EXTRA_TARGET_CFLAGS += -DOOPS_CXX_EXTERNAL_NEW_DELETE

ifeq ($(SPGK_ARMED),1)
OOPS_CXX_SRCS := $(SPGK_CXX_SRCS) shim/spgk_start.cpp
include $(OOPS_APPS_ROOT)/common/cxx.mk
OOPS_CXX_INCLUDE += $(OOPS_LIBCXX_INCLUDE)
PAYLOAD_SRCS := $(SPGK_C_SRCS) $(OOPS_POSIX_SRCS)
EXTRA_TARGET_LDFLAGS += $(OOPS_CXX_LDFLAGS) \
                        $(OOPS_LIBCXX_LDFLAGS) $(OOPS_LIBCXXABI_LDFLAGS) $(OOPS_LIBUNWIND_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(OOPS_CXX_LIB) \
                        $(OOPS_LIBCXX_LIB) $(OOPS_LIBCXXABI_LIB) $(OOPS_LIBUNWIND_LIB)
endif

OOPS_FEATURES := display input keyboard mouse audio suspend thread draw

include $(OOPS_APPS_ROOT)/common/app.mk

# Census flags. OOPS_LIBCXX_INCLUDE precedes the C headers: libc++ ships its own <math.h> and
# <stdlib.h>, and <cmath> stops with an explicit error if it reaches the C ones first. Recursive
# (`=`), because TARGET_CFLAGS is not known until app.mk has been read.
SPGK_CXXFLAGS = -x c++ -std=c++20 -Wno-c++11-narrowing $(OOPS_LIBCXX_INCLUDE) \
                $(filter-out -std=c11 -Wstrict-prototypes -Wmissing-prototypes,$(TARGET_CFLAGS)) \
                $(EXTRA_TARGET_CFLAGS)

# ---------------------------------------------------------------------------
# `make survey`: what stands between this title and a link
# ---------------------------------------------------------------------------
.PHONY: survey
survey:
	@echo "spaghetti-kart: upstream $(UPSTREAM_REF) @ $(UPSTREAM_REV)"
	@printf '\n-- submodules\n'
	@for m in libultraship torch; do \
	    if [ -n "$$(ls -A upstream/$$m 2>/dev/null)" ]; then s=present; else s="MISSING"; fi; \
	    printf '   %-14s %s\n' "$$m" "$$s"; \
	 done
	@printf '\n-- source volume\n'
	@printf '   %-18s %s\n' "this title C"   "$(words $(SPGK_C_SRCS))"
	@printf '   %-18s %s\n' "this title C++" "$(words $(SPGK_CXX_SRCS))"
	@printf '   %-18s c:%-5s cpp:%-5s\n' "libultraship" \
	    "$$(find upstream/libultraship/src -name '*.c' 2>/dev/null | wc -l)" \
	    "$$(find upstream/libultraship/src -name '*.cpp' 2>/dev/null | wc -l)"
	@printf '   %-18s cpp:%-5s\n' "torch" \
	    "$$(find upstream/torch/src -name '*.cpp' 2>/dev/null | wc -l)"
	@printf '\n-- dependencies\n'
	@have=0; need=0; \
	 for d in $(SPGK_DEPS); do \
	    if [ -d $(OOPS_APPS_ROOT)/src/oops-deps/$$d ]; then \
	        printf '   %-18s vendored\n' "$$d"; have=$$((have+1)); \
	    else \
	        printf '   %-18s TO VENDOR\n' "$$d"; need=$$((need+1)); \
	    fi; \
	 done; printf '   -> %s vendored, %s to go\n' "$$have" "$$need"
	@printf '\n-- upstream fetches two headers at configure time, from unpinned refs\n'
	@printf '   sse2neon.h   raw.githubusercontent.com/DLTcollab/sse2neon @ *master*\n'
	@printf '   semver.hpp   raw.githubusercontent.com/Neargye/semver @ v1.0.0-rc\n'
	@printf '   Both are header-only. Vendor them rather than downloading during a build:\n'
	@printf '   this collection pins every dependency by commit, and `master` is not a pin.\n'
	@printf '\n-- to link\n'
	@printf '   1. vendor the dependencies above\n'
	@printf '   2. give libultraship a build (shared with ship-of-harkinian - do it once)\n'
	@printf '   3. write shim/spgk_start.cpp (C++: main is mangled, see the Makefile)\n'
	@printf '   4. make SPGK_ARMED=1 title\n'

# ---------------------------------------------------------------------------
# `make census`: which of this title's own sources compile for the target
#
# This title's own sources only, not libultraship or torch, so a clean result here still leaves the
# runtime unbuilt.
# ---------------------------------------------------------------------------
.PHONY: census
census:
	@ok=0; bad=0; mkdir -p $(BUILD); : > $(BUILD)/census-errors.txt; \
	 for f in $(SPGK_C_SRCS); do \
	    if $(TARGET_CC) $(TARGET_CFLAGS) $(EXTRA_TARGET_CFLAGS) -fsyntax-only "$$f" \
	         2>$(BUILD)/census-e.txt; then ok=$$((ok+1)); \
	    else bad=$$((bad+1)); \
	        printf '%s\t%s\n' "$${f#$(SPGK_UP)/}" \
	          "$$(grep -a -m1 'error:' $(BUILD)/census-e.txt | sed 's|.*error: ||' | cut -c1-70)" \
	          >> $(BUILD)/census-errors.txt; \
	    fi; \
	 done; \
	 printf 'spaghetti-kart C census: %s compile, %s do not (of %s)\n' \
	    "$$ok" "$$bad" "$(words $(SPGK_C_SRCS))"; \
	 cok=0; cbad=0; \
	 for f in $(SPGK_CXX_SRCS); do \
	    if $(TARGET_CC) $(SPGK_CXXFLAGS) -fsyntax-only "$$f" 2>$(BUILD)/census-e.txt; \
	      then cok=$$((cok+1)); \
	    else cbad=$$((cbad+1)); \
	        printf '%s\t%s\n' "$${f#$(SPGK_UP)/}" \
	          "$$(grep -a -m1 'error:' $(BUILD)/census-e.txt | sed 's|.*error: ||' | cut -c1-70)" \
	          >> $(BUILD)/census-errors.txt; \
	    fi; \
	 done; \
	 printf 'spaghetti-kart C++ census: %s compile, %s do not (of %s)\n' \
	    "$$cok" "$$cbad" "$(words $(SPGK_CXX_SRCS))"; \
	 if [ "$$bad" -gt 0 ] || [ "$$cbad" -gt 0 ]; then \
	    printf '\n-- blockers, most common first --\n'; \
	    cut -f2 $(BUILD)/census-errors.txt | sort | uniq -c | sort -rn | head -20; \
	    printf '\n-- the files, in $(BUILD)/census-errors.txt --\n'; \
	    head -20 $(BUILD)/census-errors.txt; fi

# ---------------------------------------------------------------------------
# `make census-one F=<path>`: every error in one source. `X=1` for a C++ source.
# ---------------------------------------------------------------------------
.PHONY: census-one
census-one:
	@if [ -n "$(X)" ]; then \
	    $(TARGET_CC) $(SPGK_CXXFLAGS) -fsyntax-only $(SPGK_UP)/$(F) 2>&1; \
	 else \
	    $(TARGET_CC) $(TARGET_CFLAGS) $(EXTRA_TARGET_CFLAGS) -fsyntax-only $(SPGK_UP)/$(F) 2>&1; \
	 fi | grep 'error:' | sed 's|.*error: ||' | sort | uniq -c | sort -rn

# This title cannot link yet, so `make` reports what it needs.
.DEFAULT_GOAL := survey
