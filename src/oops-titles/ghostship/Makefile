# Ghostship, through oops-sdk.
#
#   make           the survey - what stands between this title and a link
#   make title     the payload, packaged - once GHST_ARMED=1 links
#
# Harbour Masters' Super Mario 64 on libultraship. The runtime, its shader dialect and its
# dependency stack are the ones `../spaghetti-kart` and `../ship-of-harkinian` already carry,
# and the ROM converter is Torch, the one Spaghetti Kart links - so this title follows those two
# rather than paralleling them.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# libultraship makes the GL calls, through its own Fast3D and prism-generated shaders.
OOPS_RENDERER = gl2

include $(OOPS_APPS_ROOT)/common/upstream.mk

# libultraship's stack, as `../spaghetti-kart` names it, and `cimgui`, which this port's C code
# reaches ImGui through. Networking (`USE_NETWORKING`, CMakeLists.txt:193) stays off, so neither
# IXWebSocket nor mbedTLS is needed.
SCAFFOLD_ARM    := GHST_ARMED
SCAFFOLD_DEPS   := sdl2 zlib libzip stb nlohmann-json thread-pool tinyxml2 spdlog imgui cimgui \
                   prism-processor libgfxd tomlplusplus semver libogg libvorbis dr-libs
SCAFFOLD_SUPPLY := a Super Mario 64 ROM (US or JP, .z64) as /data/homebrew/GHST00001/sm64.z64, \
                   converted to sm64.o2r on the console at first start

OOPS_FEATURES := display input keyboard mouse audio suspend thread draw dialog

# Arming is where upstream's source lists, libultraship's and Torch's are transcribed, as
# `../spaghetti-kart/Makefile` does, with `shim/ghst_start.cpp`, `common/player_data.c` and
# `common/crashtrace.c` beside them. Until then an armed build says so rather than linking a
# payload of three files.
ifeq ($(GHST_ARMED),1)
$(error ghostship: the source lists are not transcribed yet - `make survey` says what is next)
endif

include $(OOPS_APPS_ROOT)/common/app.mk
include $(OOPS_APPS_ROOT)/common/scaffold.mk
