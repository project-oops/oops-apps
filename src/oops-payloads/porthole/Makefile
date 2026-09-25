# Porthole - the target half of the capture-and-input path.
#
#   make check      the wire contract round-trips on a host, and the scaffold is honestly one
#   make skeleton   the payload half compiles freestanding for the target, object only
#   make elf        the plain-ELF payload an on-console loader (elfldr) runs
#   make dist       stages conforming release artifact under dist/
#
# Configuration & release formats loaded from app.env

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Host test sources
HOST_TEST_SRCS = porthole_selftest.c porthole.c $(OOPS_SDK_DIR)/src/net/net.c

OOPS_FEATURES := display net inject

# Target payload sources
PAYLOAD_SRCS = porthole.c porthole_main.c

ENTRY_POINT := porthole_start

ifdef PORTHOLE_ENCODER_SESSION
EXTRA_TARGET_CFLAGS += -DPORTHOLE_ENCODER_SESSION
endif

include $(OOPS_APPS_ROOT)/common/app.mk
