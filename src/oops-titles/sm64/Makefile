# Super Mario 64, through oops-sdk.
#
#   make census        compile every source this title needs, for the target, and report
#   make census-one F=<path>          every error in one source
#   make blank-assets  write blank stand-ins for the ROM-derived assets, so a build needs no ROM
#
# There is no payload target, and this pin cannot have one. The ROM is a runtime input for every title
# in this collection: a build carries no game assets, and a title with no ROM says so on screen. This
# upstream decides the other way - extract_assets.py reads baserom.<version>.z64 and writes textures,
# samples, sequences and level data into the tree, which the build compiles into sound_data.o,
# leveldata.o and the skybox objects - so a payload built from it embeds the assets and cannot be
# published. A port of this game needs an upstream that reads the ROM at run time, as the
# libultraship titles do.
#
# The source list and census stay, because the platform layer is worth measuring against oops-sdk
# whichever upstream ends up supplying the game.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Before the app.mk include, because this file names files under OOPS_SDK_DIR in an
# immediately-expanded list below. The `?=` matches app.mk's own default.
OOPS_SDK ?= $(abspath $(OOPS_APPS_ROOT)/../oops-sdk)
OOPS_SDK_DIR := $(abspath $(OOPS_SDK))

# src/pc/gfx/gfx_opengl.c is the backend this build selects, and it is shader-based: it compiles a
# program per material combiner, which is oops-gl's programmable path rather than its fixed-function
# one. shim/include/SDL2/SDL_opengles2.h answers upstream's ES2 include with oops-gl's own header,
# the same arrangement supertux uses.
OOPS_RENDERER = gl2

# First, because this file names files under upstream/ while it is being read.
include $(OOPS_APPS_ROOT)/common/upstream.mk

SM64_UP := $(CURDIR)/upstream
SM64_VERSION ?= us

# --- the source list -------------------------------------------------------
#
# Upstream's SRC_DIRS, transcribed from its Makefile line 297, with the non-N64 arm: the glob is one
# level deep per named directory, not recursive. That distinction is the whole list. A recursive glob
# picks up src/game/behaviors and the other trees whose files are #included into a translation unit
# rather than compiled as one, and lib/src, which is the N64 libultra and conflicts with the port's
# own ultra_reimplementation.c.
#
# levels/ contributes three named files per level rather than its whole tree, and src/game/main.c is
# the N64 entry point, replaced on this target by src/pc/pc_main.c.
SM64_SRC_DIRS := src src/engine src/game src/audio src/menu src/buffers \
                 src/pc src/pc/gfx src/pc/audio src/pc/controller \
                 src/goddard src/goddard/dynlists \
                 actors levels bin bin/$(SM64_VERSION) data assets sound

# The nine libultra sources upstream keeps for a non-N64 target, its ULTRA_C_FILES at line 326: the
# matrix and projection helpers and the audio bank reader. The rest of lib/src is the N64 library and
# conflicts with src/pc/ultra_reimplementation.c.
SM64_ULTRA_SRCS := $(addprefix $(SM64_UP)/lib/src/, \
    alBnkfNew.c guLookAtRef.c guMtxF2L.c guNormalize.c guOrthoF.c \
    guPerspectiveF.c guRotateF.c guScaleF.c guTranslateF.c)

# Alternative backends upstream selects between, left out rather than patched away: Direct3D and DXGI
# are Windows, GLX wants X11, and PulseAudio, ALSA and the Wii U adapter want Linux headers this
# platform has no equivalent of. SDL2 supplies the window, the GL context and the audio device here.
SM64_EXCLUDE := %/gfx_direct3d11.c %/gfx_direct3d12.c %/gfx_direct3d_common.c \
                %/gfx_dxgi.c %/gfx_glx.c \
                %/audio_pulse.c %/audio_alsa.c %/wup.c \
                %/main.c
# The skybox sources are generated as whole translation units rather than byte lists, so they are
# compiled rather than included. `make blank-assets` writes them, which is why the wildcard is
# deferred (`=`) until a rule runs.
SM64_SKYBOX_SRCS = $(wildcard $(SM64_BUILD_DIR)/bin/*_skybox.c)

# Upstream's GENERATED_C_FILES, also written by `make blank-assets`: the animation table from the 193
# .inc.c files in assets/anims, and the demo input table from assets/demo_data.json.
SM64_GEN_SRCS = $(wildcard $(SM64_BUILD_DIR)/assets/mario_anim_data.c) \
                $(wildcard $(SM64_BUILD_DIR)/assets/demo_data.c)

SM64_C_SRCS = $(filter-out $(SM64_EXCLUDE), \
    $(sort $(foreach d,$(SM64_SRC_DIRS),$(wildcard $(SM64_UP)/$(d)/*.c)) \
           $(wildcard $(SM64_UP)/levels/*/leveldata.c) \
           $(wildcard $(SM64_UP)/levels/*/script.c) \
           $(wildcard $(SM64_UP)/levels/*/geo.c) \
           $(SM64_ULTRA_SRCS) $(SM64_SKYBOX_SRCS) $(SM64_GEN_SRCS)))

