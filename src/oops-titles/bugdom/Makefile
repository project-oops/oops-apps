# Bugdom, through oops-sdk.
#
#   make               the payload
#   make title         the payload, packaged
#   make package       the title package with the game data, which is what runs
#   make census        compile every source this title needs, for the target, and report
#   make census-one F=<path> [X=1]   every error in one source, C or C++
#
# `census` compiles the whole source list without linking, and names the source beside each error.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Before the app.mk include, because this file names files under OOPS_SDK_DIR in an
# immediately-expanded list below. The `?=` matches app.mk's own default.
OOPS_SDK ?= $(abspath $(OOPS_APPS_ROOT)/../oops-sdk)
OOPS_SDK_DIR := $(abspath $(OOPS_SDK))

# The port renders a QuickDraw 3D game through fixed-function OpenGL, and calls GL by symbol: no
# SDL_GL_GetProcAddress, glad or GLEW anywhere in the tree, so a missing entry point is a link error
# and app.mk's guard reports it.
OOPS_RENDERER = gl1

# First, because this file names files under upstream/ while it is being read.
include $(OOPS_APPS_ROOT)/common/upstream.mk

BUG_UP := $(CURDIR)/upstream

# --- the source list -------------------------------------------------------
#
# Wildcarded because upstream globs: CMakeLists.txt:79 is
# `file(GLOB_RECURSE GAME_SOURCES ... ${GAME_SRCDIR}/*.c *.cpp)`, so the directory is the list.
BUG_C_SRCS := $(sort $(wildcard $(BUG_UP)/src/*.c) $(wildcard $(BUG_UP)/src/*/*.c))
BUG_OWN_CXX := $(sort $(wildcard $(BUG_UP)/src/*.cpp) $(wildcard $(BUG_UP)/src/*/*.cpp))

# --- Pomme, which is most of the platform layer ----------------------------
#
# Pomme reimplements the classic Mac OS toolbox - Resource Manager, Sound Manager, QuickDraw 3D -
# which a Mac game of this age rests on rather than optionally uses.
#
# Listed by name rather than globbed: CMakeLists.txt:54-57 sets POMME_NO_VIDEO, POMME_NO_INPUT,
# POMME_NO_GRAPHICS and POMME_NO_MP3, and each switch both drops sources and defines a macro the
# remaining sources compile against, so a glob would compile the four that are off against headers
# those macros empty. What stays on is the sound formats, the software mixer and QD3D.
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

# mp3.cpp goes with POMME_NO_MP3: it wants minimp3, which is not vendored, and this game's music is
# not MP3.
POMME_DEFINES := -DPOMME_NO_VIDEO -DPOMME_NO_INPUT -DPOMME_NO_GRAPHICS -DPOMME_NO_MP3

# --- the dependencies ------------------------------------------------------
include $(OOPS_APPS_ROOT)/common/posix.mk

# The pinned revision asks for SDL2: `find_package(SDL2 REQUIRED COMPONENTS main)` at
# CMakeLists.txt:67, with `#include <SDL.h>` through src/Headers.
OOPS_SDL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2)
include $(OOPS_SDL)/oops-sdl.mk

# The game throws - Boot.cpp wraps it in `catch (std::exception&)`, and std::filesystem reports
# failures as filesystem_error - and the platform exports no Itanium unwind or C++ ABI symbols, so
# both archives come from here (D005).
#
# OOPS_CXX_EXCEPTIONS is set above the include: oops-libcxx.mk reads it with `?=` at include time to
# choose between build-eh/libc++.a and build/libc++.a, and the no-exceptions archive defines
# std::terminate and std::bad_alloc itself instead of deferring to libc++abi, which collides.
OOPS_CXX_EXCEPTIONS = 1
OOPS_CXX_STD        = c++20

OOPS_LIBCXX ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libcxx)
include $(OOPS_LIBCXX)/oops-libcxx.mk
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk

# libc++ owns operator new and delete. libc++ places its own in an __lcxx_override section and each
# carries a guard asserting &operator new lies inside it; common/cxxrt.cpp's definition wins under
# --allow-multiple-definition from outside that section, and libc++'s nothrow operator new then
# executes ud2. Allocation still reaches the SDK heap, libc++'s operator new calling malloc.
# EXTRA_TARGET_CFLAGS, because common/cxx.mk appends it and compiles cxxrt.cpp.
EXTRA_TARGET_CFLAGS += -DOOPS_CXX_EXTERNAL_NEW_DELETE

# Upstream's include list, CMakeLists.txt:123. The QD3D entry is not redundant: this game's C sources
# include QD3D.h by bare name.
BUG_CFLAGS := -I$(BUG_UP)/src -I$(BUG_UP)/src/Headers \
              -I$(POMME_SRC) -I$(POMME_SRC)/QD3D \
              $(POMME_DEFINES) \
              -DOOPS_POSIX_HOME=\"/data/bugdom\" \
              $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE)

