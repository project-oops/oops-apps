# Sonic Mania, through oops-sdk.
#
#   make title     the payload, packaged
#   make survey    the pin, the dependencies and what the player supplies
#   make census    compile every component's sources for the target, set by set
#   make census-one S=<set> F=<path>   every error in one source
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

SMNA_ARMED ?= 1
SCAFFOLD_ARM    := SMNA_ARMED
# RSDKv5 bundles tinyxml2, iniparser, miniz and stb_vorbis under `dependencies/all/` and builds
# them itself, as it does here. Theora is for the video cutscenes.
SCAFFOLD_DEPS   := sdl2 libcxx libogg libtheora
SCAFFOLD_SUPPLY := Data.rsdk from the player's own copy of Sonic Mania, as \
                   /data/homebrew/SMNA00001/Data.rsdk

RSDK := $(CURDIR)/upstream/dependencies/RSDKv5
MANIA := $(CURDIR)/upstream/SonicMania

OOPS_SDL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2)
include $(OOPS_SDL)/oops-sdl.mk
OOPS_THEORA ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libtheora)
include $(OOPS_THEORA)/oops-libtheora.mk
include $(OOPS_APPS_ROOT)/common/posix.mk

OOPS_CXX_EXCEPTIONS = 1
OOPS_CXX_STD        = c++17
OOPS_LIBCXX ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libcxx)
include $(OOPS_LIBCXX)/oops-libcxx.mk
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk

# --- the components, transcribed from their CMake files ----------------------------
#
# `RETRO_FILES` (RSDKv5/CMakeLists.txt:14) - the engine, whose render, input and audio backends
# its own sources include by the device defines below - and the game (`GAME_SOURCES`, the
# `Objects/All.c` unity build).
SMNA_RSDK_SRCS := $(addprefix $(RSDK)/RSDKv5/, main.cpp \
    RSDK/Core/RetroEngine.cpp RSDK/Core/Math.cpp RSDK/Core/Reader.cpp RSDK/Core/Link.cpp \
    RSDK/Core/ModAPI.cpp RSDK/Dev/Debug.cpp RSDK/Storage/Storage.cpp RSDK/Storage/Text.cpp \
    RSDK/Graphics/Drawing.cpp RSDK/Graphics/Scene3D.cpp RSDK/Graphics/Animation.cpp \
    RSDK/Graphics/Sprite.cpp RSDK/Graphics/Palette.cpp RSDK/Graphics/Video.cpp \
    RSDK/Audio/Audio.cpp RSDK/Input/Input.cpp RSDK/Scene/Scene.cpp RSDK/Scene/Collision.cpp \
    RSDK/Scene/Object.cpp RSDK/Scene/Objects/DefaultObject.cpp RSDK/Scene/Objects/DevOutput.cpp \
    RSDK/User/Core/UserAchievements.cpp RSDK/User/Core/UserCore.cpp \
    RSDK/User/Core/UserLeaderboards.cpp RSDK/User/Core/UserPresence.cpp \
    RSDK/User/Core/UserStats.cpp RSDK/User/Core/UserStorage.cpp) \
    $(addprefix $(RSDK)/dependencies/all/, tinyxml2/tinyxml2.cpp iniparser/iniparser.cpp \
                                           iniparser/dictionary.cpp)
SMNA_RSDK_C_SRCS := $(RSDK)/dependencies/all/miniz/miniz.c
SMNA_GAME_SRCS := $(MANIA)/Game.c $(MANIA)/Objects/All.c

# --- flags ---------------------------------------------------------------------------
#
# The engine's defines are RSDKv5/CMakeLists.txt:129 for an SDL2, game-static build. The mod
# loader is off: `ModAPI.cpp` walks mod folders with `std::filesystem`, which the pinned libc++
# does not build, and the game runs without mods. The game's are CMakeLists.txt:82, for the last
# Steam release (`GAME_VERSION=6`, Plus).
SMNA_BASE := -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib -nostdlibinc \
             -fPIC -O2 -w -DRETRO_REVISION=3 -DRETRO_USE_MOD_LOADER=0 -DRETRO_MOD_LOADER_VER=2
SMNA_SYS   = $(OOPS_POSIX_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)
# `MINIZ_NO_TIME` keeps the bundled miniz from setting file times through `utime`, which it
# does only when extracting to disk.
SMNA_RSDK_DEFS := -DRSDK_USE_SDL2=1 -DRETRO_AUDIODEVICE_SDL2=1 -DRETRO_STANDALONE=0 -DMINIZ_NO_TIME=1 \
                  -DRSDK_AUTOBUILD=0 -DRETRO_DISABLE_LOG=0 '-DRETRO_DEV_EXTRA="SDL2 - 1.1.1"'
SMNA_RSDK_INC  = -I$(RSDK)/RSDKv5 -I$(RSDK)/dependencies/all -I$(RSDK)/dependencies/all/tinyxml2 \
                 -I$(RSDK)/dependencies/all/iniparser -I$(MANIA) $(OOPS_THEORA_INCLUDE) \
                 $(OOPS_SDL_INCLUDE) $(OOPS_SDL_PREFIX_INCLUDE)

