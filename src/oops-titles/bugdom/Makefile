# Bugdom, through oops-sdk.
#
#   make census   compile every source this title needs, for the target, and report
#
# **This does not build a payload yet.** `make census` compiles the whole source list under the
# real target flags and prints what each one died on; docs/PORTING.md keeps the standing numbers.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# **`OOPS_SDK_DIR` early, because this file names files under it before `app.mk` is included.**
# `app.mk` defines it at its line 11, too late for an immediately-expanded (`:=`) list further
# down. The `?=` matches app.mk's own default, so including app.mk afterwards changes nothing.
OOPS_SDK ?= $(abspath $(OOPS_APPS_ROOT)/../oops-sdk)
OOPS_SDK_DIR := $(abspath $(OOPS_SDK))

# **`gl1`, and the count is upstream's own.** Bugdom is a 1999 QuickDraw 3D game; jorio's port
# renders it through fixed-function OpenGL. Every `gl[A-Z]*` name in the tree - 51 of them - is
# defined by oops-gl.
#
# **It calls them by symbol, so the by-name trap does not apply here.** q3rally binds every entry
# point through `SDL_GL_GetProcAddress` and needed `make glsurface` to find that oops-gl's lookup
# table answered none of them; Bugdom names no loader at all - no `SDL_GL_GetProcAddress`, no glad,
# no GLEW - so a missing entry point is an ordinary link error and `app.mk`'s guard catches it.
# Checked rather than assumed: the same grep that finds the loader in q3rally's tree finds nothing
# in this one.
OOPS_RENDERER = gl1

# **First, because this file names files under `upstream/` while it is being read.**
include $(OOPS_APPS_ROOT)/common/upstream.mk

BUG_UP := $(CURDIR)/upstream

# --- the source list -------------------------------------------------------
#
# **A wildcard, and here that is upstream's own answer rather than a shortcut.** Bugdom's
# `CMakeLists.txt:79` is `file(GLOB_RECURSE GAME_SOURCES ... ${GAME_SRCDIR}/*.c *.cpp)`, so the
# directory *is* the list. q3rally needed 132 paths derived from `make -n` because its build
# assembles them through 3,420 lines of conditionals; this one does not.
BUG_C_SRCS := $(sort $(wildcard $(BUG_UP)/src/*.c) $(wildcard $(BUG_UP)/src/*/*.c))
BUG_OWN_CXX := $(sort $(wildcard $(BUG_UP)/src/*.cpp) $(wildcard $(BUG_UP)/src/*/*.cpp))

# --- Pomme, which is most of the platform layer ----------------------------
#
# Pomme is jorio's reimplementation of the classic Mac OS toolbox - Resource Manager, Sound
# Manager, QuickDraw 3D - and Bugdom is a Mac game, so this is not an optional dependency.
#
# **Its list is transcribed, not globbed, because four of its subsystems are switched off.**
# Bugdom's `CMakeLists.txt:54-57` sets `POMME_NO_VIDEO`, `POMME_NO_INPUT`, `POMME_NO_GRAPHICS` and
# `POMME_NO_MP3`, and each one both drops sources *and* defines a macro that the remaining sources
# compile against. Globing `extern/Pomme/src` would compile the four that are off - against headers
# whose declarations those very macros remove.
#
# What stays on is everything Bugdom did not name: the sound formats, the software mixer and QD3D.
POMME_SRC := $(BUG_UP)/extern/Pomme/src
POMME_CXX := \
    $(POMME_SRC)/Pomme.cpp \
    $(POMME_SRC)/PommeDebug.cpp \
    $(POMME_SRC)/Files/Files.cpp \
    $(POMME_SRC)/Files/HostVolume.cpp \
    $(POMME_SRC)/Files/Resources.cpp \
    $(POMME_SRC)/Memory/Memory.cpp \
    $(POMME_SRC)/Text/TextUtilities.cpp \
    $(POMME_SRC)/Time/TimeManager.cpp \
    $(POMME_SRC)/Utilities/bigendianstreams.cpp \
    $(POMME_SRC)/Utilities/IEEEExtended.cpp \
    $(POMME_SRC)/Utilities/memstream.cpp \
    $(POMME_SRC)/Utilities/StringUtils.cpp \
    $(POMME_SRC)/Utilities/structpack.cpp \
    $(POMME_SRC)/SoundFormats/AIFF.cpp \
    $(POMME_SRC)/SoundFormats/IMA4.cpp \
    $(POMME_SRC)/SoundFormats/MACE.cpp \
    $(POMME_SRC)/SoundFormats/Midi.cpp \
    $(POMME_SRC)/SoundFormats/SoundFormats.cpp \
    $(POMME_SRC)/SoundFormats/xlaw.cpp \
    $(POMME_SRC)/SoundMixer/ChannelImpl.cpp \
    $(POMME_SRC)/SoundMixer/cmixer.cpp \
    $(POMME_SRC)/SoundMixer/SoundManager.cpp \
    $(POMME_SRC)/QD3D/3DMFParser.cpp \
    $(POMME_SRC)/QD3D/QD3D.cpp \
    $(POMME_SRC)/QD3D/QD3DMath.cpp

