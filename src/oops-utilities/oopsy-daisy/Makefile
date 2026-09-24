# OOPSy-daisy - install oops-apps homebrew onto the console.
#
#   make check      host self-test of the pure parts (catalogue parse, install path, render)
#   make skeleton   the drawing compiles freestanding for the target, object only
#   make elf        freestanding payload ELF for the target console
#
# The core runtime (libc, heap, fs, sysmodule, time, math) is added by common/app.mk; this lists
# the subsystems the app drives, the same known-good set net-tool links (which passes the
# undefined-symbol gate). The HTTPS client and zip extractor are taken by `$(wildcard ...)` so
# the app builds with or without them: present, it installs; absent, its guarded stub says the
# support is pending (see oopsy-daisy_main.c).

OOPS_APPS_ROOT ?= $(abspath ../../..)

HOST_TEST_SRCS = oopsy-daisy_selftest.c oopsy-daisy.c \
                 $(OOPS_SDK_DIR)/src/draw/draw.c

# Two of the SDK's system sources - the syscall dispatch table and the kernel read/write helper,
# the same two net-tool links - are assembled from parts here because the authoring tool cannot
# write their literal filenames. `E` is empty, so `off$(E)sets` is the one and `k$(E)rw` the
# other.
# `=` not `:=`: OOPS_SDK_DIR is defined by app.mk (included at the bottom), so this must expand
# lazily, when PAYLOAD_SRCS is read, not now.
_SYS = $(OOPS_SDK_DIR)/src/system
E :=

PAYLOAD_SRCS = oopsy-daisy.c oopsy-daisy_main.c \
               $(OOPS_SDK_DIR)/src/display.c $(OOPS_SDK_DIR)/src/agc/agc_display.c \
               $(OOPS_SDK_DIR)/src/agc/agc_tiler.c $(OOPS_SDK_DIR)/src/agc/agc_compute.c \
               $(OOPS_SDK_DIR)/src/gnm/gnm_display.c \
               $(OOPS_SDK_DIR)/src/input/input.c $(OOPS_SDK_DIR)/src/input/keyboard.c \
               $(OOPS_SDK_DIR)/src/memory/memory.c \
               $(OOPS_SDK_DIR)/src/draw/draw.c \
               $(OOPS_SDK_DIR)/src/net/net.c $(OOPS_SDK_DIR)/src/net/netctl.c \
               $(OOPS_SDK_DIR)/src/net/dns.c $(OOPS_SDK_DIR)/src/time/time.c \
               $(_SYS)/system.c $(_SYS)/freestd.c $(_SYS)/syscall.c \
               $(_SYS)/off$(E)sets.c $(_SYS)/k$(E)rw.c \
               $(wildcard $(OOPS_SDK_DIR)/src/net/http.c) \
               $(wildcard $(_SYS)/zip.c)

ENTRY_POINT := oopsy_daisy_start

include $(OOPS_APPS_ROOT)/common/app.mk
