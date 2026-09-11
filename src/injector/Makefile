# injector - dedicated process payload injector.
#
#   make check      run host selftest (runs anywhere)
#   make skeleton   freestanding object compilation for target
#   make elf        freestanding payload ELF for elfldr on console
#   make dist       stage conforming release artifact under dist/
#
# Configuration & release formats loaded from app.env

OOPS_APPS_ROOT ?= $(abspath ../..)

# Host test sources
HOST_TEST_SRCS = injector_selftest.c \
                 $(OOPS_SDK_DIR)/src/system/inject.c \
                 $(OOPS_SDK_DIR)/src/system/target.c \
                 $(OOPS_SDK_DIR)/src/system/loader.c \
                 $(OOPS_SDK_DIR)/src/system/procctl.c \
                 $(OOPS_SDK_DIR)/src/system/krw.c \
                 $(OOPS_SDK_DIR)/src/system/freestd.c \
                 $(OOPS_SDK_DIR)/src/system/syscall.c

# Target payload sources
PAYLOAD_SRCS = injector.c \
               $(OOPS_SDK_DIR)/src/system/freestd.c \
               $(OOPS_SDK_DIR)/src/system/syscall.c \
               $(OOPS_SDK_DIR)/src/system/krw.c \
               $(OOPS_SDK_DIR)/src/system/procctl.c \
               $(OOPS_SDK_DIR)/src/system/loader.c \
               $(OOPS_SDK_DIR)/src/system/target.c \
               $(OOPS_SDK_DIR)/src/system/inject.c

ENTRY_POINT := injector_start

include $(OOPS_APPS_ROOT)/common/app.mk
