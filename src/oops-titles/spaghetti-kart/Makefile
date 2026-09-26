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

SPGK_INCLUDE := -I$(SPGK_UP) -I$(SPGK_UP)/include -I$(SPGK_UP)/src \
                -I$(SPGK_LUS)/include -I$(SPGK_LUS)/src \
                -DOOPS_POSIX_HOME=\"/data/spaghetti-kart\" \
                $(OOPS_POSIX_INCLUDE)

SPGK_WARNINGS := -Wno-strict-prototypes -Wno-missing-prototypes -Wno-conversion \
                 -Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-comment \
                 -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-shadow \
                 -Wno-sign-compare -Wno-char-subscripts -Wno-unused-function \
                 -Wno-unused-const-variable -Wno-missing-field-initializers \
                 -Wno-unused-variable -Wno-multichar

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
ifeq ($(SPGK_ARMED),1)
OOPS_CXX_EXCEPTIONS = 1
OOPS_CXX_STD        = c++20
OOPS_CXX_SRCS := $(SPGK_CXX_SRCS) shim/spgk_start.cpp
OOPS_LIBCXX ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libcxx)
include $(OOPS_LIBCXX)/oops-libcxx.mk
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk
# libc++ owns new/delete for any title linking the full archive - see bugdom/docs/PORTING.md.
EXTRA_TARGET_CFLAGS += -DOOPS_CXX_EXTERNAL_NEW_DELETE
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

# Recursive (`=`), because `TARGET_CFLAGS` is not known until `common/app.mk` has been read.
SPGK_CXXFLAGS = -x c++ -std=c++20 \
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
	        grep -m1 'error:' $(BUILD)/census-e.txt | sed 's|.*error: ||' | cut -c1-70 \
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
	        grep -m1 'error:' $(BUILD)/census-e.txt | sed 's|.*error: ||' | cut -c1-70 \
	          >> $(BUILD)/census-errors.txt; \
	    fi; \
	 done; \
	 printf 'spaghetti-kart C++ census: %s compile, %s do not (of %s)\n' \
	    "$$cok" "$$cbad" "$(words $(SPGK_CXX_SRCS))"; \
	 if [ "$$bad" -gt 0 ] || [ "$$cbad" -gt 0 ]; then \
	    printf '\n-- blockers, most common first --\n'; \
	    sort $(BUILD)/census-errors.txt | uniq -c | sort -rn | head -20; fi

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
