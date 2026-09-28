# Starship, through oops-sdk.
#
#   make           the survey - what stands between this title and a link
#   make title     the payload, packaged - once STRF_ARMED=1 links
#
# Harbour Masters' Star Fox 64 on libultraship. The runtime, its shader dialect and its
# dependency stack are the ones `../spaghetti-kart` already carries, and the ROM converter is
# Torch (`tools/Torch`, driven from `src/port/extractor`), the one Spaghetti Kart links.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# libultraship makes the GL calls, through its own Fast3D and prism-generated shaders.
OOPS_RENDERER = gl2

include $(OOPS_APPS_ROOT)/common/upstream.mk

# libultraship's stack, as `../spaghetti-kart` names it, with `cimgui` for the port's C code and
# SDL2_net, which upstream finds unconditionally (CMakeLists.txt:383).
SCAFFOLD_ARM    := STRF_ARMED
SCAFFOLD_DEPS   := sdl2 sdl2-net zlib libzip stb nlohmann-json thread-pool tinyxml2 spdlog imgui \
                   cimgui prism-processor libgfxd tomlplusplus semver libogg libvorbis dr-libs
SCAFFOLD_SUPPLY := a Star Fox 64 ROM (US 1.0 or 1.1, .z64) as /data/homebrew/STRF00001/sf64.z64, \
                   converted to sf64.o2r on the console at first start

OOPS_FEATURES := display input keyboard mouse audio suspend thread draw dialog

# Arming is where upstream's source lists, libultraship's and Torch's are transcribed, as
# `../spaghetti-kart/Makefile` does, with `shim/strf_start.cpp`, `common/player_data.c` and
# `common/crashtrace.c` beside them. Until then an armed build says so rather than linking a
# payload of three files.
ifeq ($(STRF_ARMED),1)
$(error starship: the source lists are not transcribed yet - `make survey` says what is next)
endif

include $(OOPS_APPS_ROOT)/common/app.mk
include $(OOPS_APPS_ROOT)/common/scaffold.mk
