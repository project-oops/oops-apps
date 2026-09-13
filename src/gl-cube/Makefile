# gl-cube - Clean-room OpenGL 3D Rotating Cube Demo for Prospero / Trinity.
#
#   make elf        the plain-ELF payload a homebrew loader runs
#   make eboot      signed eboot.bin container via selfish
#   make title      complete title package (.zip) for homebrew installation
#   make dist       stages conforming release artifacts under dist/

OOPS_APPS_ROOT ?= $(abspath ../..)
EXTRA_TARGET_CFLAGS += -I$(OOPS_APPS_ROOT)/common
EXTRA_CFLAGS += -I$(OOPS_APPS_ROOT)/common

# Host test sources
HOST_TEST_SRCS = gl_cube_selftest.c \
                 $(OOPS_SDK_DIR)/src/gl/gl_context.c \
                 $(OOPS_SDK_DIR)/src/gl/gl_state.c \
                 $(OOPS_SDK_DIR)/src/gl/gl_matrix.c \
                 $(OOPS_SDK_DIR)/src/gl/gl_draw.c \
                 $(OOPS_SDK_DIR)/src/system/system.c
HOST_TEST_LIBS = -lm

# Target payload sources
PAYLOAD_SRCS = gl_cube_main.c \
               $(OOPS_APPS_ROOT)/common/obj_loader.c \
               $(OOPS_SDK_DIR)/src/display.c \
               $(OOPS_SDK_DIR)/src/gl/gl_context.c \
               $(OOPS_SDK_DIR)/src/gl/gl_state.c \
               $(OOPS_SDK_DIR)/src/gl/gl_matrix.c \
               $(OOPS_SDK_DIR)/src/gl/gl_draw.c \
               $(OOPS_SDK_DIR)/src/agc/agc_display.c \
               $(OOPS_SDK_DIR)/src/agc/agc_tiler.c \
               $(OOPS_SDK_DIR)/src/agc/agc_compute.c \
               $(OOPS_SDK_DIR)/src/gnm/gnm_display.c \
               $(OOPS_SDK_DIR)/src/input/input.c \
               $(OOPS_SDK_DIR)/src/input/keyboard.c \
               $(OOPS_SDK_DIR)/src/input/mouse.c \
               $(OOPS_SDK_DIR)/src/audio/audio.c \
               $(OOPS_SDK_DIR)/src/audio/audiodec.c \
               $(OOPS_SDK_DIR)/src/videodec/videodec.c \
               $(OOPS_SDK_DIR)/src/memory/memory.c \
               $(OOPS_SDK_DIR)/src/time/time.c \
               $(OOPS_SDK_DIR)/src/system/system.c \
               $(OOPS_SDK_DIR)/src/system/freestd.c \
               $(OOPS_SDK_DIR)/src/system/syscall.c \
               $(OOPS_SDK_DIR)/src/system/sysmodule.c \
               $(OOPS_SDK_DIR)/src/system/krw.c \
               $(OOPS_SDK_DIR)/src/draw/draw.c

ENTRY_POINT := gl_cube_start

include $(OOPS_APPS_ROOT)/common/app.mk
