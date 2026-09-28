# Balatro, through oops-sdk.
#
#   make           the survey - what stands between this title and a link
#   make title     the payload, packaged - once BLTR_ARMED=1 links
#
# LÖVE 11.5 (`upstream/`), running the game from the player's own `Balatro.exe`. Nothing of
# Balatro is fetched or built: the engine is the port, and every other LÖVE game runs on the same
# payload given a different archive.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# LÖVE 11 needs OpenGL 2.1 with GLSL. It loads its entry points through its bundled `glad`, by
# name, so a clean link says nothing about which of them answer - `../q3rally`'s
# `scripts/gl-surface.sh` is the check for that.
OOPS_RENDERER = gl2

include $(OOPS_APPS_ROOT)/common/upstream.mk

# PhysFS, Box2D, lua-enet, luasocket, glslang and the image decoders are bundled under
# `src/libraries/` and build from upstream. LuaJIT is built with its JIT disabled
# (`LUAJIT_DISABLE_JIT`): the interpreter needs no executable memory, and the games are written
# against LuaJIT's `bit` library, which PUC Lua 5.1 does not have.
SCAFFOLD_ARM    := BLTR_ARMED
SCAFFOLD_DEPS   := sdl2 libcxx openal-soft freetype libogg libvorbis libtheora libmodplug zlib \
                   luajit
SCAFFOLD_SUPPLY := Balatro.exe from the player's own copy of Balatro, as \
                   /data/homebrew/BLTR00001/Balatro.exe

OOPS_FEATURES := display input keyboard mouse audio suspend thread draw dialog

# Arming is where LÖVE's source list is transcribed from its CMakeLists.txt, with
# `shim/bltr_start.cpp`, `common/player_data.c` and `common/crashtrace.c` beside it. Until then
# an armed build says so rather than linking a payload of three files.
ifeq ($(BLTR_ARMED),1)
$(error balatro: the source lists are not transcribed yet - `make survey` says what is next)
endif

include $(OOPS_APPS_ROOT)/common/app.mk
include $(OOPS_APPS_ROOT)/common/scaffold.mk
