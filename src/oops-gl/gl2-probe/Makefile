# gl2-probe - OpenGL 2.0 measured against the specification.
#
#   make check      run the suite against oops-gl's software reference
#
# **There is no `elf` target, and the reason is the same one gl2-cube gives.** The reference runs
# GLSL, and as of 2026-09-21 so does the console path - on paper: the fragment stage compiles to
# gfx1030 and the draw path points the pixel stage at it. **No console has executed one.**
#
# obSCEne's `REQ-20260921T1615Z-4e77` asks the gating question - does a generated pixel shader
# retire at all - and packaging a payload before it answers would be shipping a suite whose every
# drawing check might fail for one reason upstream of all of them.
#
# The day it answers, this file gains an `elf` target and a `gl2_probe_main.c` beside gl1-probe's,
# and the suite in `gl2_probe.c` runs on hardware unchanged - which is the whole reason it is
# written as a shared suite with a thin runner rather than as one program.

OOPS_APPS_ROOT ?= $(abspath ../../..)
EXTRA_CFLAGS += -I$(OOPS_APPS_ROOT)/common

FORMATS = check-only

# The whole GL layer, from the one list in common/app.mk. The self-test supplies its own display
# and allocator, since the SDK's are direct memory and have nothing to talk to on a build
# machine.
HOST_TEST_SRCS = gl2_probe_selftest.c \
                 gl2_probe.c \
                 $(OOPS_GL_SRCS) \
                 $(OOPS_SDK_DIR)/src/system/system.c \
                 $(OOPS_SDK_DIR)/src/system/freestd.c
HOST_TEST_LIBS = -lm

include $(OOPS_APPS_ROOT)/common/app.mk