# --- the dependencies ------------------------------------------------------
include $(OOPS_APPS_ROOT)/common/posix.mk

# SDL2 is the only dependency: gfx_sdl2.c is the windowing and input backend, and the audio path is
# upstream's own software mixer (src/pc/mixer.c) over SDL's audio device.
OOPS_SDL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2)
include $(OOPS_SDL)/oops-sdl.mk

# Upstream's own include set and defines, from its Makefile: INCLUDE_DIRS at line 386, the version
# and microcode DEFINES, PLATFORM_CFLAGS and GFX_CFLAGS. `_LANGUAGE_C` is the one that matters most:
# it gates the N64 type definitions in include/ultra64.h, and without it every source using s32 or
# OSMesgQueue fails. TARGET_LINUX picks the POSIX arm of upstream's platform chain; nothing in the
# sources reads it as "Linux" beyond that.
#
# The build directory is on the path twice because upstream puts generated headers under both it and
# its include/ subdirectory.
# shim/include carries SDL2/SDL.h and SDL2/SDL_opengles2.h, which forward to the include root:
# upstream writes <SDL2/...> and oops-deps/sdl2 puts its headers directly on the path.
SM64_BUILD_DIR := $(SM64_UP)/build/$(SM64_VERSION)_pc
SM64_CFLAGS := -Ishim/include \
               -I$(SM64_UP) -I$(SM64_UP)/include -I$(SM64_UP)/src -I$(SM64_UP)/lib \
               -I$(SM64_BUILD_DIR) -I$(SM64_BUILD_DIR)/include \
               -D_LANGUAGE_C -DVERSION_US=1 -DNON_MATCHING=1 -DAVOID_UB=1 \
               -DF3DEX_GBI_2=1 -DF3DEX_GBI_SHARED=1 \
               -DTARGET_LINUX -DNO_SEGMENTED_MEMORY -DUSE_SYSTEM_MALLOC \
               -DENABLE_OPENGL -DWIDESCREEN=0 \
               -DOOPS_POSIX_HOME=\"/data/sm64\" \
               $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE)

# app.mk compiles at -Werror with a set stricter than a decompiled 1996 codebase meets. Switched off
# rather than patched, a patch against somebody else's tree being a rebase burden for no behaviour
# change.
# Upstream compiles with these two and the decompiled code depends on both: it aliases through
# unions and relies on signed overflow wrapping.
SM64_CFLAGS += -fno-strict-aliasing -fwrapv

SM64_WARNINGS := -Wno-strict-prototypes -Wno-missing-prototypes -Wno-conversion \
                 -Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-comment \
                 -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-shadow \
                 -Wno-sign-compare -Wno-char-subscripts -Wno-unused-function \
                 -Wno-unused-const-variable -Wno-missing-field-initializers \
                 -Wno-unused-variable -Wno-multichar -Wno-absolute-value \
                 -Wno-return-type -Wno-unknown-pragmas \
                 -Wno-incompatible-pointer-types -Wno-int-conversion

EXTRA_TARGET_CFLAGS += $(SM64_CFLAGS) $(SM64_WARNINGS)

# --- the link --------------------------------------------------------------
#
# The payload carries no game assets: every one of them is a zero array `make blank-assets` sized
# from assets.json, filled at run time from a ROM the player supplies. So this builds and publishes
# without a ROM, which is the rule for every title here.
#
# `make blank-assets` has to have run, because the source list includes the generated skybox
# translation units and most sources include a generated byte list.
PAYLOAD_SRCS = shim/sm64_start.c $(SM64_C_SRCS) $(OOPS_POSIX_SRCS)

# No net: the game has no networking.
OOPS_FEATURES := display input keyboard mouse audio suspend thread draw

