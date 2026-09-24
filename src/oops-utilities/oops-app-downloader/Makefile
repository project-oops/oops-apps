# oops-app-downloader - browse and install oops-apps homebrew on the console.
#
#   make check      host self-test of the pure parts (runs anywhere)
#   make skeleton   the drawing compiles freestanding for the target, object only
#   make elf        freestanding payload ELF for the target console
#
# Configuration & release formats loaded from app.env. The core runtime (libc, heap, fs,
# sysmodule, time, math) is added by common/app.mk; this lists the subsystems the app itself
# drives: the display, the pad, the network, and the software canvas.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Host test sources: the pure render + helper, plus the software canvas they draw through.
HOST_TEST_SRCS = oops-app-downloader_selftest.c oops-app-downloader.c \
                 $(OOPS_SDK_DIR)/src/draw/draw.c

# Target payload sources.
PAYLOAD_SRCS = oops-app-downloader.c oops-app-downloader_main.c \
               $(OOPS_SDK_DIR)/src/display.c $(OOPS_SDK_DIR)/src/agc/agc_display.c \
               $(OOPS_SDK_DIR)/src/agc/agc_tiler.c $(OOPS_SDK_DIR)/src/agc/agc_compute.c \
               $(OOPS_SDK_DIR)/src/gnm/gnm_display.c \
               $(OOPS_SDK_DIR)/src/input/input.c $(OOPS_SDK_DIR)/src/memory/memory.c \
               $(OOPS_SDK_DIR)/src/net/net.c $(OOPS_SDK_DIR)/src/net/netctl.c \
               $(OOPS_SDK_DIR)/src/net/dns.c $(OOPS_SDK_DIR)/src/time/time.c \
               $(OOPS_SDK_DIR)/src/system/system.c $(OOPS_SDK_DIR)/src/system/freestd.c \
               $(OOPS_SDK_DIR)/src/system/syscall.c $(OOPS_SDK_DIR)/src/draw/draw.c

# Two further `src/system/` sources that net-tool's Makefile lists are left out of PAYLOAD_SRCS
# above (the syscall dispatch table and the kernel-memory helper). If `make elf`'s
# undefined-symbol gate reports missing system symbols, copy those two lines in from
# `../net-tool/Makefile` - they were omitted only because the authoring tool could not write
# their filenames, not by design.

ENTRY_POINT := oops_app_downloader_start

include $(OOPS_APPS_ROOT)/common/app.mk
