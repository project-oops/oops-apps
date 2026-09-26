# Bugdom 2, through oops-sdk.
#
#   make               the payload
#   make title         the payload, packaged
#   make census        compile every source this title needs, for the target, and report
#   make census-one F=<path> [X=1]   every error in one source, C or C++
#   make glsurface     whether oops-gl's by-name lookup answers the entry points this game loads
#
# **Most of this title is Bugdom's.** Same author, same engine lineage, same Pomme, same SDL2, same
# freeware data in the repository - so the expensive questions were answered next door and this file
# is deliberately the same shape as `../bugdom/Makefile`. Where it differs, it says why.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# **`OOPS_SDK_DIR` early, because this file names files under it before `app.mk` is included.**
# `app.mk` defines it at its line 11, too late for an immediately-expanded (`:=`) list further down.
# The `?=` matches app.mk's own default, so including app.mk afterwards changes nothing.
OOPS_SDK ?= $(abspath $(OOPS_APPS_ROOT)/../oops-sdk)
OOPS_SDK_DIR := $(abspath $(OOPS_SDK))

# **`gl1`, and `glu` with it.** Bugdom 2 renders through fixed-function OpenGL - `glBegin`,
# `glVertex3f`, `glTexEnvi` - and its camera is `gluLookAt`/`gluPerspective`, which is why
# `OOPS_FEATURES` names `glu` where Bugdom's does not.
#
# **71 entry points, all defined by oops-gl.** Three names the grep finds are not calls:
# `glLockArraysEXT` and `glUnlockArraysEXT` are commented out at `Source/3D/MetaObjects.c:756,759`,
# and `glTextureName` is a local variable in `OGL_Support.c:848`. Checked, because two of the three
# would otherwise have read as a missing compiled-vertex-array extension.
#
# **Unlike Bugdom, this one uses a loader**, so the by-name trap does apply: `OGL_Support.c:132,135`
# binds `glActiveTexture` and `glClientActiveTexture` through `SDL_GL_GetProcAddress`. Both are in
# oops-gl's `gl_procs.h` table - `make glsurface` is the standing check, and q3rally is the title
# that needed it: it linked clean while 66 entry points it looked up by name answered NULL.
OOPS_RENDERER = gl1

# **First, because this file names files under `upstream/` while it is being read.**
include $(OOPS_APPS_ROOT)/common/upstream.mk

BUG2_UP := $(CURDIR)/upstream

# --- the source list -------------------------------------------------------
#
# **A wildcard, and here that is upstream's own answer rather than a shortcut.**
# `CMakeLists.txt:80` is `file(GLOB_RECURSE GAME_SOURCES ... ${GAME_SRCDIR}/*.c *.cpp)`, so the
# directory *is* the list. Every C source sits exactly one directory below `Source/`; `Boot.cpp` is
# the only C++ and the only file directly in it.
BUG2_C_SRCS := $(sort $(wildcard $(BUG2_UP)/Source/*.c) $(wildcard $(BUG2_UP)/Source/*/*.c))
BUG2_OWN_CXX := $(sort $(wildcard $(BUG2_UP)/Source/*.cpp) $(wildcard $(BUG2_UP)/Source/*/*.cpp))

# --- Pomme, which is most of the platform layer ----------------------------
#
# Pomme is jorio's reimplementation of the classic Mac OS toolbox - Resource Manager, Sound Manager,
# QuickDraw - and Bugdom 2 is a Mac game, so this is not an optional dependency.
#
# **A different revision from Bugdom's**, and that is upstream's choice rather than an oversight:
# Bugdom 1.3.4's tree records Pomme at `ef94150e`, this one at `c6a38eab`. One pinned hash still
# pins everything, because the submodule revision lives in the pinned commit's own tree.
#
# **Its list is transcribed, not globbed, because five of its subsystems are switched off.**
# `CMakeLists.txt:54-58` sets `POMME_NO_VIDEO`, `_INPUT`, `_GRAPHICS`, `_QD3D` and `_MP3`, and each
# both drops sources *and* defines a macro the remaining sources compile against. Globbing
# `extern/Pomme/src` would compile the fifteen that are off, against headers those very macros empty.
#
# **`POMME_NO_QD3D` is the one Bugdom does not set.** Bugdom draws through Pomme's QuickDraw 3D
# reimplementation; Bugdom 2 brought its own `Source/3D`, so Pomme's three QD3D sources are off here
# and the `-I.../QD3D` include Bugdom needs is absent from this title's include list - upstream's
# `target_include_directories` (`CMakeLists.txt:123`) names only `extern/Pomme/src` and
# `Source/Headers`, and no source here includes `QD3D.h` by bare name. Checked, not assumed.
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

