# Bugdom 2, through oops-sdk.
#
#   make               the payload
#   make title         the payload, packaged
#   make census        compile every source this title needs, for the target, and report
#   make census-one F=<path> [X=1]   every error in one source, C or C++
#   make glsurface     whether oops-gl's by-name lookup answers the entry points this game loads
#
# This file is the same shape as ../bugdom/Makefile: same author, same engine lineage, the same Pomme,
# the same SDL2 and the same freeware data arrangement. Only the differences carry comments.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Before the app.mk include, because this file names files under OOPS_SDK_DIR in an
# immediately-expanded list below. The `?=` matches app.mk's own default.
OOPS_SDK ?= $(abspath $(OOPS_APPS_ROOT)/../oops-sdk)
OOPS_SDK_DIR := $(abspath $(OOPS_SDK))

# Fixed-function OpenGL, with gluLookAt and gluPerspective for the camera, which is why
# OOPS_FEATURES names glu where Bugdom's does not.
#
# Unlike Bugdom this port uses a loader, so a lookup answering NULL is a null call at run time rather
# than a link error: OGL_Support.c:132 and :135 bind glActiveTexture and glClientActiveTexture through
# SDL_GL_GetProcAddress. `make glsurface` is the check.
OOPS_RENDERER = gl1

# First, because this file names files under upstream/ while it is being read.
include $(OOPS_APPS_ROOT)/common/upstream.mk

BUG2_UP := $(CURDIR)/upstream

# --- the source list -------------------------------------------------------
#
# Wildcarded because upstream globs (CMakeLists.txt:80). Every C source sits one directory below
# Source/; Boot.cpp is the only C++ and the only file directly in it.
BUG2_C_SRCS := $(sort $(wildcard $(BUG2_UP)/Source/*.c) $(wildcard $(BUG2_UP)/Source/*/*.c))
BUG2_OWN_CXX := $(sort $(wildcard $(BUG2_UP)/Source/*.cpp) $(wildcard $(BUG2_UP)/Source/*/*.cpp))

# --- Pomme, which is most of the platform layer ----------------------------
#
# Pomme reimplements the classic Mac OS toolbox, which a Mac game of this age rests on rather than
# optionally uses. The pinned tree records a different Pomme revision from Bugdom's, which is
# upstream's choice; one pinned hash still pins everything.
#
# Listed by name rather than globbed: CMakeLists.txt:54-58 sets POMME_NO_VIDEO, POMME_NO_INPUT,
# POMME_NO_GRAPHICS, POMME_NO_QD3D and POMME_NO_MP3, and each switch both drops sources and defines a
# macro the remaining sources compile against.
#
# POMME_NO_QD3D is the one Bugdom does not set: this game brought its own Source/3D, so Pomme's QD3D
# sources are off and the -I.../QD3D include Bugdom needs is absent here. Upstream's
# target_include_directories (CMakeLists.txt:123) names only extern/Pomme/src and Source/Headers, and
# no source here includes QD3D.h by bare name.
POMME_SRC := $(BUG2_UP)/extern/Pomme/src
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
    $(POMME_SRC)/SoundMixer/SoundManager.cpp

POMME_DEFINES := -DPOMME_NO_VIDEO -DPOMME_NO_INPUT -DPOMME_NO_GRAPHICS -DPOMME_NO_QD3D \
                 -DPOMME_NO_MP3

# --- the dependencies ------------------------------------------------------
include $(OOPS_APPS_ROOT)/common/posix.mk

# The pinned revision asks for SDL2: `find_package(SDL2 REQUIRED COMPONENTS main)` at
# CMakeLists.txt:68, with `#include <SDL.h>` throughout.
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
EXTRA_TARGET_CFLAGS += -DOOPS_CXX_EXTERNAL_NEW_DELETE

# Upstream's include list, CMakeLists.txt:123-128. GL_SILENCE_DEPRECATION is upstream's flag, from
# its target_compile_definitions.
BUG2_CFLAGS := -I$(BUG2_UP)/Source -I$(BUG2_UP)/Source/Headers \
               -I$(POMME_SRC) \
               $(POMME_DEFINES) -DGL_SILENCE_DEPRECATION \
               -DOOPS_POSIX_HOME=\"/data/bugdom2\" \
               $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE)

