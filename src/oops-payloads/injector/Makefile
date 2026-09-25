# injector - dedicated process payload injector.
#
#   make check      run host selftest (runs anywhere)
#   make skeleton   freestanding object compilation for target
#   make elf        freestanding payload ELF for elfldr on console
#   make dist       stage conforming release artifact under dist/
#
# Configuration & release formats loaded from app.env

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Host test sources
HOST_TEST_SRCS = injector_selftest.c \
                 $(OOPS_FEATURE_inject_SRCS) \
                 $(OOPS_SDK_DIR)/src/system/system.c \
                 $(OOPS_SDK_DIR)/src/system/fs.c \
                 $(OOPS_SDK_DIR)/src/memory/heap.c \
                 $(OOPS_SDK_DIR)/src/system/freestd.c \
                 $(OOPS_SDK_DIR)/src/system/syscall.c

OOPS_FEATURES := inject
EXTRA_UNDEF_ALLOW := ^__payload_(start|end)$$

# Target payload sources
PAYLOAD_SRCS = injector.c

ENTRY_POINT := injector_start

include $(OOPS_APPS_ROOT)/common/app.mk
