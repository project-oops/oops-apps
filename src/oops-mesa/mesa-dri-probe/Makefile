# mesa-dri-probe - the smallest title that brings GL up through the Gallium DRI frontend.
#
# It calls `oops_gl_create` (`driCreateNewScreen3`, a drawable, a context, `dri_make_current`),
# then `glGetString`, a draw and a present. `mesa-winsys-probe` calls `radeonsi_screen_create`
# directly; the frontend creates its own screen, so the two stay separate titles and the winsys
# probe remains the control.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# The renderer this title links (common/app.mk): the hosted Mesa stack (oops-mesa#D002).
OOPS_RENDERER = mesa

OOPS_FEATURES := display thread

PAYLOAD_SRCS = mesa_dri_probe_main.c

ENTRY_POINT := mesa_dri_probe_start

# SELFish's `native_eboot.ld` plus a PT_TLS segment, which Mesa needs to link. The frontend's
# GL dispatch goes through the thread-local `_mesa_glapi_tls_Context`.
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
