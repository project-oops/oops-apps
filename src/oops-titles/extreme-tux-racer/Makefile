# Extreme Tux Racer, through oops-sdk.
#
#   make elf      the plain-ELF payload a homebrew loader runs
#   make title    complete title package (.zip) - the payload and sce_sys, no content
#   make package  the title package **with ETR's 55 MB of data**, which is what runs
#
# All 45 of ETR's sources and the whole stack under them - libc++, SDL2, SDL2_image, SDL2_mixer,
# freetype, libpng, zlib - compile and link here, and `make package` is the runnable result. The
# C++ standard library that was this title's last gap is `src/oops-deps/libcxx`.

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
# **Two includes, and zlib is not one of them.** `oops-libpng.mk` includes `oops-zlib.mk` itself,
# so naming zlib here too defines every one of its rules twice - make warns "overriding recipe for
# target .../libz.a" and takes the second. `oops-sdl2-image.mk` reads libpng's and SDL2's
# variables as it is read, so it goes after both.
#
# Each of these was already pinned and already compiling - Neverball has been shipping libpng and
# zlib - which is why the whole of "this title can read its own artwork" is two lines here and no
# new code anywhere.
OOPS_PNG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libpng)
include $(OOPS_PNG)/oops-libpng.mk

# libjpeg because SDL2_image decodes JPEG for every title that shares it - SuperTux's backgrounds
# are JPEGs - and its link names the decoder whether or not this title ever loads one.
OOPS_JPEG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libjpeg-turbo)
include $(OOPS_JPEG)/oops-libjpeg.mk

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
#
# **`OOPS_POSIX_HOME` is `/app0`, and it was `/data/extreme-tux-racer` until hardware said no.**
# `/data` is outside the app's sandbox: measured on 2026-09-24 with the SDK's `fs` channel, the very
# first thing this title does is `open("/data")` and it fails, so a directory under it can be
# neither created nor written and every setting was silently lost. `/app0` is inside the sandbox and
# is writable - Neverball's shim measured that separately, and its notes say so. It also puts the
# config beside the data ETR already reads from `/app0//data`, which is where upstream expects both.
#
# The cost is honest and worth stating: `/app0` is the package, so `make package` plus a restore
# replaces it, and settings do not survive a redeploy. A savedata mount is the arrangement that
# would - Neverball has one - and it is a bigger change than this title needs to start rendering.
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
# **`make package` is the one to run**, not `make title`: a title package built by `app.mk` holds
# the payload and `sce_sys/`, and a *port* also ships the program's own content. ETR's is 55 MB in
# 432 files - 283 PNGs, 93 course and object lists, the music and the fonts - and it reaches none
# of its own menus without them.
#
# **Where it has to land is decided by upstream, not by us.** `game_config.cpp:313` sets
# `data_dir = prog_dir + "/data"`, and `prog_dir` is `argv[0]` with its last three characters cut
# off - so `shim/etr_start.cpp`'s `/app0/etr` makes the data directory `/app0/data`. That is why
# this copies into the package rather than restoring beside it, and it is also why no patch is
# needed to tell ETR where its content is.
#
# There is no host-side compile step here, which is the one way this is cheaper than Neverball:
# ETR's data ships in the form the game reads.
#
# This is the **second** title to need this, Neverball's `package` being the first, and its comment
# says `app.mk` "has no hook for that and should not grow one for a single title". Two is still not
# three: a third port that ships content is the one that should make this shared rather than
# copied, and at that point both of these become one `TITLE_DATA_DIR` line.
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
