# sandbox-daemon - on-demand filesystem namespace & unsandboxing daemon.
#
#   make check      run host selftest (runs anywhere)
#   make skeleton   freestanding object compilation for target
#   make elf        freestanding payload ELF for elfldr / pldmgr on console
#   make dist       stage conforming release artifact under dist/
#
# Configuration & release formats loaded from app.env

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Host test sources
HOST_TEST_SRCS = sandbox_daemon_selftest.c $(OOPS_SDK_DIR)/src/system/freestd.c

OOPS_FEATURES := inject

# Target payload sources
PAYLOAD_SRCS = sandbox_daemon.c

ENTRY_POINT := sandbox_daemon_start

include $(OOPS_APPS_ROOT)/common/app.mk
