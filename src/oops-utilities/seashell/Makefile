# seashell - SeaShell unified homebrew shell for Prospero.
#
#   make check      drive the model and render into memory, verifying both (runs anywhere)
#   make skeleton   the model and renderer compile freestanding for the target, object only
#   make elf        the plain-ELF payload a homebrew loader (elfldr) runs on the console
#   make dist       stages launcher ELF, eboot.bin, and title.zip under dist/
#
# Configuration & release formats loaded from app.env

OOPS_APPS_ROOT ?= $(abspath ../../..)

SKIN_SRCS = skins/skin_registry.c \
            skins/skin_modern.c \
            skins/skin_xmb.c \
            skins/skin_blades.c \
            skins/skin_list.c \
            skins/skin_amber.c \
            skins/skin_revolution.c

# Host test sources
HOST_TEST_SRCS = home_selftest.c home.c $(SKIN_SRCS) \
                 $(OOPS_SDK_DIR)/src/draw/draw.c \
                 $(OOPS_SDK_DIR)/src/draw/png.c \
                 $(OOPS_SDK_DIR)/src/input/keyboard.c \
                 $(OOPS_SDK_DIR)/src/memory/heap.c \
                 $(OOPS_SDK_DIR)/src/system/system.c \
                 $(OOPS_SDK_DIR)/src/system/freestd.c

# Target payload sources
PAYLOAD_SRCS = home.c home_main.c $(SKIN_SRCS) \
               $(OOPS_SDK_DIR)/src/display.c \
               $(OOPS_SDK_DIR)/src/agc/agc_display.c $(OOPS_SDK_DIR)/src/agc/agc_tiler.c \
               $(OOPS_SDK_DIR)/src/agc/agc_compute.c \
               $(OOPS_SDK_DIR)/src/gnm/gnm_display.c \
               $(OOPS_SDK_DIR)/src/input/input.c \
               $(OOPS_SDK_DIR)/src/input/keyboard.c \
               $(OOPS_SDK_DIR)/src/memory/memory.c \
               $(OOPS_SDK_DIR)/src/memory/heap.c \
               $(OOPS_SDK_DIR)/src/system/system.c \
               $(OOPS_SDK_DIR)/src/system/fs.c \
               $(OOPS_SDK_DIR)/src/system/pkg.c \
               $(OOPS_SDK_DIR)/src/system/sysmodule.c \
               $(OOPS_SDK_DIR)/src/system/freestd.c \
               $(OOPS_SDK_DIR)/src/system/syscall.c \
               $(OOPS_SDK_DIR)/src/system/offsets.c \
               $(OOPS_SDK_DIR)/src/net/net.c \
               $(OOPS_SDK_DIR)/src/net/dns.c \
               $(OOPS_SDK_DIR)/src/net/netctl.c \
               $(OOPS_SDK_DIR)/src/time/time.c \
               $(OOPS_SDK_DIR)/src/draw/draw.c \
               $(OOPS_SDK_DIR)/src/draw/png.c

ENTRY_POINT := seashell_start

include $(OOPS_APPS_ROOT)/common/app.mk

