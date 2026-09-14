# net-tool - what the network is, and a socket you can talk to.
#
#   make check      render the panel and round-trip an address on a host (runs anywhere)
#   make skeleton   the drawing compiles freestanding for the target, object only
#   make elf        freestanding payload ELF for the target console
#   make dist       stage conforming release artifact under dist/
#
# Configuration & release formats loaded from app.env

OOPS_APPS_ROOT ?= $(abspath ../..)

# Host test sources
HOST_TEST_SRCS = net-tool_selftest.c net-tool.c \
                 $(OOPS_SDK_DIR)/src/draw/draw.c \
                 $(OOPS_SDK_DIR)/src/net/net.c

# Target payload sources
PAYLOAD_SRCS = net-tool.c net-tool_main.c \
               $(OOPS_SDK_DIR)/src/display.c $(OOPS_SDK_DIR)/src/agc/agc_display.c \
               $(OOPS_SDK_DIR)/src/agc/agc_tiler.c $(OOPS_SDK_DIR)/src/gnm/gnm_display.c \
               $(OOPS_SDK_DIR)/src/input/input.c $(OOPS_SDK_DIR)/src/memory/memory.c \
               $(OOPS_SDK_DIR)/src/net/net.c $(OOPS_SDK_DIR)/src/net/netctl.c \
               $(OOPS_SDK_DIR)/src/net/dns.c $(OOPS_SDK_DIR)/src/time/time.c \
               $(OOPS_SDK_DIR)/src/system/system.c $(OOPS_SDK_DIR)/src/system/offsets.c \
               $(OOPS_SDK_DIR)/src/system/sysmodule.c \
               $(OOPS_SDK_DIR)/src/system/freestd.c $(OOPS_SDK_DIR)/src/system/syscall.c \
               $(OOPS_SDK_DIR)/src/system/krw.c $(OOPS_SDK_DIR)/src/draw/draw.c

ENTRY_POINT := net_tool_start

include $(OOPS_APPS_ROOT)/common/app.mk
