# mesa-winsys-probe - the smallest title that links upstream Mesa and says how far it gets.
#
# It links oops-mesa's archives, its winsys and runtime shims, the C++ support archive and
# oops-sdk into one binary the way a real title does. It does not render; it reports where
# the stack stops.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# The renderer this title links (common/app.mk): the hosted Mesa stack (oops-mesa#D002).
OOPS_RENDERER = mesa

OOPS_FEATURES := display thread

PAYLOAD_SRCS = mesa_winsys_probe_main.c

ENTRY_POINT := mesa_winsys_probe_start

# SELFish's `native_eboot.ld` plus a PT_TLS segment: Mesa uses thread-local storage, and
# without the segment the link fails on every `__thread` symbol.
TARGET_LD_SCRIPT := $(abspath local_tls.ld)

# The import manifest names the library each imported symbol resolves from. A Mesa title
# imports more than `common/symbols.txt` carries, so `oops-mesa/tools/generate-imports.sh`
# reads the imports from the binary and places each from obSCEne's corpus. Run `make imports`
# after a link, then `make title`: without `build/imports.txt` mkmodule builds no vendor
# segment and the loader refuses the module.
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
