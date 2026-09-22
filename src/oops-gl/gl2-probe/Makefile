# gl2-probe - OpenGL 2.0 measured against the specification.
#
#   make check      run the suite against oops-gl's software reference
#   make eboot      the artifact that deploys to a console
#   make title      the installable package
#
# **The suite now packages, because the thing it measures now exists.** Two answers opened the
# gate that held it back. obSCEne's `REQ-20260921T1615Z-4e77` established that a generated pixel
# shader retires on this pipeline and its interpolated parameters arrive; `-2a45` then measured a
# compiled shader's `image_sample` returning its texel under whole-quad mode. Until both, a
# payload would have reported a block of checks as failed for one reason upstream of all of them,
# which is the thing the ordering existed to avoid.
#
# **No `elf` in FORMATS**, as with gl2-cube. That format stages the bare ELF for a homebrew
# loader; this collection installs native titles, so the artifacts worth staging are the eboot
# and the title package. The `.elf` is still built - it is the intermediate both of those are
# made from, linked against `selfish/link/native_eboot.ld`.
#
# The suite in `gl2_probe.c` runs on hardware unchanged, which is the whole reason it is written
# as a shared suite with a thin runner rather than as one program: `gl2_probe_selftest.c` is the
# host's runner and `gl2_probe_main.c` the console's, and neither owns a check.

OOPS_APPS_ROOT ?= $(abspath ../../..)
EXTRA_CFLAGS += -I$(OOPS_APPS_ROOT)/common
EXTRA_TARGET_CFLAGS += -I$(OOPS_APPS_ROOT)/common

FORMATS = eboot title

# The whole GL layer, from the one list in common/app.mk. The self-test supplies its own display
# and allocator, since the SDK's are direct memory and have nothing to talk to on a build
# machine.
HOST_TEST_SRCS = gl2_probe_selftest.c \
                 gl2_probe.c \
                 $(OOPS_GL_SRCS) \
                 $(OOPS_SDK_DIR)/src/system/system.c \
                 $(OOPS_SDK_DIR)/src/system/freestd.c
HOST_TEST_LIBS = -lm

# The payload, which is the same suite behind gl2-probe's own runner. The list matches
# gl1-probe's: a drawing probe needs the display, the allocator, the GL layer and the AGC
# submission path behind it, and a GL 2.0 one needs the shader front end and the back end in
# `glsl_ps.c` as well - all of which `OOPS_GL_SRCS` already carries.
PAYLOAD_SRCS = gl2_probe_main.c \
               gl2_probe.c \
               $(OOPS_SDK_DIR)/src/display.c \
               $(OOPS_GL_SRCS) \
               $(OOPS_SDK_DIR)/src/agc/agc_display.c \
               $(OOPS_SDK_DIR)/src/agc/agc_tiler.c \
               $(OOPS_SDK_DIR)/src/agc/agc_compute.c \
               $(OOPS_SDK_DIR)/src/gnm/gnm_display.c \
               $(OOPS_SDK_DIR)/src/input/input.c \
               $(OOPS_SDK_DIR)/src/memory/memory.c \
               $(OOPS_SDK_DIR)/src/memory/heap.c \
               $(OOPS_SDK_DIR)/src/time/time.c \
               $(OOPS_SDK_DIR)/src/system/system.c \
               $(OOPS_SDK_DIR)/src/system/freestd.c \
               $(OOPS_SDK_DIR)/src/system/syscall.c \
               $(OOPS_SDK_DIR)/src/system/sysmodule.c \
               $(OOPS_SDK_DIR)/src/system/krw.c \
               $(OOPS_SDK_DIR)/src/system/fs.c \
               $(OOPS_SDK_DIR)/src/draw/draw.c \
               $(OOPS_SDK_DIR)/src/system/procparam.c

include $(OOPS_APPS_ROOT)/common/app.mk
