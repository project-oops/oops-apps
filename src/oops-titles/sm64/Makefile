# Super Mario 64, through oops-sdk.
#
#   make census        compile every source this title needs, for the target, and report
#   make census-one F=<path>          every error in one source
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

# src/pc/gfx/gfx_opengl.c is the backend this build selects, and it is fixed-function. The Direct3D
# backends beside it are for Windows and are excluded from the source list below.
OOPS_RENDERER = gl1

# First, because this file names files under upstream/ while it is being read.
include $(OOPS_APPS_ROOT)/common/upstream.mk

SM64_UP := $(CURDIR)/upstream
SM64_VERSION ?= us

# --- the source list -------------------------------------------------------
#
# Upstream's own `Makefile` builds `src/` and `lib/` and nothing else; `tools/` is host tooling that
# runs during extraction, so it is not payload code.
#
# The Direct3D and DXGI backends are alternative renderers selected by upstream's RENDER_API, so
# leaving them out of the list is upstream's own arrangement rather than a change to its tree.
SM64_EXCLUDE := %/gfx_direct3d11.c %/gfx_direct3d12.c %/gfx_direct3d_common.c %/gfx_dxgi.c
SM64_C_SRCS := $(filter-out $(SM64_EXCLUDE), \
    $(sort $(wildcard $(SM64_UP)/src/*.c) $(wildcard $(SM64_UP)/src/*/*.c) \
           $(wildcard $(SM64_UP)/src/*/*/*.c) \
           $(wildcard $(SM64_UP)/lib/*.c) $(wildcard $(SM64_UP)/lib/*/*.c)))

# --- the dependencies ------------------------------------------------------
include $(OOPS_APPS_ROOT)/common/posix.mk

# SDL2 is the only dependency: gfx_sdl2.c is the windowing and input backend, and the audio path is
# upstream's own software mixer (src/pc/mixer.c) over SDL's audio device.
OOPS_SDL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2)
include $(OOPS_SDL)/oops-sdl.mk

SM64_CFLAGS := -I$(SM64_UP) -I$(SM64_UP)/include -I$(SM64_UP)/src -I$(SM64_UP)/lib \
               -I$(SM64_UP)/build/$(SM64_VERSION)_pc \
               -DVERSION_US=1 -DNON_MATCHING=1 -DAVOID_UB=1 -DTARGET_PORT_CONSOLE=1 \
               -DNO_SEGMENTED_MEMORY=1 -DWIDESCREEN=0 \
               -DOOPS_POSIX_HOME=\"/data/sm64\" \
               $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE)

# app.mk compiles at -Werror with a set stricter than a decompiled 1996 codebase meets. Switched off
# rather than patched, a patch against somebody else's tree being a rebase burden for no behaviour
# change.
SM64_WARNINGS := -Wno-strict-prototypes -Wno-missing-prototypes -Wno-conversion \
                 -Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-comment \
                 -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-shadow \
                 -Wno-sign-compare -Wno-char-subscripts -Wno-unused-function \
                 -Wno-unused-const-variable -Wno-missing-field-initializers \
                 -Wno-unused-variable -Wno-multichar -Wno-absolute-value \
                 -Wno-incompatible-pointer-types -Wno-int-conversion

EXTRA_TARGET_CFLAGS += $(SM64_CFLAGS) $(SM64_WARNINGS)

# No payload, and so neither OOPS_RENDERER nor OOPS_FEATURES: app.mk appends the renderer's and each
# feature's sources to PAYLOAD_SRCS, which is enough on its own to link an eboot containing the SDK
# and no game. That packages and lists like a working title, which is worse than no eboot. The GL
# headers the census needs come from oops-sdk/include either way.
#
# The entry point in shim/ is kept because it is upstream-independent: it probes for the data
# directory and calls main, which any replacement upstream will also need.
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

# `make title` fails on undefined SDK symbols while there is no payload, which is the honest answer
# and not a useful default. The census is.
.DEFAULT_GOAL := census
