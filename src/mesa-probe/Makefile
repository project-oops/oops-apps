# mesa-probe - the smallest title that links upstream Mesa and says how far it gets.
#
# It is not a demo. Its job is to be the thing that proves the link: oops-mesa's archives, its
# winsys and runtime shims, the C++ support archive and oops-sdk all in one binary, built the way
# a real title would build. Everything else in oops-mesa is verified by compiling; this is
# verified by linking.
#
# It does not render. What it reports is exactly where the stack stops. That used to be known in
# advance - GB_ADDR_CONFIG, which the winsys refused (oops-mesa worklog 010) - and since
# 2026-09-16 it is not: the winsys answers that register from a derived value, so every step
# worklog 010 enumerated is answered and the next stopping point is unpredicted.

OOPS_APPS_ROOT ?= $(abspath ../..)

# The switch that brings Mesa in. A title that sets it is hosted (oops-mesa D002).
USE_MESA = 1

PAYLOAD_SRCS = mesa_probe_main.c \
               $(OOPS_SDK_DIR)/src/display.c \
               $(OOPS_SDK_DIR)/src/agc/agc_display.c \
               $(OOPS_SDK_DIR)/src/agc/agc_tiler.c \
               $(OOPS_SDK_DIR)/src/memory/memory.c \
               $(OOPS_SDK_DIR)/src/thread/thread.c \
               $(OOPS_SDK_DIR)/src/time/time.c \
               $(OOPS_SDK_DIR)/src/system/system.c \
               $(OOPS_SDK_DIR)/src/system/freestd.c \
               $(OOPS_SDK_DIR)/src/system/syscall.c \
               $(OOPS_SDK_DIR)/src/system/fs.c \
               $(OOPS_SDK_DIR)/src/agc/agc_compute.c

ENTRY_POINT := mesa_probe_start

# A local link script, and it is temporary.
#
# Mesa uses thread-local storage - its debug layer and its shader backend among others - and
# SELFish's `native_eboot.ld` emits no `PT_TLS` segment, because nothing in the collection has
# ever needed one. Without it the link fails outright on every `__thread` symbol.
#
# `local_tls.ld` is that script with a TLS segment added, kept here so this title can be built
# and the rest of the stack verified while the real change is decided. `REQ-20260915T0001Z-8d72`
# on the SELFish bus asks for it, and asks the question this cannot answer: whether the target's
# loader honours a `PT_TLS` segment in a title's own image at all. **Delete this line and this
# file when that request resolves.**
TARGET_LD_SCRIPT := $(abspath local_tls.ld)

# The import manifest, generated rather than written.
#
# A title's module says which library each imported symbol resolves from. `common/symbols.txt`
# is the shared list, 376 lines, and enough for a freestanding title importing a few dozen vendor
# calls. This one imports 284, and 140 of them are not in that list.
#
# `oops-mesa/tools/generate-imports.sh` reads them out of the binary and asks obSCEne's mined
# corpus where each lives, so nothing here is decided by hand: the corpus places 140, the shared
# list already covers 373, and as of 2026-09-16 none is left unknown.
#
# Run `make imports` after a link, then `make title`. It is not optional and it is not a
# convenience: without `build/imports.txt` mkmodule cannot name the library each import resolves
# from, so it builds no vendor segment, and the container that results is refused by the loader
# rather than failing here. app.mk now stops the build instead of packaging that quietly.
OOPS_MESA ?= $(abspath $(OOPS_APPS_ROOT)/../oops-mesa)
SYMBOLS_FILE := $(abspath build/imports.txt)

include $(OOPS_APPS_ROOT)/common/app.mk

.PHONY: imports
imports: $(BUILD)/$(APP_NAME).elf
	@OOPS_APPS_SYMBOLS=$(OOPS_APPS_ROOT)/common/symbols.txt \
	  bash $(OOPS_MESA)/tools/generate-imports.sh $< $(SYMBOLS_FILE)
