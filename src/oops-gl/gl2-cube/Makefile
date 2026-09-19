# gl2-cube - the GL 2.0 oracle, once there is a GL 2.0 back end to record.
#
#   make check      push the shaders through oops-gl's GLSL front end
#
# **There is no `elf` target yet, deliberately.** gl2-cube cannot draw: the back end is blocked
# on whether this pipeline accepts a third parameter export, which is what a GLSL `varying`
# compiles to (obSCEne REQ-...-f9d3). A payload that built and drew nothing would be the silent
# lie this collection refuses everywhere else - so until the back end exists there is nothing to
# link, and `FORMATS` says so.
#
# What runs today is real: the two shaders in gl2_cube_shaders.h go through the actual
# preprocessor, parser and semantic stage, so they are known-good GLSL 1.10 before the code
# generator and the hardware path are being debugged at the same time.
#
# gl1-cube is the fixed-function half of this pair, and is pinned to a measured console frame.

OOPS_APPS_ROOT ?= $(abspath ../../..)
EXTRA_CFLAGS += -I$(OOPS_APPS_ROOT)/common

FORMATS = check-only

HOST_TEST_SRCS = gl2_cube_selftest.c \
                 $(OOPS_SDK_DIR)/src/gl/glsl_lex.c \
                 $(OOPS_SDK_DIR)/src/gl/glsl_pp.c \
                 $(OOPS_SDK_DIR)/src/gl/glsl_parse.c \
                 $(OOPS_SDK_DIR)/src/gl/glsl_sema.c
HOST_TEST_LIBS = -lm

include $(OOPS_APPS_ROOT)/common/app.mk
