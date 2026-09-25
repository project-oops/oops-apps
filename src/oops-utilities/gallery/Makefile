# gallery - one page per SDK subsystem, driven by hand.
#
#   make check      render every page into a host buffer and verify it drew (runs anywhere)
#   make skeleton   the drawing compiles freestanding for the target, object only
#   make elf        the plain-ELF payload a homebrew loader (elfldr) runs on the console
#   make dist       stages conforming release artifact under dist/
#
# Configuration & release formats loaded from app.env

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Host test sources
# system.c, fs.c and heap.c travel together: the disk sink reaches fs.c, fs.c reaches the
# allocator.
HOST_TEST_SRCS = gallery_selftest.c gallery.c \
                 $(OOPS_SDK_DIR)/src/draw/draw.c \
                 $(OOPS_SDK_DIR)/src/system/system.c \
                 $(OOPS_SDK_DIR)/src/system/fs.c \
                 $(OOPS_SDK_DIR)/src/memory/heap.c \
                 $(OOPS_SDK_DIR)/src/system/freestd.c

OOPS_FEATURES := display draw input keyboard mouse audio audiodec videodec net

# Target payload sources
PAYLOAD_SRCS = gallery.c gallery_main.c

ENTRY_POINT := gallery_start

include $(OOPS_APPS_ROOT)/common/app.mk
