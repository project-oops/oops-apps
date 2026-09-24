# gl1-probe - does oops-gl actually do OpenGL 1.x?
#
#   make check      run the check suite on the host software rasteriser
#   make elf        the plain-ELF payload a homebrew loader runs
#   make dist       stages conforming release artifacts under dist/
#
# gl-cube is the pinned hardware *oracle* for one measured frame. This is the breadth half:
# each check drives one feature and reads the pixels back to decide.

OOPS_APPS_ROOT ?= $(abspath ../../..)
EXTRA_TARGET_CFLAGS += -I$(OOPS_APPS_ROOT)/common
EXTRA_CFLAGS += -I$(OOPS_APPS_ROOT)/common

# The GL layer, from the one list in common/app.mk - so a new source in oops-gl does not break
# this app's link the next time it is built.
GL_SRCS = $(OOPS_GL_SRCS)

# `fs.c` because `system.c` reaches into it for the log's disk sink; the payload link has always
# had it, and only the host self-test had to be told.
# `gl_glu.c` beside the GL sources: GLU is a separate library and `$(GL_SRCS)` does not carry it.
HOST_TEST_SRCS = gl1_probe_selftest.c \
                 gl1_probe.c \
                 $(GL_SRCS) \
                 $(OOPS_SDK_DIR)/src/gl/gl_glu.c \
                 $(OOPS_SDK_DIR)/src/system/system.c \
                 $(OOPS_SDK_DIR)/src/system/fs.c \
                 $(OOPS_SDK_DIR)/src/memory/heap.c \
                 $(OOPS_SDK_DIR)/src/system/freestd.c
HOST_TEST_LIBS = -lm

# `gl_glu.c` here as well as in HOST_TEST_SRCS above, and for the same reason: the suite calls
# `gluPickMatrix`, GLU is a separate library, and `$(GL_SRCS)` does not carry it. Listed in only
# one of the two lists, the host self-test passes 109/109 while the payload link fails on the
# symbol - which is exactly what the undefined-symbol gate is for and exactly what it said.
PAYLOAD_SRCS = gl1_probe_main.c \
               gl1_probe.c \
               $(OOPS_SDK_DIR)/src/display.c \
               $(GL_SRCS) \
               $(OOPS_SDK_DIR)/src/gl/gl_glu.c \
               $(OOPS_SDK_DIR)/src/agc/agc_display.c \
               $(OOPS_SDK_DIR)/src/agc/agc_tiler.c \
               $(OOPS_SDK_DIR)/src/agc/agc_compute.c \
               $(OOPS_SDK_DIR)/src/gnm/gnm_display.c \
               $(OOPS_SDK_DIR)/src/input/input.c \
               $(OOPS_SDK_DIR)/src/input/keyboard.c \
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
