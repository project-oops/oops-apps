# pltauth-patch - kernel DMAP patcher for SceShellCore entitlement bypass.
#
#   make check      run host selftest (runs anywhere)
#   make skeleton   freestanding object compilation for target
#   make elf        freestanding payload ELF for elfldr on console
#   make dist       stage conforming release artifact under dist/
#
# Configuration & release formats loaded from app.env

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Host test sources
HOST_TEST_SRCS = pltauth_patch_selftest.c $(OOPS_SDK_DIR)/src/system/freestd.c

OOPS_FEATURES := inject

# Target payload sources
PAYLOAD_SRCS = pltauth_patch.c

ENTRY_POINT := pltauth_patch_start

include $(OOPS_APPS_ROOT)/common/app.mk
