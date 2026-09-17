# pad-viz - a live controller diagram.
#
#   make check      render several controller states into a host buffer and verify (any machine)
#   make skeleton   the drawing compiles freestanding for the target, object only
#   make elf        the plain-ELF payload a homebrew loader (elfldr) runs on the console
#   make dist       stages conforming release artifact under dist/
#
# Configuration & release formats loaded from app.env

OOPS_APPS_ROOT ?= $(abspath ../..)

# Host test sources
HOST_TEST_SRCS = pad-viz_selftest.c pad-viz.c $(OOPS_SDK_DIR)/src/draw/draw.c

# Target payload sources
PAYLOAD_SRCS = pad-viz.c pad-viz_main.c \
               $(OOPS_SDK_DIR)/src/display.c \
               $(OOPS_SDK_DIR)/src/agc/agc_display.c $(OOPS_SDK_DIR)/src/agc/agc_tiler.c \
               $(OOPS_SDK_DIR)/src/agc/agc_compute.c \
               $(OOPS_SDK_DIR)/src/gnm/gnm_display.c $(OOPS_SDK_DIR)/src/input/input.c \
               $(OOPS_SDK_DIR)/src/memory/memory.c $(OOPS_SDK_DIR)/src/system/system.c \
               $(OOPS_SDK_DIR)/src/system/offsets.c $(OOPS_SDK_DIR)/src/system/freestd.c \
               $(OOPS_SDK_DIR)/src/system/syscall.c $(OOPS_SDK_DIR)/src/system/krw.c \
               $(OOPS_SDK_DIR)/src/draw/draw.c

ENTRY_POINT := padviz_start

include $(OOPS_APPS_ROOT)/common/app.mk
