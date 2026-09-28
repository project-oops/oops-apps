# Mineclonia, through oops-sdk.
#
#   make           the survey - what stands between this title and a link
#   make title     the payload, packaged - once MNCL_ARMED=1 links
#
# The Luanti engine (`upstream/`) with the Mineclonia game (`upstream-game/`) packaged under
# `games/mineclonia`. Both are free, so the package is complete and the player supplies nothing.
# A separate title from `../craft`, which is its own small engine.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Luanti's Irrlicht fork (`irr/`) draws through its OpenGL driver, GL 2.x with GLSL, in a window
# SDL2 opens.
OOPS_RENDERER = gl2

include $(OOPS_APPS_ROOT)/common/upstream.mk

# Lua 5.1 (with `bitop`), jsoncpp, mini-gmp and the glTF reader are bundled under `lib/` and build
# from upstream, so no LuaJIT. SQLite is the world store and zstd compresses map blocks; both are
# required. cURL, gettext, LevelDB, PostgreSQL, OpenSSL and ncurses are options, all off: nothing
# here downloads content, and a single-player world lives in SQLite.
SCAFFOLD_ARM    := MNCL_ARMED
SCAFFOLD_DEPS   := sdl2 libcxx openal-soft libogg libvorbis freetype libpng libjpeg-turbo zlib \
                   zstd sqlite
SCAFFOLD_SUPPLY := nothing - the engine and the game are both free and ship in the package

# `net` for the engine's loopback server, which a single-player world runs through.
OOPS_FEATURES := display input keyboard mouse audio suspend thread draw dialog net

# Arming is where the engine's and Irrlicht's source lists are transcribed from their
# CMakeLists.txt, with `RUN_IN_PLACE=1`, `shim/mncl_start.cpp` and `common/crashtrace.c`, and a
# `package` target that copies `upstream/builtin`, `client`, `fonts`, `textures` and the game.
# Until then an armed build says so rather than linking a payload of two files.
ifeq ($(MNCL_ARMED),1)
$(error mineclonia: the source lists are not transcribed yet - `make survey` says what is next)
endif

include $(OOPS_APPS_ROOT)/common/app.mk
include $(OOPS_APPS_ROOT)/common/scaffold.mk
