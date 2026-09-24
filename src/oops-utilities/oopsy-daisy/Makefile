# OOPSy-daisy - install oops-apps homebrew onto the console.
#
#   make check      host self-test of the pure parts (catalogue parse, install path, render)
#   make skeleton   the drawing compiles freestanding for the target, object only
#   make elf        freestanding payload ELF for the target console
#
# The core runtime (libc, heap, fs, sysmodule, time, math) is added by common/app.mk; this lists
# the subsystems the app drives. The SDK's `net/` (sockets + the HTTPS client) and `system/`
# (syscall dispatch, the platform tables, the zip extractor) are taken by wildcard rather than by
# name so the whole set is picked up as it grows; sce_module.c is the one system source a payload
# must not link (it is the library module's entry), so it is filtered out.

OOPS_APPS_ROOT ?= $(abspath ../../..)

HOST_TEST_SRCS = oopsy-daisy_selftest.c oopsy-daisy.c \
                 $(OOPS_SDK_DIR)/src/draw/draw.c

PAYLOAD_SRCS = oopsy-daisy.c oopsy-daisy_main.c \
               $(OOPS_SDK_DIR)/src/display.c $(OOPS_SDK_DIR)/src/agc/agc_display.c \
               $(OOPS_SDK_DIR)/src/agc/agc_tiler.c $(OOPS_SDK_DIR)/src/agc/agc_compute.c \
               $(OOPS_SDK_DIR)/src/gnm/gnm_display.c \
               $(OOPS_SDK_DIR)/src/input/input.c $(OOPS_SDK_DIR)/src/memory/memory.c \
               $(OOPS_SDK_DIR)/src/draw/draw.c \
               $(wildcard $(OOPS_SDK_DIR)/src/net/*.c) \
               $(filter-out $(OOPS_SDK_DIR)/src/system/sce_module.c,\
                            $(wildcard $(OOPS_SDK_DIR)/src/system/*.c))

ENTRY_POINT := oopsy_daisy_start

include $(OOPS_APPS_ROOT)/common/app.mk
