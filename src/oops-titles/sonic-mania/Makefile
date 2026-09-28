# Sonic Mania, through oops-sdk.
#
#   make           the survey - what stands between this title and a link
#   make title     the payload, packaged - once SMNA_ARMED=1 links
#
# The decompiled game logic (`upstream/SonicMania`) linked into the RSDKv5 engine
# (`upstream/dependencies/RSDKv5`), which upstream calls `GAME_STATIC`. The player's `Data.rsdk`
# is the data, read at run time.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# RSDKv5 draws its frame in software. Of its render devices this takes `SDL2`
# (`RSDK/Graphics/SDL2/`), which presents through an `SDL_Renderer`; oops-deps/sdl2 builds SDL's
# OpenGL renderer, which runs on fixed-function GL, so the GL is SDL's rather than the
# engine's. Audio is SDL's too (`RETRO_AUDIODEVICE_SDL2`), not the MiniAudio default.
OOPS_RENDERER = gl1

include $(OOPS_APPS_ROOT)/common/upstream.mk

# `tinyxml2` and `stb_vorbis` are submodules of upstream; the first is vendored here already and
# taken from there. Theora is for the video cutscenes.
SCAFFOLD_ARM    := SMNA_ARMED
SCAFFOLD_DEPS   := sdl2 libcxx libogg libtheora tinyxml2 zlib
SCAFFOLD_SUPPLY := Data.rsdk from the player's own copy of Sonic Mania, as \
                   /data/homebrew/SMNA00001/Data.rsdk

OOPS_FEATURES := display input keyboard mouse audio suspend thread draw dialog

# Arming is where RSDKv5's and the game's source lists are transcribed from their CMake files,
# with `RETRO_SUBSYSTEM=SDL2`, `GAME_STATIC` and `shim/smna_start.cpp`, `common/player_data.c`
# and `common/crashtrace.c` beside them. Until then an armed build says so rather than linking a
# payload of three files.
ifeq ($(SMNA_ARMED),1)
$(error sonic-mania: the source lists are not transcribed yet - `make survey` says what is next)
endif

include $(OOPS_APPS_ROOT)/common/app.mk
include $(OOPS_APPS_ROOT)/common/scaffold.mk