# app.mk compiles at -Werror with a set stricter than upstream's code meets. Switched off rather than
# patched, a patch against somebody else's tree being a rebase burden for no behaviour change.
BUG_WARNINGS := -Wno-strict-prototypes -Wno-missing-prototypes -Wno-conversion \
                -Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-comment \
                -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-shadow \
                -Wno-sign-compare -Wno-char-subscripts -Wno-unused-function \
                -Wno-unused-const-variable -Wno-missing-field-initializers \
                -Wno-unused-variable \
                -Wno-multichar

EXTRA_TARGET_CFLAGS += $(BUG_CFLAGS) $(BUG_WARNINGS)

# Census flags. OOPS_LIBCXX_INCLUDE precedes the C headers: libc++'s <errno.h> and friends wrap the C
# ones with #include_next, and <cerrno> does not build without them. Recursive (`=`), because
# TARGET_CFLAGS is not known until app.mk has been read.
BUG_CXX_FLAGS = -x c++ -std=c++20 $(OOPS_LIBCXX_INCLUDE) \
                $(filter-out -std=c11 -Wstrict-prototypes -Wmissing-prototypes,$(TARGET_CFLAGS)) \
                $(EXTRA_TARGET_CFLAGS)

# --- the link --------------------------------------------------------------
#
# The entry point is C++ because main is mangled: in a freestanding C++ translation unit it is an
# ordinary function, so Boot.cpp exports _Z4mainiPPc and a C shim misses it, the payload link
# ignoring unresolved symbols. So it goes in OOPS_CXX_SRCS rather than PAYLOAD_SRCS.
BUG_SHIM_CXX := shim/bugdom_start.cpp

# The C++ half goes through common/cxx.mk, app.mk building every PAYLOAD_SRCS entry as C11.
#
# OOPS_CXX_SRCS is set above the include: cxx.mk names it as a prerequisite of the archive, and make
# expands a rule's prerequisites when it reads the rule, so naming them afterwards gives an archive
# that never rebuilds when this title's C++ changes.
#
# No OOPS_CXX_LINK_OBJECTS: no two sources here share a basename, which oops_ar_check confirms.
OOPS_CXX_SRCS := $(POMME_CXX) $(BUG_OWN_CXX) $(BUG_SHIM_CXX)
include $(OOPS_APPS_ROOT)/common/cxx.mk

# After the include, which clears it, and libc++'s headers only: the rest of this title's include
# path reaches C++ through EXTRA_TARGET_CFLAGS, which OOPS_CXX_FLAGS appends last. Naming BUG_CFLAGS
# here would put the POSIX shim's headers ahead of libc++'s.
OOPS_CXX_INCLUDE += $(OOPS_LIBCXX_INCLUDE)

PAYLOAD_SRCS := $(BUG_C_SRCS) $(OOPS_POSIX_SRCS)

# No net: the game has no networking.
OOPS_FEATURES := display input keyboard mouse audio suspend thread draw

# Each archive after everything that calls it, app.mk putting LDFLAGS ahead of the payload objects
# and a plain archive seen before its callers contributing nothing. libc++ calls libc++abi, which
# calls libunwind, so those three come last in that order.
EXTRA_TARGET_LDFLAGS += $(OOPS_CXX_LDFLAGS) $(OOPS_SDL_LDFLAGS) \
                        $(OOPS_LIBCXX_LDFLAGS) $(OOPS_LIBCXXABI_LDFLAGS) $(OOPS_LIBUNWIND_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(OOPS_CXX_LIB) $(OOPS_SDL_LIB) \
                        $(OOPS_LIBCXX_LIB) $(OOPS_LIBCXXABI_LIB) $(OOPS_LIBUNWIND_LIB)

include $(OOPS_APPS_ROOT)/common/app.mk

# ---------------------------------------------------------------------------
# `make census`: which of this title's sources compile for the target
#
# One file at a time, -fsyntax-only, each blocker printed with the source that hit it. C and C++ are
# counted separately because they compile under different flags.
#
# A clean census is not a working payload: -fsyntax-only does not link, and a link is where a missing
# driver or GL entry point appears.
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

# ---------------------------------------------------------------------------
# `make package`: the title package with the game data, which is what runs
#
# `common/app.mk`'s `title` builds the eboot and `sce_sys/`, and stops there - a title's content is
# the title's business. Bugdom's is `Data/`, shipped in the upstream repository, and the entry point
# probes for `Data/Skeletons/DoodleBug.3dmf` under the package root.
# ---------------------------------------------------------------------------
.PHONY: package
package: title
	@test -d upstream/Data || { echo "bugdom: no upstream/Data - run make first" >&2; exit 1; }
	@echo "bugdom: copying $$(du -sh upstream/Data | cut -f1) of data into the package"
	@rm -rf build/title/$(TITLE_ID)/Data
	@cp -r upstream/Data build/title/$(TITLE_ID)/Data
	@# The file the shim probes for, so a truncated copy fails here rather than on the console.
	@test -f build/title/$(TITLE_ID)/Data/Skeletons/DoodleBug.3dmf || { \
	    echo "bugdom: Data/Skeletons/DoodleBug.3dmf is missing from the package" >&2; exit 1; }
	@echo "bugdom: package is build/title/$(TITLE_ID) ($$(du -sh build/title/$(TITLE_ID) | cut -f1), $$(find build/title/$(TITLE_ID) -type f | wc -l) files)"
