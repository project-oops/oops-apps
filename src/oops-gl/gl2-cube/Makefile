# gl2-cube - the GL 2.0 cube, drawn by shaders.
#
#   make check      compile, link and draw the cube through oops-gl's software reference
#
# **There is still no `elf` target, and the reason changed on 2026-09-21.**
#
# It used to be that there was no GL 2.0 back end at all: `glCreateShader` and the rest did not
# exist, and the Makefile gave the blocker as whether this pipeline accepts a third parameter
# export. That blocker was never the real one - obSCEne's `REQ-20260919T1745Z-9c3e` measured a
# third parameter arriving byte for byte, and three- and four-parameter draws have run on a
# console since 2026-09-21.
#
# What is true now: the **reference** runs GLSL. The entry points exist, the linker builds a
# program's interface, and the software path runs the vertex shader per vertex and the fragment
# shader per fragment - which is what `make check` measures below, as pixels.
#
# **The console path is built and has never run** (2026-09-21). `oops-sdk/src/gl/glsl_ps.c`
# compiles a fragment shader to gfx1030 - every encoding verified against clang - and the draw
# path copies it into the payload and points the pixel stage at it. What no console has done is
# execute one. obSCEne's `REQ-20260921T1615Z-4e77` asks the gating question: does a *generated*
# pixel shader retire on this pipeline at all, and do its interpolated parameters arrive?
#
# `FORMATS` stays check-only until that answers. Packaging a payload first would be shipping a
# picture nobody has seen, and the honest order is the one gl1-cube followed: measure, then
# package. The day it answers, this file gains an `elf` target and a `gl2_cube_main.c` beside
# it - the same shape gl1-cube already has.
#
# gl1-cube is the fixed-function half of this pair and is pinned to a measured console frame.

OOPS_APPS_ROOT ?= $(abspath ../../..)
EXTRA_CFLAGS += -I$(OOPS_APPS_ROOT)/common

FORMATS = check-only

# The whole GL layer, from the one list in common/app.mk - the check draws, so the shader front
# end and interpreter need the context, the rasteriser and the texture code behind them. The
# self-test supplies its own display and allocator, since the SDK's are direct memory and have
# nothing to talk to on a build machine.
HOST_TEST_SRCS = gl2_cube_selftest.c \
                 $(OOPS_GL_SRCS) \
                 $(OOPS_SDK_DIR)/src/system/system.c \
                 $(OOPS_SDK_DIR)/src/system/freestd.c
HOST_TEST_LIBS = -lm

include $(OOPS_APPS_ROOT)/common/app.mk