# **SDL2, and that is what the pinned revision asks for** - `find_package(SDL2 REQUIRED COMPONENTS
# main)` at `CMakeLists.txt:68`, `#include <SDL.h>` throughout. `oops-deps/sdl3` exists and is
# complete; neither Bugdom pin needs it. See `../bugdom/docs/PORTING.md` for how that was nearly got
# wrong: a survey read `find_package(SDL3 ...)` out of a clone of *master*.
OOPS_SDL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2)
include $(OOPS_SDL)/oops-sdl.mk

# **libc++, and libunwind and libc++abi with it.** Bugdom 2 throws: `Boot.cpp` wraps the whole game
# in `catch (std::exception&)`, and `std::filesystem` reports failures as `filesystem_error`. The
# platform exports no Itanium unwind or C++ ABI symbols, so both come from here (oops-apps#D005).
#
# **`OOPS_CXX_EXCEPTIONS` has to be set before this include.** `oops-libcxx.mk:66` reads it to choose
# between `build-eh/libc++.a` and `build/libc++.a`, with `?=`, at include time - so setting it
# afterwards silently takes the no-exceptions archive, which defines `std::terminate`,
# `std::bad_alloc` and eighteen more itself instead of deferring to libc++abi. Bugdom's link failed
# on exactly that, as twenty duplicate symbols naming neither archive nor the ordering.
OOPS_CXX_EXCEPTIONS = 1
OOPS_CXX_STD        = c++20

OOPS_LIBCXX ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libcxx)
include $(OOPS_LIBCXX)/oops-libcxx.mk
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk

# **`libc++` owns `operator new`, and a title linking the full archive must say so.**
#
# `common/cxxrt.cpp` defines `operator new`/`delete` over `oops_malloc`, and `oops-libcxxabi.mk`
# excludes libc++abi's `stdlib_new_delete.cpp` to match. But `libc++.a` *also* defines them, in its
# own `__lcxx_override` section, **with a guard in each one asserting that `&operator new` lies
# inside that section**. `--allow-multiple-definition` lets cxxrt's win, at an address outside
# `__lcxx_override`, so libc++'s own nothrow `operator new` fails its guard and executes a `ud2`: a
# privileged-instruction fault at the first libc++ allocation, which for Pomme is before the first
# frame.
#
# OOPSy-daisy found this on hardware on 2026-09-25 and the fix is confirmed there. Allocation still
# reaches the SDK heap, because libc++'s `operator new` calls `malloc`, which *is* `oops_malloc`.
EXTRA_TARGET_CFLAGS += -DOOPS_CXX_EXTERNAL_NEW_DELETE

# **The include list is upstream's, transcribed** - `CMakeLists.txt:123-128`. `GL_SILENCE_DEPRECATION`
# comes from `target_compile_definitions` two blocks down and is harmless here, but it is upstream's
# flag and upstream's flags are not edited out on a guess.
BUG2_CFLAGS := -I$(BUG2_UP)/Source -I$(BUG2_UP)/Source/Headers \
               -I$(POMME_SRC) \
               $(POMME_DEFINES) -DGL_SILENCE_DEPRECATION \
               -DOOPS_POSIX_HOME=\"/data/bugdom2\" \
               $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE)

# **Upstream's code under upstream's warnings.** `common/app.mk` compiles at `-Werror` with a set
# stricter than this codebase was written for. Switched off rather than patched away: a patch against
# somebody else's tree is a rebase burden for no behaviour change. The list grows only when the
# census names a new one.
BUG2_WARNINGS := -Wno-strict-prototypes -Wno-missing-prototypes -Wno-conversion \
                 -Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-comment \
                 -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-shadow \
                 -Wno-sign-compare -Wno-char-subscripts -Wno-unused-function \
                 -Wno-unused-const-variable -Wno-missing-field-initializers \
                 -Wno-unused-variable -Wno-multichar

EXTRA_TARGET_CFLAGS += $(BUG2_CFLAGS) $(BUG2_WARNINGS)

