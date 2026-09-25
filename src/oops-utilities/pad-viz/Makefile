# pad-viz - a live controller diagram.
#
#   make check      render several controller states into a host buffer and verify (any machine)
#   make skeleton   the drawing compiles freestanding for the target, object only
#   make elf        the plain-ELF payload a homebrew loader (elfldr) runs on the console
#   make dist       stages conforming release artifact under dist/
#
# Configuration & release formats loaded from app.env

OOPS_APPS_ROOT ?= $(abspath ../../..)

OOPS_FEATURES := display draw input

# Host test sources
HOST_TEST_SRCS = pad-viz_selftest.c pad-viz.c $(OOPS_FEATURE_draw_SRCS)

# Target payload sources
PAYLOAD_SRCS = pad-viz.c pad-viz_main.c

ENTRY_POINT := padviz_start

include $(OOPS_APPS_ROOT)/common/app.mk
