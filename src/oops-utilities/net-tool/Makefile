# net-tool - what the network is, and a socket you can talk to.
#
#   make check      render the panel and round-trip an address on a host (runs anywhere)
#   make skeleton   the drawing compiles freestanding for the target, object only
#   make elf        freestanding payload ELF for the target console
#   make dist       stage conforming release artifact under dist/
#
# Configuration & release formats loaded from app.env

OOPS_APPS_ROOT ?= $(abspath ../../..)

OOPS_FEATURES := display draw input net

# Host test sources
HOST_TEST_SRCS = net-tool_selftest.c net-tool.c \
                 $(OOPS_FEATURE_draw_SRCS) \
                 $(OOPS_SDK_DIR)/src/net/net.c

# Target payload sources
PAYLOAD_SRCS = net-tool.c net-tool_main.c

ENTRY_POINT := net_tool_start

include $(OOPS_APPS_ROOT)/common/app.mk
