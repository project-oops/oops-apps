# mesa-cube - the example title for oops-mesa: what a title author reads before writing one.
#
# It sets `USE_MESA`, calls `oops_gl_create`, and draws and presents every frame until the host
# closes it: a texture and sampler, a depth test, an index buffer, a matrix pipeline. It is an
# example, not conformance. Presenting every frame gives `oops_gl_present`'s per-part cost
# report a profile rather than one sample (oops-mesa#D012).

OOPS_APPS_ROOT ?= $(abspath ../../..)

# The shared cube dashboard (common/cube_hud.c) is included, so `common/` is on the header path.
EXTRA_TARGET_CFLAGS += -I$(OOPS_APPS_ROOT)/common

# The renderer this title links (common/app.mk). `mesa` is the hosted OpenGL-through-Mesa stack
# (oops-mesa#D002); the same source builds on oops-gl with `gl1`/`gl2` instead.
OOPS_RENDERER = mesa

# SDK math for the matrices, input for the pad, and the HUD overlay. Under USE_MESA the libc
# headers come from the Mesa sysroot. The HUD draws fixed-function GL, which Mesa's
# compatibility context runs beside this title's 3.3 shaders.
OOPS_FEATURES := display thread input hud

PAYLOAD_SRCS = mesa_cube_main.c \
               $(OOPS_APPS_ROOT)/common/cube_hud.c

ENTRY_POINT := mesa_cube_start

# SELFish's `native_eboot.ld` plus a PT_TLS segment, which Mesa's thread-local GL dispatch
# needs, and `.init_array` bounds: a hosted title has no crt, so it runs Mesa's constructors
# itself.
TARGET_LD_SCRIPT := $(abspath local_tls.ld)

# The import manifest is generated from obSCEne's corpus; a Mesa title imports more than
# `common/symbols.txt` carries. Run `make imports` after a link, then `make title`: without
# `build/imports.txt` mkmodule builds no vendor segment and the loader refuses the module.
OOPS_MESA ?= $(abspath $(OOPS_APPS_ROOT)/../oops-mesa)
SYMBOLS_FILE := $(abspath build/imports.txt)

include $(OOPS_APPS_ROOT)/common/app.mk

$(SYMBOLS_FILE): $(BUILD)/$(APP_NAME).elf
	@if [ ! -f "$@" ]; then \
	  mkdir -p $(BUILD); \
	  OOPS_APPS_SYMBOLS=$(OOPS_APPS_ROOT)/common/symbols.txt \
	    bash $(OOPS_MESA)/tools/generate-imports.sh $< $@; \
	fi

.PHONY: imports
imports: $(BUILD)/$(APP_NAME).elf
	@mkdir -p $(BUILD)
	@OOPS_APPS_SYMBOLS=$(OOPS_APPS_ROOT)/common/symbols.txt \
	  bash $(OOPS_MESA)/tools/generate-imports.sh $< $(SYMBOLS_FILE)
