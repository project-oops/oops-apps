# glut-demo - a GLUT program built for Prospero / Trinity by compiling it.
#
#   make elf        the plain-ELF payload a homebrew loader runs
#   make eboot      signed eboot.bin container via selfish
#   make title      complete title package (.zip) for homebrew installation
#
# The point of this app is the build: glut_demo_main.c is ordinary GLUT code with one payload
# entry point at the bottom, and if it links then a port of the same shape links.

OOPS_APPS_ROOT ?= $(abspath ../../..)
EXTRA_TARGET_CFLAGS += -I$(OOPS_APPS_ROOT)/common
EXTRA_CFLAGS += -I$(OOPS_APPS_ROOT)/common

# The SDK's GL, GLU and GLUT, and what they stand on. This list is the answer to "what does a
# GLUT program need from this SDK": the GL implementation, the display, the input devices GLUT
# reads, time for its timers and elapsed clock, and the heap the mipmap builder allocates from.
PAYLOAD_SRCS = glut_demo_main.c \
               $(OOPS_SDK_DIR)/src/display.c \
               $(OOPS_GL_SRCS) \
               $(OOPS_SDK_DIR)/src/gl/gl_glu.c \
               $(OOPS_SDK_DIR)/src/gl/glut.c \
               $(OOPS_SDK_DIR)/src/gl/glut_font.c \
               $(OOPS_SDK_DIR)/src/agc/agc_display.c \
               $(OOPS_SDK_DIR)/src/agc/agc_tiler.c \
               $(OOPS_SDK_DIR)/src/agc/agc_compute.c \
               $(OOPS_SDK_DIR)/src/gnm/gnm_display.c \
               $(OOPS_SDK_DIR)/src/input/input.c \
               $(OOPS_SDK_DIR)/src/input/keyboard.c \
               $(OOPS_SDK_DIR)/src/input/mouse.c \
               $(OOPS_SDK_DIR)/src/memory/memory.c \
               $(OOPS_SDK_DIR)/src/memory/heap.c \
               $(OOPS_SDK_DIR)/src/time/time.c \
               $(OOPS_SDK_DIR)/src/system/system.c \
               $(OOPS_SDK_DIR)/src/system/freestd.c \
               $(OOPS_SDK_DIR)/src/system/syscall.c \
               $(OOPS_SDK_DIR)/src/system/sysmodule.c \
               $(OOPS_SDK_DIR)/src/system/krw.c \
               $(OOPS_SDK_DIR)/src/system/fs.c \
               $(OOPS_SDK_DIR)/src/draw/draw.c

ENTRY_POINT := glut_demo_start

include $(OOPS_APPS_ROOT)/common/app.mk