# `mp3.cpp` is absent for the same reason as the rest: `POMME_NO_MP3`. It wants minimp3, which is
# not vendored here, and Bugdom's music is not MP3.
POMME_DEFINES := -DPOMME_NO_VIDEO -DPOMME_NO_INPUT -DPOMME_NO_GRAPHICS -DPOMME_NO_MP3

# --- the dependencies ------------------------------------------------------
include $(OOPS_APPS_ROOT)/common/posix.mk

# **SDL2, and that is what the pinned revision asks for.**
#
# This was going to be SDL3 - `oops-deps/sdl3` was vendored for it - and the premise was wrong.
# The candidate survey read `find_package(SDL3 CONFIG REQUIRED)` out of a clone of **master**;
# 1.3.4, which `upstream.lock` pins and which is the newest release tag, has
# `find_package(SDL2 REQUIRED COMPONENTS main)` at `CMakeLists.txt:67` and `#include <SDL.h>`
# throughout `src/Headers`. Bugdom 2's pinned v4.0.0 is the same. The lesson is the cheap one:
# read the revision the lock names, not the checkout that happens to be on disk.
#
# Staying on SDL2 rather than bumping the pin, for the reason the lock already gives - a port
# starts from what upstream calls finished, and `master` is not a release. It is also the shorter
# road to a hardware run: five titles are proven on `oops-deps/sdl2` and nothing has yet run on
# `oops-deps/sdl3`, so this keeps one unknown at a time.
OOPS_SDL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2)
include $(OOPS_SDL)/oops-sdl.mk

OOPS_LIBCXX ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libcxx)
include $(OOPS_LIBCXX)/oops-libcxx.mk

# **The include list is upstream's, transcribed.** `CMakeLists.txt:123` names
# `extern/Pomme/src`, `extern/Pomme/src/QD3D` and `src/Headers` - and the QD3D one is not
# redundant: Bugdom's C sources write `#include <QD3D.h>` by bare name, so without it 77 of the 78
# stop on the first include. That was the census's first answer.
BUG_CFLAGS := -I$(BUG_UP)/src -I$(BUG_UP)/src/Headers \
              -I$(POMME_SRC) -I$(POMME_SRC)/QD3D \
              $(POMME_DEFINES) \
              -DOOPS_POSIX_HOME=\"/data/bugdom\" \
              $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE)

# **Upstream's code under upstream's warnings.** `common/app.mk` compiles at `-Werror` with a set
# stricter than this codebase was written for. Switched off rather than patched away: a patch
# against somebody else's tree is a rebase burden for no behaviour change. The list grows only when
# the census names a new one.
BUG_WARNINGS := -Wno-strict-prototypes -Wno-missing-prototypes -Wno-conversion \
                -Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-comment \
                -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-shadow \
                -Wno-sign-compare -Wno-char-subscripts -Wno-unused-function \
                -Wno-unused-const-variable -Wno-missing-field-initializers \
                -Wno-unused-variable

EXTRA_TARGET_CFLAGS += $(BUG_CFLAGS) $(BUG_WARNINGS)