CENSUS_SETS := rsdk rsdk_c game
CENSUS_rsdk_SRCS := $(SMNA_RSDK_SRCS)
# The system headers before the engine's and SDL's paths: `sdl2/include` carries a `unistd.h`
# of its own, without `ssize_t`, which `iniparser/dictionary.cpp` needs from the real one.
CENSUS_rsdk_FLAGS = -x c++ $(SMNA_BASE) -std=c++17 $(SMNA_RSDK_DEFS) $(OOPS_LIBCXX_INCLUDE) \
                    $(SMNA_SYS) $(SMNA_RSDK_INC)
CENSUS_rsdk_c_SRCS := $(SMNA_RSDK_C_SRCS)
CENSUS_rsdk_c_FLAGS = $(SMNA_BASE) -std=gnu11 $(SMNA_RSDK_DEFS) $(SMNA_SYS) $(SMNA_RSDK_INC)
CENSUS_game_SRCS := $(SMNA_GAME_SRCS)
CENSUS_game_FLAGS = $(SMNA_BASE) -std=gnu11 -DGAME_INCLUDE_EDITOR=1 -DMANIA_PREPLUS=0 \
                    -DMANIA_FIRST_RELEASE=0 -DGAME_VERSION=6 -I$(MANIA) -I$(MANIA)/Objects \
                    $(SMNA_SYS)

OOPS_FEATURES := display input keyboard mouse audio suspend thread draw dialog

include $(OOPS_APPS_ROOT)/common/deps.mk

ifeq ($(SMNA_ARMED),1)
# --- the payload ---------------------------------------------------------------------
#
# Engine and game in one archive, linked whole: `main` is in there, and the game's entry
# (`LinkGameLogicDLL`) is reached only through a pointer.
SMNA_BUILD := build/smna

define smna_objs
SMNA_$(1)_OBJS := $$(call oops_objs,$$(SMNA_BUILD)/$(1),$$(CENSUS_$(1)_SRCS))
$$(call oops_obj_rules,$$(SMNA_BUILD)/$(1),TARGET_CC,CENSUS_$(1)_FLAGS,$$(CENSUS_$(1)_SRCS))
-include $$(SMNA_$(1)_OBJS:.o=.d)
endef
$(foreach s,$(CENSUS_SETS),$(eval $(call smna_objs,$(s))))

SMNA_LIB := $(SMNA_BUILD)/libmania.a
SMNA_OBJS := $(SMNA_rsdk_OBJS) $(SMNA_rsdk_c_OBJS) $(SMNA_game_OBJS)
$(SMNA_LIB): $(SMNA_OBJS) | $(OOPS_SDL_PREFIX_STAMP)
	@rm -f $@
	@a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $(SMNA_OBJS)
	@echo "sonic-mania: $@ ($(words $(SMNA_OBJS)) objects)"
$(SMNA_OBJS): | $(OOPS_SDL_PREFIX_STAMP)

# The entry shim is C++ (upstream's `main` is), and brings the C++ runtime with it.
OOPS_CXX_SRCS := $(CURDIR)/shim/smna_start.cpp
include $(OOPS_APPS_ROOT)/common/cxx.mk
OOPS_CXX_INCLUDE += $(OOPS_LIBCXX_INCLUDE)
OOPS_CXX_FLAGS += -I$(OOPS_APPS_ROOT)/common $(OOPS_POSIX_INCLUDE) -DOOPS_POSIX_HOME=\"/app0\"

# The POSIX shim's headers before SDL's: `sdl2/include` carries a `unistd.h` of its own.
EXTRA_TARGET_CFLAGS += -DOOPS_CXX_EXTERNAL_NEW_DELETE $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE) \
                       -I$(OOPS_APPS_ROOT)/common -DOOPS_POSIX_HOME=\"/app0\"
PAYLOAD_SRCS := $(OOPS_APPS_ROOT)/common/player_data.c $(OOPS_APPS_ROOT)/common/crashtrace.c \
                $(OOPS_POSIX_SRCS)
EXTRA_TARGET_LDFLAGS += -Wl,--whole-archive $(SMNA_LIB) -Wl,--no-whole-archive \
                        $(OOPS_CXX_LDFLAGS) $(OOPS_THEORA_LDFLAGS) $(OOPS_SDL_LDFLAGS) \
                        $(OOPS_LIBCXX_LDFLAGS) $(OOPS_LIBCXXABI_LDFLAGS) $(OOPS_LIBUNWIND_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(SMNA_LIB) $(OOPS_CXX_LIB) $(OOPS_THEORA_LIB) $(OOPS_OGG_LIB) \
                        $(OOPS_SDL_LIB) $(OOPS_LIBCXX_LIB) $(OOPS_LIBCXXABI_LIB) \
                        $(OOPS_LIBUNWIND_LIB)
endif

include $(OOPS_APPS_ROOT)/common/app.mk
include $(OOPS_APPS_ROOT)/common/scaffold.mk
include $(OOPS_APPS_ROOT)/common/census.mk