# app.mk compiles at -Werror with a set stricter than upstream's code meets. Switched off rather than
# patched, a patch against somebody else's tree being a rebase burden for no behaviour change.
BUG2_WARNINGS := -Wno-strict-prototypes -Wno-missing-prototypes -Wno-conversion \
                 -Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-comment \
                 -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-shadow \
                 -Wno-sign-compare -Wno-char-subscripts -Wno-unused-function \
                 -Wno-unused-const-variable -Wno-missing-field-initializers \
                 -Wno-unused-variable -Wno-multichar

EXTRA_TARGET_CFLAGS += $(BUG2_CFLAGS) $(BUG2_WARNINGS)

# Census flags. OOPS_LIBCXX_INCLUDE precedes the C headers: libc++'s <errno.h> and friends wrap the C
# ones with #include_next, and <cerrno> does not build without them. Recursive (`=`), because
# TARGET_CFLAGS is not known until app.mk has been read.
BUG2_CXX_FLAGS = -x c++ -std=c++20 $(OOPS_LIBCXX_INCLUDE) \
                 $(filter-out -std=c11 -Wstrict-prototypes -Wmissing-prototypes,$(TARGET_CFLAGS)) \
                 $(EXTRA_TARGET_CFLAGS)

# --- the link --------------------------------------------------------------
#
# The entry point is C++ because main is mangled: in a freestanding C++ translation unit it is an
# ordinary function, so Boot.cpp exports _Z4mainiPPc and a C shim misses it, the payload link ignoring
# unresolved symbols. So it goes in OOPS_CXX_SRCS rather than PAYLOAD_SRCS.
BUG2_SHIM_CXX := shim/bugdom2_start.cpp

# The C++ half goes through common/cxx.mk, app.mk building every PAYLOAD_SRCS entry as C11.
#
# OOPS_CXX_SRCS is set above the include: cxx.mk names it as a prerequisite of the archive, and make
# expands a rule's prerequisites when it reads the rule, so naming them afterwards gives an archive
# that never rebuilds when this title's C++ changes.
#
# No OOPS_CXX_LINK_OBJECTS: no two sources here share a basename, which oops_ar_check confirms.
OOPS_CXX_SRCS := $(POMME_CXX) $(BUG2_OWN_CXX) $(BUG2_SHIM_CXX)
include $(OOPS_APPS_ROOT)/common/cxx.mk

# After the include, which clears it, and libc++'s headers only: the rest of this title's include path
# reaches C++ through EXTRA_TARGET_CFLAGS, which OOPS_CXX_FLAGS appends last. Naming BUG2_CFLAGS here
# would put the POSIX shim's headers ahead of libc++'s.
OOPS_CXX_INCLUDE += $(OOPS_LIBCXX_INCLUDE)

PAYLOAD_SRCS := $(BUG2_C_SRCS) $(OOPS_POSIX_SRCS)

# glu for the camera. No net: the game has no networking.
OOPS_FEATURES := glu display input keyboard mouse audio suspend thread draw

# Each archive after everything that calls it, app.mk putting LDFLAGS ahead of the payload objects and
# a plain archive seen before its callers contributing nothing. libc++ calls libc++abi, which calls
# libunwind, so those three come last in that order.
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
	 for f in $(BUG2_C_SRCS); do \
	    if $(TARGET_CC) $(TARGET_CFLAGS) $(EXTRA_TARGET_CFLAGS) -fsyntax-only "$$f" \
	         2>$(BUILD)/census-e.txt; then ok=$$((ok+1)); \
	    else bad=$$((bad+1)); \
	        printf '%s: %s\n' "$${f#$(BUG2_UP)/}" \
	          "$$(grep -m1 'error:' $(BUILD)/census-e.txt | sed 's|.*error: ||' | cut -c1-60)" \
	          >> $(BUILD)/census-errors.txt; \
	    fi; \
	 done; \
	 printf 'bugdom2 C census: %s compile, %s do not\n' "$$ok" "$$bad"; \
	 cok=0; cbad=0; \
	 for f in $(POMME_CXX) $(BUG2_OWN_CXX); do \
	    if $(TARGET_CC) $(BUG2_CXX_FLAGS) -fsyntax-only "$$f" 2>$(BUILD)/census-e.txt; \
	      then cok=$$((cok+1)); \
	    else cbad=$$((cbad+1)); \
	        printf '%s: %s\n' "$${f#$(BUG2_UP)/}" \
	          "$$(grep -m1 'error:' $(BUILD)/census-e.txt | sed 's|.*error: ||' | cut -c1-60)" \
	          >> $(BUILD)/census-errors.txt; \
	    fi; \
	 done; \
	 printf 'bugdom2 C++ census: %s compile, %s do not\n' "$$cok" "$$cbad"; \
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
	    $(TARGET_CC) $(BUG2_CXX_FLAGS) -fsyntax-only $(BUG2_UP)/$(F) 2>&1; \
	 else \
	    $(TARGET_CC) $(TARGET_CFLAGS) $(EXTRA_TARGET_CFLAGS) -fsyntax-only $(BUG2_UP)/$(F) 2>&1; \
	 fi | grep 'error:' | sed 's|.*error: ||' | sort | uniq -c | sort -rn

