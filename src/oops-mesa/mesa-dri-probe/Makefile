# mesa-dri-probe - the smallest title that brings GL up through the Gallium DRI frontend.
#
# # How this differs from mesa-winsys-probe, and why they are two titles
#
# `mesa-winsys-probe` calls `radeonsi_screen_create` directly. That is the winsys path, and it is what
# proved unit 5: thirty-five ioctls answered and a non-null `pipe_screen` on hardware
# (oops-mesa worklog 042, `docs/hardware/screen-created-fw1240.md`).
#
# This one calls `oops_gl_create`, which goes through the frontend instead - `driCreateNewScreen3`,
# a drawable, a context, and `dri_make_current`. The frontend creates **its own** screen on the way
# through, so a title that did both would have two, and a failure in either would be reported
# against a stack it did not come from. Keeping them apart is what makes `mesa-winsys-probe` still usable
# as the control when this one stops somewhere.
#
# It also takes the one step past context creation that costs nothing to take: it calls
# `glGetString`. See the note in `mesa_dri_probe_main.c` - a context that is current but cannot
# dispatch a GL call is a distinct failure worth separating from one that never became current.
#
# It renders and it presents. Both were open questions when this file was written and both have
# since been answered on hardware: the frame reaches the panel (oops-mesa worklog 065), so
# `oops_gl_present` returning false is now the surprise rather than the expectation.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# The renderer this title links (common/app.mk): the hosted Mesa stack (oops-mesa D002).
OOPS_RENDERER = mesa

PAYLOAD_SRCS = mesa_dri_probe_main.c \
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

ENTRY_POINT := mesa_dri_probe_start

# A local link script, and it is temporary, for the reason mesa-winsys-probe's copy of it is: SELFish's
# `native_eboot.ld` emits no `PT_TLS` segment and Mesa cannot link without one. Both titles carry
# the same file and both lines go when `REQ-20260915T0001Z-8d72` on the SELFish bus resolves.
#
# It matters more here than there. The frontend's GL dispatch is thread-local by construction -
# `_mesa_glapi_tls_Context` is the table `glGetString` goes through - so this title exercises the
# TLS model worklog 040 settled rather than merely linking against it.
TARGET_LD_SCRIPT := $(abspath local_tls.ld)

# The import manifest, generated rather than written. Same arrangement as mesa-winsys-probe, and for the
# same reason: a Mesa-linked title imports far more than `common/symbols.txt` carries, and every
# attribution comes from obSCEne's mined corpus rather than from anyone's judgement.
#
# Run `make imports` after a link, then `make title`. Not optional: without `build/imports.txt`
# mkmodule builds no vendor segment and the container is refused when it loads.
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
