# gl1-probe - OpenGL 1.x conformance checks for oops-gl
#
#   make check      run the check suite on the host software rasteriser
#   make elf        the plain-ELF payload a homebrew loader runs
#   make dist       stages conforming release artifacts under dist/
#
# Each check drives one feature and reads the pixels back to decide.

OOPS_APPS_ROOT ?= $(abspath ../../..)
EXTRA_TARGET_CFLAGS += -I$(OOPS_APPS_ROOT)/common
EXTRA_CFLAGS += -I$(OOPS_APPS_ROOT)/common

OOPS_RENDERER = gl1
OOPS_FEATURES := display glu input keyboard draw

# `fs.c` because `system.c` reaches into it for the log's disk sink.
# `gl_glu.c` beside the GL sources: GLU is a separate library and `$(GL_SRCS)` does not carry it.
HOST_TEST_SRCS = gl1_probe_selftest.c \
                 gl1_probe.c \
                 $(OOPS_GL_SRCS) \
                 $(OOPS_SDK_DIR)/src/gl/gl_glu.c \
                 $(OOPS_SDK_DIR)/src/system/system.c \
                 $(OOPS_SDK_DIR)/src/system/fs.c \
                 $(OOPS_SDK_DIR)/src/memory/heap.c \
                 $(OOPS_SDK_DIR)/src/system/freestd.c
HOST_TEST_LIBS = -lm

PAYLOAD_SRCS = gl1_probe_main.c \
               gl1_probe.c

include $(OOPS_APPS_ROOT)/common/app.mk
