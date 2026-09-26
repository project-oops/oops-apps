# seashell - SeaShell unified homebrew shell for Prospero.
#
#   make check      drive the model and render into memory, verifying both (runs anywhere)
#   make skeleton   the model and renderer compile freestanding for the target, object only
#   make elf        the plain-ELF payload a homebrew loader (elfldr) runs on the console
#   make dist       stages launcher ELF, eboot.bin, and title.zip under dist/
#
# Configuration & release formats loaded from app.env

OOPS_APPS_ROOT ?= $(abspath ../../..)
EXTRA_CFLAGS += -I$(OOPS_APPS_ROOT)/common
EXTRA_TARGET_CFLAGS += -I$(OOPS_APPS_ROOT)/common

SKIN_SRCS = skins/skin_registry.c \
            skins/skin_modern.c \
            skins/skin_xmb.c \
            skins/skin_blades.c \
            skins/skin_list.c \
            skins/skin_amber.c \
            skins/skin_revolution.c

OOPS_FEATURES := display draw png input keyboard savedata pkg net

# Host test sources
HOST_TEST_SRCS = home_selftest.c home.c $(SKIN_SRCS) \
                 $(OOPS_FEATURE_draw_SRCS) \
                 $(OOPS_FEATURE_png_SRCS) \
                 $(OOPS_FEATURE_keyboard_SRCS) \
                 $(OOPS_SDK_DIR)/src/memory/heap.c \
                 $(OOPS_SDK_DIR)/src/system/system.c \
                 $(OOPS_SDK_DIR)/src/system/fs.c \
                 $(OOPS_SDK_DIR)/src/system/freestd.c

# Target payload sources
PAYLOAD_SRCS = home.c home_main.c $(SKIN_SRCS)

ENTRY_POINT := seashell_start

include $(OOPS_APPS_ROOT)/common/app.mk

