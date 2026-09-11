# wipeout - native clean-room WipEout engine port for Prospero / Trinity.
#
#   make check      run host platform test (runs anywhere)
#   make skeleton   compiles freestanding platform object for target
#   make elf        the plain-ELF payload a homebrew loader (elfldr) runs on the console
#   make dist       stages conforming release artifacts under dist/ per app.env
#
# Configuration & release formats loaded from app.env

OOPS_APPS_ROOT ?= $(abspath ../..)

# Host test sources
HOST_TEST_SRCS = wipeout_selftest.c platform_oops.c $(OOPS_SDK_DIR)/src/system/system.c

# Target payload sources
PAYLOAD_SRCS = platform_oops.c \
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

ENTRY_POINT := wipeout_start

include $(OOPS_APPS_ROOT)/common/app.mk
