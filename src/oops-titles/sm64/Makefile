# Super Mario 64, through oops-sdk.
#
#   make               the payload
#   make title         the payload, packaged
#   make census        compile every source this title needs, for the target, and report
#   make census-one F=<path>          every error in one source
#   make assets        run upstream's asset extraction (needs your own ROM - see below)
#
# This title needs a ROM on the build machine, which is upstream's design: extract_assets.py reads
# baserom.<version>.z64 and writes the textures, samples, sequences and level data into the tree, and
# the build compiles them in, so the payload carries the game's assets.
#
# Put baserom.us.z64 in upstream/ and run `make assets` once. `make census` works without it.

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
SM64_ROM := $(SM64_UP)/baserom.$(SM64_VERSION).z64

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

# --- the link --------------------------------------------------------------
#
# PAYLOAD_SRCS is behind SM64_ARMED because every level, texture and sound is a generated file:
# without the assets the compile produces hundreds of missing-file errors, where the warning below
# names the one thing that is wrong. Set SM64_ARMED=1 once `make assets` has run.
ifeq ($(SM64_ARMED),1)
PAYLOAD_SRCS := shim/sm64_start.c $(SM64_C_SRCS) $(OOPS_POSIX_SRCS)
EXTRA_TARGET_LDFLAGS += $(OOPS_SDL_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(OOPS_SDL_LIB)
endif

# No net: the game has no networking.
OOPS_FEATURES := display input keyboard mouse audio suspend thread draw

ifeq ($(wildcard $(SM64_ROM)),)
$(warning sm64: no $(notdir $(SM64_ROM)) in upstream/ - `make` cannot produce an eboot.)
$(warning sm64: upstream generates every asset from the cartridge image. Put your own copy at)
$(warning sm64:   $(SM64_ROM))
$(warning sm64: then `make assets`, then `make SM64_ARMED=1 title`. `make census` works now.)
endif

include $(OOPS_APPS_ROOT)/common/app.mk

# ---------------------------------------------------------------------------
# `make assets`: upstream's extraction, which needs the ROM
#
# Run on the host, not in the build container: it is a host Python script and the container carries
# no ROM. It writes into `upstream/`, which the fetch stamp does not cover, so it survives a rebuild
# but not an `upstream-clean`.
# ---------------------------------------------------------------------------
.PHONY: assets
assets:
	@test -f $(SM64_ROM) || { \
	    echo "sm64: $(SM64_ROM) is missing." >&2; \
	    echo "  This title's assets are generated from the cartridge image, so the build" >&2; \
	    echo "  machine needs a copy of the game you own. Nothing here can supply it." >&2; \
	    exit 1; }
	@cd $(SM64_UP) && python3 extract_assets.py $(SM64_VERSION)
	@echo "sm64: assets extracted - now 'make SM64_ARMED=1 title'"

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