# **The C++ flags, with `OOPS_LIBCXX_INCLUDE` ahead of the C headers.** `oops-stormlib.mk` had that
# ordering the other way round and built anyway, because its C++ never includes `<cmath>`; prism
# found it the hard way. Pomme includes `<string>`, `<array>` or `<iostream>` in almost every
# source, so it would find this immediately.
#
# Recursive (`=`), because `TARGET_CFLAGS` is not known until `common/app.mk` has been read.
BUG_CXX_FLAGS = -x c++ -std=c++20 $(OOPS_LIBCXX_INCLUDE) \
                $(filter-out -std=c11 -Wstrict-prototypes -Wmissing-prototypes,$(TARGET_CFLAGS)) \
                $(EXTRA_TARGET_CFLAGS)

# Nothing links yet. `q3rally/Makefile` is the shape this grows into: a real source list, named and
# deliberately not armed, beside a measurement that decides the title.
FORMATS = check-only

include $(OOPS_APPS_ROOT)/common/app.mk

# ---------------------------------------------------------------------------
# `make census`: which of this title's sources compile for the target, today
#
# One file at a time, `-fsyntax-only`, each blocker printed **with the source that hit it** so that
# clearing one does not start with a grep to find out where it was.
#
# **C and C++ are counted separately**, because they are compiled differently and the C++ half is a
# different question: Pomme is C++20 and needs libc++, which the C half does not.
#
# **A clean census is not a working port.** `-fsyntax-only` does not link, and a link is where a
# missing driver or GL entry point shows up - SDL3's own backend compiled 159 sources while 57
# symbols were missing.
# ---------------------------------------------------------------------------
.PHONY: census
census:
	@ok=0; bad=0; mkdir -p $(BUILD); : > $(BUILD)/census-errors.txt; \
	 for f in $(BUG_C_SRCS); do \
	    if $(TARGET_CC) $(TARGET_CFLAGS) $(EXTRA_TARGET_CFLAGS) -fsyntax-only "$$f" \
	         2>$(BUILD)/census-e.txt; then ok=$$((ok+1)); \
	    else bad=$$((bad+1)); \
	        printf '%s: %s\n' "$${f#$(BUG_UP)/}" \
	          "$$(grep -m1 'error:' $(BUILD)/census-e.txt | sed 's|.*error: ||' | cut -c1-60)" \
	          >> $(BUILD)/census-errors.txt; \
	    fi; \
	 done; \
	 printf 'bugdom C census: %s compile, %s do not\n' "$$ok" "$$bad"; \
	 cok=0; cbad=0; \
	 for f in $(POMME_CXX) $(BUG_OWN_CXX); do \
	    if $(TARGET_CC) $(BUG_CXX_FLAGS) -fsyntax-only "$$f" 2>$(BUILD)/census-e.txt; \
	      then cok=$$((cok+1)); \
	    else cbad=$$((cbad+1)); \
	        printf '%s: %s\n' "$${f#$(BUG_UP)/}" \
	          "$$(grep -m1 'error:' $(BUILD)/census-e.txt | sed 's|.*error: ||' | cut -c1-60)" \
	          >> $(BUILD)/census-errors.txt; \
	    fi; \
	 done; \
	 printf 'bugdom C++ census: %s compile, %s do not\n' "$$cok" "$$cbad"; \
	 if [ "$$bad" -gt 0 ] || [ "$$cbad" -gt 0 ]; then \
	    printf '\n-- blockers --\n'; sort $(BUILD)/census-errors.txt | head -30; fi

# ---------------------------------------------------------------------------
# `make census-one F=<path>`: every error in one source, not just the first
#
# `F` is relative to the upstream tree. Add `X=1` for a C++ source.
# ---------------------------------------------------------------------------
.PHONY: census-one
census-one:
	@if [ -n "$(X)" ]; then \
	    $(TARGET_CC) $(BUG_CXX_FLAGS) -fsyntax-only $(BUG_UP)/$(F) 2>&1; \
	 else \
	    $(TARGET_CC) $(TARGET_CFLAGS) $(EXTRA_TARGET_CFLAGS) -fsyntax-only $(BUG_UP)/$(F) 2>&1; \
	 fi | grep 'error:' | sed 's|.*error: ||' | sort | uniq -c | sort -rn
