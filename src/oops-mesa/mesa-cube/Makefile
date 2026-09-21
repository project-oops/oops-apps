# mesa-cube - the example title for oops-mesa, and the first one here that is not a probe.
#
# # What it is for
#
# `mesa-probe` and `dri-probe` each answer a question and stop. This one answers
# nothing: it is what roadmap unit 7 asks for - "an example title" - and its job is to be the thing
# a title author reads before writing their own. So it is ordinary on purpose. It sets `USE_MESA`,
# it calls `oops_gl_create`, it draws every frame and presents every frame, and it stops when the
# host closes it.
#
# # Why a cube rather than another triangle
#
# `dri-probe` draws one triangle and hashes it, which is the right shape for a gate and the wrong
# shape for an example: it exercises a single draw of untextured, undepth-tested geometry. This
# one adds the parts a real title uses immediately and nothing had touched here yet - a texture
# and a sampler, a depth buffer with the test on, an index buffer, a matrix pipeline, and a frame
# loop that presents repeatedly rather than once.
#
# That coverage matters more than it did a day ago. Unit 8's CTS subset is blocked on a C++
# standard library this repository does not have (worklog 067), so until that is answered this
# title is the broadest exercise of the GL stack that exists here. It is **an example, not
# conformance** - the tests that would mean something are Khronos's, not ours - and nothing in this
# directory should ever be described as the latter.
#
# Its other use is the present path. `oops_gl_present` reports its cost in four parts for the
# first ten frames and every sixtieth after (oops-mesa D012), and a title that presents once
# produces one sample. This one produces a profile, which is what decides D012's open question.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# The switch that brings Mesa in. A title that sets it is hosted (oops-mesa D002).
USE_MESA = 1

# The example reaches for the SDK the way a real title does: `math.c` for the matrix pipeline
# (this file no longer hand-rolls one) and `input.c` for the pad. `fs.c` was already here for the
# stop file. A hosted title takes the freestanding libc these need from the Mesa sysroot, so no
# `-Iinclude/libc` is added (`common/app.mk` withholds it under USE_MESA) - `math.c` and `input.c`
# include only `<string.h>`/`<sys/types.h>`-level headers, which the sysroot provides.
PAYLOAD_SRCS = mesa_cube_main.c \
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
               $(OOPS_SDK_DIR)/src/math/math.c \
               $(OOPS_SDK_DIR)/src/input/input.c \
               $(OOPS_SDK_DIR)/src/agc/agc_compute.c

ENTRY_POINT := mesa_cube_start

# The same local link script the two Mesa probes carry, and it goes the same way they do: SELFish's
# `native_eboot.ld` emits no `PT_TLS` segment and Mesa cannot link without one, and Mesa's GL
# dispatch is thread-local by construction. All three lines go when `REQ-20260915T0001Z-8d72` on
# the SELFish bus resolves. It also carries the `.preinit_array`/`.init_array`/`.fini_array`
# output sections with `PROVIDE_HIDDEN` bounds, which is not optional: a hosted title has no crt to
# walk them, and until worklog 062 ran them by hand ACO's opcode table stayed zeroed and faulted
# the GPU.
TARGET_LD_SCRIPT := $(abspath local_tls.ld)

# The import manifest, generated rather than written - same arrangement and same reason as the
# probes: a Mesa-linked title imports far more than `common/symbols.txt` carries, and every
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
