# gl-hello-world - AGC Hardware 3D Triangle Testbed for Prospero / Trinity.
#
#   make check      run host platform test
#   make skeleton   compiles freestanding platform object for target
#   make elf        the plain-ELF payload a homebrew loader runs
#   make eboot      signed eboot.bin container via selfish
#   make title      complete title package (.zip) for homebrew installation
#   make dist       stages conforming release artifacts under dist/

OOPS_APPS_ROOT ?= $(abspath ../..)

# Host test sources
HOST_TEST_SRCS = gl_hello_world_selftest.c gl_hello_world.c $(OOPS_SDK_DIR)/src/system/system.c

# Target payload sources
PAYLOAD_SRCS = gl_hello_world_main.c gl_hello_world.c \
               $(OOPS_SDK_DIR)/src/display.c \
               $(OOPS_SDK_DIR)/src/agc/agc_display.c $(OOPS_SDK_DIR)/src/agc/agc_tiler.c \
               $(OOPS_SDK_DIR)/src/agc/agc_compute.c \
               $(OOPS_SDK_DIR)/src/gnm/gnm_display.c $(OOPS_SDK_DIR)/src/input/input.c \
               $(OOPS_SDK_DIR)/src/input/keyboard.c $(OOPS_SDK_DIR)/src/input/mouse.c \
               $(OOPS_SDK_DIR)/src/audio/audio.c $(OOPS_SDK_DIR)/src/audio/audiodec.c \
               $(OOPS_SDK_DIR)/src/videodec/videodec.c $(OOPS_SDK_DIR)/src/memory/memory.c \
               $(OOPS_SDK_DIR)/src/time/time.c $(OOPS_SDK_DIR)/src/system/system.c \
               $(OOPS_SDK_DIR)/src/system/freestd.c $(OOPS_SDK_DIR)/src/system/syscall.c \
               $(OOPS_SDK_DIR)/src/system/sysmodule.c \
               $(OOPS_SDK_DIR)/src/system/krw.c $(OOPS_SDK_DIR)/src/draw/draw.c

ENTRY_POINT := gl_hello_world_start

include $(OOPS_APPS_ROOT)/common/app.mk