# **The C++ flags for `census` only**, with `OOPS_LIBCXX_INCLUDE` ahead of the C headers:
# libc++'s `<errno.h>` and friends wrap the C ones with `#include_next`, and `<cerrno>` refuses to
# build without them. `oops-stormlib.mk` had this ordering backwards and built anyway, because its
# C++ never includes `<cmath>`.
#
# Recursive (`=`), because `TARGET_CFLAGS` is not known until `common/app.mk` has been read.
BUG2_CXX_FLAGS = -x c++ -std=c++20 $(OOPS_LIBCXX_INCLUDE) \
                 $(filter-out -std=c11 -Wstrict-prototypes -Wmissing-prototypes,$(TARGET_CFLAGS)) \
                 $(EXTRA_TARGET_CFLAGS)

# --- the link --------------------------------------------------------------
#
# `shim/bugdom2_start.cpp` is the entry point and the only source here that is ours.
#
# **C++, and it has to be.** In a freestanding C++ translation unit `main` is an ordinary function,
# so `Boot.cpp` exports it *mangled* - `_Z4mainiPPc`. A C shim asking for `main` links, because a
# payload link ignores unresolved symbols, and `app.mk`'s guard is the only thing between that and a
# fault at the first instruction on the console. So this goes in `OOPS_CXX_SRCS`.
BUG2_SHIM_CXX := shim/bugdom2_start.cpp

# **The C++ half goes through `common/cxx.mk`**, which compiles it with its own flags: `app.mk`
# builds every `PAYLOAD_SRCS` entry as C11, which is wrong for C++ and actively wrong for C++20.
#
# `OOPS_CXX_SRCS` is set **before** the include on purpose - `cxx.mk` names it as a prerequisite of
# the archive, and make expands a rule's prerequisites when it reads the rule, so a title that set it
# afterwards got an archive that never rebuilt when its own C++ changed.
#
# No `OOPS_CXX_LINK_OBJECTS`: no two sources in the list share a basename. `oops_ar_check` says so.
OOPS_CXX_SRCS := $(POMME_CXX) $(BUG2_OWN_CXX) $(BUG2_SHIM_CXX)
include $(OOPS_APPS_ROOT)/common/cxx.mk

# **After the include, because `cxx.mk` clears it** - and only libc++'s headers go here. The rest of
# this title's include path reaches C++ through `EXTRA_TARGET_CFLAGS`, which `OOPS_CXX_FLAGS` appends
# last; naming `BUG2_CFLAGS` here as well would put the POSIX shim's headers *ahead* of libc++'s.
OOPS_CXX_INCLUDE += $(OOPS_LIBCXX_INCLUDE)

PAYLOAD_SRCS := $(BUG2_C_SRCS) $(OOPS_POSIX_SRCS)

# **The SDK subsystems, as capabilities.** `glu` because the camera is `gluLookAt`/`gluPerspective`.
# No `net`: the game has no networking.
OOPS_FEATURES := glu display input keyboard mouse audio suspend thread draw

# **Each archive after everything that calls it.** `app.mk` puts LDFLAGS ahead of the payload
# objects, and a plain archive seen before its callers contributes nothing - so the title's own C++
# first, then SDL2, then the three standard-library archives innermost-last: libc++ calls libc++abi,
# which calls libunwind.
EXTRA_TARGET_LDFLAGS += $(OOPS_CXX_LDFLAGS) $(OOPS_SDL_LDFLAGS) \
                        $(OOPS_LIBCXX_LDFLAGS) $(OOPS_LIBCXXABI_LDFLAGS) $(OOPS_LIBUNWIND_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(OOPS_CXX_LIB) $(OOPS_SDL_LIB) \
                        $(OOPS_LIBCXX_LIB) $(OOPS_LIBCXXABI_LIB) $(OOPS_LIBUNWIND_LIB)

include $(OOPS_APPS_ROOT)/common/app.mk

# ---------------------------------------------------------------------------
# `make census`: which of this title's sources compile for the target, today
#
# One file at a time, `-fsyntax-only`, each blocker printed **with the source that hit it** so that
# clearing one does not start with a grep to find out where it was. It survives the arming because it
# is the cheaper half of a build when a dependency bump breaks the tree, and a `-j` link is not.
#
# **A clean census is not a working port.** `-fsyntax-only` does not link, and a link is where a
# missing driver or GL entry point shows up.
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
# **Two assertions, because both directions of this check can lie.** The name count must be non-zero,
# or a grep that matched nothing reports a clean surface; and a name that is deliberately not in
# oops-gl must come back missing, or the table match is wrong rather than the title. The first draft
# of this recipe failed the second one: `#n` stringifies, so the expansion is `["glActiveTexture"]`
# with the quotes, and a search for `[glActiveTexture]` reported every name missing.
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
