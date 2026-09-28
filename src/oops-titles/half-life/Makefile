# Half-Life, through oops-sdk.
#
#   make           the survey - what stands between this title and a link
#   make title     the payload, packaged - once HALF_ARMED=1 links
#
# Xash3D FWGS (`upstream/`) running hlsdk-portable's game code (`upstream-hlsdk/`), both linked
# into one payload under upstream's own `XASH_STATIC_LIBS` mode. The player's `valve/` directory is
# the data, read at run time.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# `ref/gl`, the engine's OpenGL renderer, is fixed-function GL 1.x. `ref/soft` is the fallback
# if it is not.
OOPS_RENDERER = gl1

include $(OOPS_APPS_ROOT)/common/upstream.mk

# SDL2 for the window, input and audio. The codecs and compression the engine carries as
# submodules under `3rdparty/` are the ones already vendored here, so the port takes these rather
# than a second copy. `mainui` (the menu) is a submodule too, and builds from `upstream/`.
SCAFFOLD_ARM    := HALF_ARMED
SCAFFOLD_DEPS   := sdl2 libcxx zlib libogg libvorbis opus opusfile
SCAFFOLD_SUPPLY := the valve/ folder from the player's own Half-Life install, as \
                   /data/homebrew/HALF00001/valve/

# `net` for the engine's loopback server, which a single-player game runs through.
OOPS_FEATURES := display input keyboard mouse audio suspend thread draw dialog net

# Arming is where the engine, `ref/gl`, `mainui`, and hlsdk's `cl_dll` and `dlls` are
# transcribed from their wscripts, with `-DXASH_STATIC_LIBS` and a
# `shim/include/generated_library_tables.h` - the table waf generates
# (`scripts/waifulib/xshlib.py:112`) that maps each library name to its exports. Until then an
# armed build says so rather than linking a payload of three files.
ifeq ($(HALF_ARMED),1)
$(error half-life: the source lists are not transcribed yet - `make survey` says what is next)
endif

include $(OOPS_APPS_ROOT)/common/app.mk
include $(OOPS_APPS_ROOT)/common/scaffold.mk
