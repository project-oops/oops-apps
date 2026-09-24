# Extreme Tux Racer, through oops-sdk.
#
#   make elf      the plain-ELF payload a homebrew loader runs
#   make title    complete title package (.zip)
#
# **This does not build yet, and the reason is one dependency.** ETR's 45 sources compile against
# everything here *except* a C++ standard library: `bh.h` includes <string>, <map>, <list> and
# <iostream>, and nothing on this target provides them. Every other gap is closed - measured, not
# assumed. See README.md for the numbers.
#
# The Makefile is written now rather than when that lands, because it is what says what the title
# is made of, and because the pieces it names are all real and all tested.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# **First, because the next thing this file does is look inside `upstream/`.** The fetch is
# normally done by `common/app.mk`, which is included at the bottom - too late for a wildcard
# forty lines above it. On an already-fetched tree that read correctly and on a fresh checkout
# it matched nothing, so the title built without its own 45 sources and the *second* `make` was
# the right one. `common/upstream.mk` says the rest.
include $(OOPS_APPS_ROOT)/common/upstream.mk

# --- C++ -------------------------------------------------------------------
#
# ETR's own C++, plus our entry point. Everything upstream, compiled under upstream's flags by
# `cxx.mk`; nothing of it is committed here.
#
# **Above the include, not below it**, which is where it used to be: `cxx.mk` names these as
# prerequisites of the archive and make expands a rule's prerequisites as it reads the rule, so
# setting them afterwards left the archive with none. Editing any of ETR's own C++ then rebuilt
# nothing. `cxx.mk` now refuses an empty list rather than letting that happen quietly.
OOPS_CXX_SRCS := $(wildcard upstream/*.cpp) $(wildcard shim/*.cpp)

# `-fno-exceptions -fno-rtti` and the `new`/`delete`/guard runtime, shared with every C++ title.
# ETR is the one that pays for it first: **0 `throw` and 0 `dynamic_cast`**, measured, which is
# what makes those flags honest here and what rules Armagetron out of the same treatment.
include $(OOPS_APPS_ROOT)/common/cxx.mk

# --- SDL2 ------------------------------------------------------------------
#
# **SDL2, not sdl12-compat.** The mirror this title pins is already ported to SDL2 - see
# `upstream.lock`, which compares all three. `oops-deps/sdl12-compat` exists and links, and is
# simply not on this title's path.
OOPS_SDL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2)
include $(OOPS_SDL)/oops-sdl.mk

# --- the rest of the stack ------------------------------------------------
OOPS_LIBCXX ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libcxx)
include $(OOPS_LIBCXX)/oops-libcxx.mk

OOPS_FT ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/freetype)
include $(OOPS_FT)/oops-freetype.mk

# After oops-sdl.mk: the mixer needs SDL2's headers on the path and does not include it itself.
OOPS_MIX ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2-mixer)
include $(OOPS_MIX)/oops-sdl2-mixer.mk

# SDL2_image, for the one `IMG_Load` in `textures.cpp` that reads all 149 of this title's PNGs.
#
# **Three includes for one function, and in this order.** `oops-libpng.mk` reads
# `$(OOPS_ZLIB_INCLUDE)` as it is read, so zlib is first; `oops-sdl2-image.mk` reads libpng's and
# SDL2's for the same reason, so it is last. Each of these was already pinned and already
# compiling - Neverball has been shipping libpng and zlib - which is why the whole of "this title
# can read its own artwork" is three lines here and no new code anywhere.
OOPS_ZLIB ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/zlib)
include $(OOPS_ZLIB)/oops-zlib.mk

OOPS_PNG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libpng)
include $(OOPS_PNG)/oops-libpng.mk

OOPS_IMG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2-image)
include $(OOPS_IMG)/oops-sdl2-image.mk

# The shared POSIX shim - `stat`, `opendir`, `getcwd` and the rest. Shared with Neverball, which
# is why it lives in `common/` rather than here.
include $(OOPS_APPS_ROOT)/common/posix.mk

# `OS_LINUX` picks the POSIX arm of upstream's own `#if` chain in `bh.h`, and the shim satisfies
# what that arm asks for. That pair is why this title needs **no patch to ETR at all**: the
# platform layer is chosen by a define and answered from outside upstream's tree.
#
# `-include GL/glext.h` is the same idea one step further. ETR's Linux branch includes
# `<GL/gl.h>` and `<GL/glu.h>` but not `<GL/glext.h>`, and expects the system GL headers to have
# brought the compiled-vertex-array typedefs with them. Forcing our own in on the command line
# answers that without editing upstream's include list.
#
# `-Wno-c++11-narrowing` is for upstream's own braced initialisers, which narrow `double` to
# `GLfloat` in about twenty-seven places. That was legal when ETR was written.
ETR_CFLAGS := -DOS_LINUX=1 -Ishim/include -include GL/glext.h -Wno-c++11-narrowing \
              -DOOPS_POSIX_HOME=\"/data/extreme-tux-racer\" \
              $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE) $(OOPS_MIX_INCLUDE) $(OOPS_FT_INCLUDE) \
              $(OOPS_IMG_INCLUDE)

OOPS_CXX_INCLUDE     += $(OOPS_LIBCXX_INCLUDE)
EXTRA_TARGET_CFLAGS  += $(ETR_CFLAGS)
# `$(OOPS_IMG_LDFLAGS)` carries libpng and zlib behind it, so they are not named again here.
EXTRA_TARGET_LDFLAGS += $(OOPS_CXX_LDFLAGS) $(OOPS_SDL_LDFLAGS) $(OOPS_MIX_LDFLAGS) \
                        $(OOPS_FT_LDFLAGS) $(OOPS_IMG_LDFLAGS) $(OOPS_LIBCXX_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(OOPS_CXX_LIB) $(OOPS_SDL_LIB) $(OOPS_MIX_LIB) \
                        $(OOPS_FT_LIB) $(OOPS_IMG_LIB) $(OOPS_PNG_LIB) $(OOPS_ZLIB_LIB) \
                        $(OOPS_LIBCXX_LIB)

# The C half of the payload: the shared POSIX shim and the SDK. It is C rather than C++ because
# it implements a C ABI that upstream's headers declare.
PAYLOAD_SRCS = $(OOPS_POSIX_SRCS) \
               $(OOPS_GL_SRCS) \
               $(OOPS_SDK_DIR)/src/display.c \
               $(OOPS_SDK_DIR)/src/agc/agc_display.c \
               $(OOPS_SDK_DIR)/src/agc/agc_tiler.c \
               $(OOPS_SDK_DIR)/src/agc/agc_compute.c \
               $(OOPS_SDK_DIR)/src/input/input.c \
               $(OOPS_SDK_DIR)/src/input/keyboard.c \
               $(OOPS_SDK_DIR)/src/input/mouse.c \
               $(OOPS_SDK_DIR)/src/audio/audio.c \
               $(OOPS_SDK_DIR)/src/memory/memory.c \
               $(OOPS_SDK_DIR)/src/system/system.c \
               $(OOPS_SDK_DIR)/src/system/suspend.c \
               $(OOPS_SDK_DIR)/src/system/freestd.c \
               $(OOPS_SDK_DIR)/src/system/syscall.c \
               $(OOPS_SDK_DIR)/src/system/sysmodule.c \
               $(OOPS_SDK_DIR)/src/thread/thread.c \
               $(OOPS_SDK_DIR)/src/draw/draw.c

ENTRY_POINT := etr_start

include $(OOPS_APPS_ROOT)/common/app.mk