EXTRA_TARGET_LDFLAGS += $(OOPS_SDL_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(OOPS_SDL_LIB)

include $(OOPS_APPS_ROOT)/common/app.mk

# ---------------------------------------------------------------------------
# `make census`: which of this title's sources compile for the target
#
# One file at a time, -fsyntax-only, each blocker printed with the source that hit it.
#
# Without the ROM this measures the platform layer and little else, the generated asset headers being
# what most of these sources wait for. A clean census is not a working payload in any case:
# -fsyntax-only does not link.
# ---------------------------------------------------------------------------
.PHONY: census
census:
	@ok=0; bad=0; mkdir -p $(BUILD); : > $(BUILD)/census-errors.txt; \
	 for f in $(SM64_C_SRCS); do \
	    if $(TARGET_CC) $(TARGET_CFLAGS) $(EXTRA_TARGET_CFLAGS) -fsyntax-only "$$f" \
	         2>$(BUILD)/census-e.txt; then ok=$$((ok+1)); \
	    else bad=$$((bad+1)); \
	        printf '%s: %s\n' "$${f#$(SM64_UP)/}" \
	          "$$(grep -m1 'error:' $(BUILD)/census-e.txt | sed 's|.*error: ||' | cut -c1-60)" \
	          >> $(BUILD)/census-errors.txt; \
	    fi; \
	 done; \
	 printf 'sm64 C census: %s compile, %s do not (of %s)\n' "$$ok" "$$bad" "$(words $(SM64_C_SRCS))"; \
	 if [ "$$bad" -gt 0 ]; then \
	    printf '\n-- blockers, most common first --\n'; \
	    sed 's|.*: ||' $(BUILD)/census-errors.txt | sort | uniq -c | sort -rn | head -20; fi

# ---------------------------------------------------------------------------
# `make census-one F=<path>`: every error in one source, not just the first
#
# `F` is relative to the upstream tree.
# ---------------------------------------------------------------------------
.PHONY: census-one
census-one:
	@$(TARGET_CC) $(TARGET_CFLAGS) $(EXTRA_TARGET_CFLAGS) -fsyntax-only $(SM64_UP)/$(F) 2>&1 \
	 | grep 'error:' | sed 's|.*error: ||' | sort | uniq -c | sort -rn


# ---------------------------------------------------------------------------
# `make blank-assets`: the ROM-free half of the asset problem
#
# upstream/assets.json carries every asset's size, its position in the ROM and, for a texture, its
# pixel dimensions, so a correctly shaped empty file can be written for each one without a ROM.
# Upstream's own rules then produce .inc.c arrays of exactly the sizes a ROM build produces, which
# keeps every address and offset identical; only the contents are zero.
#
# Textures, sequences and raw binaries are covered. The sound banks are not: upstream rebuilds those
# host-native from carved AIFC samples rather than slicing them out of the ROM, so a zero file of the
# right length does not stand in for one.
# ---------------------------------------------------------------------------
# textconv encodes the in-game text headers from files in the repository, so it needs no ROM. CC is
# on the command line because upstream's tools/Makefile sets `CC := gcc`, which this toolchain does
# not have and an environment variable does not override.
# The subset of the compile defines the generation passes need, without the include paths: these
# commands run from inside the upstream tree, where this title's relative -I flags do not resolve.
SM64_GEN_DEFINES := -D_LANGUAGE_C -DVERSION_US=1 -DNON_MATCHING=1 -DAVOID_UB=1 \
                    -DF3DEX_GBI_2=1 -DF3DEX_GBI_SHARED=1

.PHONY: blank-assets
blank-assets:
	@python3 scripts/gen-blank-assets.py $(SM64_UP) $(SM64_BUILD_DIR)
	@mkdir -p $(SM64_BUILD_DIR)/include
	@$(MAKE) -s -C $(SM64_UP)/tools CC=clang textconv
	@cd $(SM64_UP) && ./tools/textconv charmap.txt include/text_strings.h.in \
	    $(SM64_BUILD_DIR)/include/text_strings.h
	@cd $(SM64_UP) && ./tools/textconv charmap_menu.txt include/text_menu_strings.h.in \
	    $(SM64_BUILD_DIR)/include/text_menu_strings.h
	@# The dialogue and course tables, and the level header list: preprocessor passes over files in
	@# the repository, so no ROM either. The sed turns each emitted path into an #include.
	@mkdir -p $(SM64_BUILD_DIR)/text/$(SM64_VERSION)
	@cd $(SM64_UP) && clang -E -P -I . -I text/$(SM64_VERSION) text/define_text.inc.c \
	    | ./tools/textconv charmap.txt - $(SM64_BUILD_DIR)/text/$(SM64_VERSION)/define_text.inc.c
	@cd $(SM64_UP) && clang -E -P -I . -I text/$(SM64_VERSION) text/define_courses.inc.c \
	    | ./tools/textconv charmap.txt - $(SM64_BUILD_DIR)/text/$(SM64_VERSION)/define_courses.inc.c
	@# `-x c` because the input is a `.h.in`, and the version defines because levels/level_defines.h
	@# expands to nothing without them. Upstream passes both through CPPFLAGS.
	@cd $(SM64_UP) && clang -E -P -x c -Wno-trigraphs $(SM64_GEN_DEFINES) -I . \
	    levels/level_headers.h.in | sed -E 's|(.+)|#include "\1"|' \
	    > $(SM64_BUILD_DIR)/include/level_headers.h
	@test -s $(SM64_BUILD_DIR)/include/level_headers.h || { \
	    echo "sm64: level_headers.h came out empty - the preprocessor found no levels" >&2; \
	    exit 1; }
	@# The animation and demo tables. assets/anims holds 193 .inc.c files in the repository, and the
	@# demo inputs read the blank .bin files written above, so neither needs a ROM.
	@mkdir -p $(SM64_BUILD_DIR)/assets
	@cd $(SM64_UP) && python3 tools/mario_anims_converter.py \
	    > $(SM64_BUILD_DIR)/assets/mario_anim_data.c
	@cd $(SM64_UP) && python3 tools/demo_data_converter.py assets/demo_data.json \
	    $(SM64_GEN_DEFINES) > $(SM64_BUILD_DIR)/assets/demo_data.c
	@echo "sm64: animation and demo tables generated"
	@echo "sm64: text headers and level headers encoded"