# ---------------------------------------------------------------------------
# `make glsurface`: does oops-gl's by-name lookup answer what this game loads?
#
# `OGL_Support.c:132,135` binds two entry points through `SDL_GL_GetProcAddress`, and a lookup that
# answers NULL is a null call at run time, not a link error, so nothing else catches it. q3rally's
# `scripts/gl-surface.sh` is not reused here: it parses ioquake3's `qgl.h` macro groups, which this
# title has no equivalent of.
#
# Every name is read out of the source rather than listed here, so a name added upstream is checked
# too. The table is compared by expanding `OOPS_GL_PROC_LIST` with the preprocessor, which is why it
# does not matter that the list is split across `gl_procs.h` and `gl_procs_core.h`.
#
# Two assertions, because both directions of this check can lie. The name count must be non-zero, or a
# grep that matched nothing reports a clean surface; and a control name that is deliberately not in
# oops-gl must come back missing, or the table match is wrong rather than the title. `#n` stringifies,
# so the expanded table holds `["glActiveTexture"]` with the quotes.
# ---------------------------------------------------------------------------
.PHONY: glsurface
glsurface:
	@set -e; t=$$(mktemp -d); trap 'rm -rf $$t' EXIT; \
	 printf '#include "gl_procs.h"\n#define X(n) [#n]\n#define XS(n, s) [#n #s]\nOOPS_GL_PROC_LIST(X, XS)\n' \
	   > $$t/expand.c; \
	 $(TARGET_CC) -E -P -I $(OOPS_SDK_DIR)/src/gl $$t/expand.c > $$t/table; \
	 if grep -q '\["glNotAnEntryPointAtAll"\]' $$t/table; then \
	    echo "bugdom2: the control name matched - the table match is wrong" >&2; exit 1; fi; \
	 grep -rhoE 'SDL_GL_GetProcAddress *\( *"[A-Za-z0-9_]+"' $(BUG2_UP)/Source \
	   | sed 's/.*"\(.*\)"/\1/' | sort -u > $$t/wanted; \
	 n=$$(wc -l < $$t/wanted); \
	 if [ "$$n" -eq 0 ]; then \
	    echo "bugdom2: found no SDL_GL_GetProcAddress call - the grep is wrong, not the title" >&2; \
	    exit 1; fi; \
	 miss=0; \
	 while read -r name; do \
	    if grep -q "\[\"$$name\"\]" $$t/table; then printf '  ok      %s\n' "$$name"; \
	    else printf '  MISSING %s\n' "$$name"; miss=$$((miss+1)); fi; \
	 done < $$t/wanted; \
	 printf 'bugdom2 GL by-name surface: %s looked up, %s not in oops-gl\n' "$$n" "$$miss"; \
	 [ "$$miss" -eq 0 ]

# ---------------------------------------------------------------------------
# `make package`: the title package with the game data, which is what runs
#
# `common/app.mk`'s `title` builds the eboot and `sce_sys/`, and stops there - a title's content is
# the title's business. Bugdom 2's is `Data/`, shipped in the upstream repository, and the entry point
# probes for `Data/Skeletons/Grasshopper.bg3d` under the package root.
# ---------------------------------------------------------------------------
.PHONY: package
package: title
	@test -d upstream/Data || { echo "bugdom2: no upstream/Data - run make first" >&2; exit 1; }
	@echo "bugdom2: copying $$(du -sh upstream/Data | cut -f1) of data into the package"
	@rm -rf build/title/$(TITLE_ID)/Data
	@cp -r upstream/Data build/title/$(TITLE_ID)/Data
	@# The file the shim probes for, so a truncated copy fails here rather than on the console.
	@test -f build/title/$(TITLE_ID)/Data/Skeletons/Grasshopper.bg3d || { \
	    echo "bugdom2: Data/Skeletons/Grasshopper.bg3d is missing from the package" >&2; exit 1; }
	@echo "bugdom2: package is build/title/$(TITLE_ID) ($$(du -sh build/title/$(TITLE_ID) | cut -f1), $$(find build/title/$(TITLE_ID) -type f | wc -l) files)"
