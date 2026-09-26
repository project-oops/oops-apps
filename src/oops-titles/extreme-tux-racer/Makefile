# Extreme Tux Racer, through oops-sdk.
#
#   make elf      the plain-ELF payload a homebrew loader runs
#   make title    complete title package (.zip) - the payload and sce_sys, no content
#   make package  the title package with ETR's data, which is what runs
#
# ETR's sources and the stack under them - libc++, SDL2, SDL2_image, SDL2_mixer, freetype,
# libpng, zlib - compile and link here.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# First, because the wildcard below reads `upstream/`, and `common/app.mk` (which normally
# fetches it) is included at the bottom. `common/upstream.mk` explains.
include $(OOPS_APPS_ROOT)/common/upstream.mk

# --- C++ -------------------------------------------------------------------
#
# ETR's own C++, plus our entry point, compiled under upstream's flags by `cxx.mk`.
# Set above the include: `cxx.mk` names these as archive prerequisites, which make expands
# as it reads the rule.
OOPS_CXX_SRCS := $(wildcard upstream/*.cpp) $(wildcard shim/*.cpp)

# `-fno-exceptions -fno-rtti` and the `new`/`delete`/guard runtime, shared with every C++ title.
# ETR has no `throw` and no `dynamic_cast`, so those flags hold here.
include $(OOPS_APPS_ROOT)/common/cxx.mk

# --- SDL2 ------------------------------------------------------------------
#
# SDL2, not sdl12-compat: the pinned mirror is already ported to SDL2 (see `upstream.lock`).
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

# SDL2_image, for the `IMG_Load` in `textures.cpp` that reads this title's PNGs.
# zlib is not included here: `oops-libpng.mk` includes `oops-zlib.mk` itself, and a second
# include defines its rules twice. `oops-sdl2-image.mk` reads libpng's and SDL2's variables
# as it is read, so it goes after both.
OOPS_PNG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libpng)
include $(OOPS_PNG)/oops-libpng.mk

# libjpeg because SDL2_image decodes JPEG for every title that shares it - SuperTux's backgrounds
# are JPEGs - and its link names the decoder whether or not this title ever loads one.
OOPS_JPEG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libjpeg-turbo)
include $(OOPS_JPEG)/oops-libjpeg.mk

OOPS_IMG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2-image)
include $(OOPS_IMG)/oops-sdl2-image.mk

# The shared POSIX shim - `stat`, `opendir`, `getcwd` and the rest.
include $(OOPS_APPS_ROOT)/common/posix.mk

# `OS_LINUX` picks the POSIX arm of upstream's `#if` chain in `bh.h`, and the shim answers
# what that arm asks for, so ETR needs no patch.
# `-include GL/glext.h`: ETR's Linux branch expects the system GL headers to bring the
# compiled-vertex-array typedefs; `shim/include/GL/glext.h` supplies them.
# `-Wno-c++11-narrowing`: upstream's braced initialisers narrow `double` to `GLfloat`.
# `OOPS_POSIX_HOME` is `/app0` because `/data` is outside the app sandbox. `/app0` is the
# package, so settings do not survive a redeploy; a savedata mount would keep them.
ETR_CFLAGS := -DOS_LINUX=1 -Ishim/include -include GL/glext.h -Wno-c++11-narrowing \
              -DOOPS_POSIX_HOME=\"/app0\" \
              $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE) $(OOPS_MIX_INCLUDE) $(OOPS_FT_INCLUDE) \
              $(OOPS_IMG_INCLUDE)

OOPS_CXX_INCLUDE     += $(OOPS_LIBCXX_INCLUDE)
EXTRA_TARGET_CFLAGS  += $(ETR_CFLAGS)
# `$(OOPS_IMG_LDFLAGS)` carries libpng, zlib and libjpeg behind it, so they are not named again here.
EXTRA_TARGET_LDFLAGS += $(OOPS_CXX_LDFLAGS) $(OOPS_SDL_LDFLAGS) $(OOPS_MIX_LDFLAGS) \
                        $(OOPS_FT_LDFLAGS) $(OOPS_IMG_LDFLAGS) $(OOPS_LIBCXX_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(OOPS_CXX_LIB) $(OOPS_SDL_LIB) $(OOPS_MIX_LIB) \
                        $(OOPS_FT_LIB) $(OOPS_IMG_LIB) $(OOPS_PNG_LIB) $(OOPS_ZLIB_LIB) $(OOPS_JPEG_LIB) \
                        $(OOPS_LIBCXX_LIB)

OOPS_RENDERER = gl1
OOPS_FEATURES := glu display input keyboard mouse audio suspend thread draw

PAYLOAD_SRCS = $(OOPS_POSIX_SRCS)

ENTRY_POINT := etr_start

include $(OOPS_APPS_ROOT)/common/app.mk

# --- the data -------------------------------------------------------------
#
# `make package` adds ETR's content (textures, course lists, music, fonts) to the title
# package; the game reaches none of its menus without it. The data ships in the form the game
# reads, so there is no host-side compile step.
#
# `game_config.cpp:313` sets `data_dir = prog_dir + "/data"`, where `prog_dir` is `argv[0]`
# less its last three characters, so `shim/etr_start.cpp`'s `/app0/etr` puts the data at
# `/app0/data` - inside the package.
.PHONY: package
package: title
	@test -d upstream/data || { echo "extreme-tux-racer: no upstream/data - run make first" >&2; exit 1; }
	@echo "extreme-tux-racer: copying $$(du -sh upstream/data | cut -f1) of data into the package"
	@rm -rf build/title/$(TITLE_ID)/data
	@cp -r upstream/data build/title/$(TITLE_ID)/data
	@# **The controls card's artwork, copied in here rather than into `upstream/data`.** The
	@# help screen loads it by name out of the texture directory, but it is ours and shared
	@# with every other port - so it goes into the *packaged* data and upstream's tree stays
	@# exactly as upstream ships it. See `common/assets/controls/`.
	@cp $(OOPS_APPS_ROOT)/common/assets/controls/controller.png \
	    build/title/$(TITLE_ID)/data/textures/oops-controller.png
	@echo "extreme-tux-racer: package is build/title/$(TITLE_ID) ($$(du -sh build/title/$(TITLE_ID) | cut -f1))"
