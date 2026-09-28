# SuperTuxKart, through oops-sdk.
#
#   make           the survey - what stands between this title and a link
#   make title     the payload, packaged - once STKT_ARMED=1 links
#
# The game (`upstream/`, stk-code 1.5) with its data (`upstream-assets/`, the matching release
# download). Both are free, so the package is complete and the player supplies nothing. A third
# racing title beside `../extreme-tux-racer`, not a replacement for it.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Its renderer is OpenGL 3.3 (or GLES 3.0), with no GL 2 path left in 1.x: that is Mesa's here.
# The Vulkan renderer is off (`NO_SHADERC`).
OOPS_RENDERER = mesa

include $(OOPS_APPS_ROOT)/common/upstream.mk

# AngelScript, Bullet, ENet, the Irrlicht fork, libsquish, mcpp, SheenBidi and tinygettext are
# bundled under `lib/` and build from upstream. HarfBuzz shapes the text FreeType draws. cURL is
# required by the build (CMakeLists.txt:554) for add-ons and online play, and the crypto behind
# them is MbedTLS or OpenSSL; neither is used offline, but both must link. SQLite is optional
# (`USE_SQLITE3`, server statistics) and is off.
#
# The assets are 4,279 files (273 MB unpacked): they ship as one archive and unpack at first
# run, since `pros restore` costs per file.
SCAFFOLD_ARM    := STKT_ARMED
SCAFFOLD_DEPS   := sdl2 libcxx openal-soft libogg libvorbis freetype harfbuzz libpng \
                   libjpeg-turbo zlib curl mbedtls
SCAFFOLD_SUPPLY := nothing - the game and its data are free and ship in the package

OOPS_FEATURES := display input keyboard mouse audio suspend thread draw dialog net

# Arming is where the game's source list (`sources.cmake`) and the bundled libraries' are
# transcribed, with `NO_SHADERC=ON`, `shim/stkt_start.cpp` and `common/crashtrace.c`, and a
# `package` target that stages `upstream/data` and the assets. Until then an armed build says so
# rather than linking a payload of two files.
ifeq ($(STKT_ARMED),1)
$(error supertuxkart: the source lists are not transcribed yet - `make survey` says what is next)
endif

include $(OOPS_APPS_ROOT)/common/app.mk
include $(OOPS_APPS_ROOT)/common/scaffold.mk
