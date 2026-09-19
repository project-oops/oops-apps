# tracer - dedicated in-process API and GPU command/shader telemetry tracer
#
#   make check      run host selftest
#   make skeleton   freestanding object compilation for target
#   make elf        freestanding payload ELF for elfldr / injector
#   make dist       stage conforming release artifact under dist/
#
# Configuration & release formats loaded from app.env

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Host test sources
HOST_TEST_SRCS = tracer_selftest.c trace_decode.c hook.c shader_dump.c tracer.c
EXTRA_CFLAGS += -DTRACER_SELFTEST

# Target payload sources
PAYLOAD_SRCS = tracer.c \
               hook.c \
               shader_dump.c \
               $(OOPS_SDK_DIR)/src/system/system.c \
               $(OOPS_SDK_DIR)/src/system/fs.c \
               $(OOPS_SDK_DIR)/src/system/freestd.c \
               $(OOPS_SDK_DIR)/src/system/syscall.c \
               $(OOPS_SDK_DIR)/src/system/krw.c

ENTRY_POINT := tracer_start

include $(OOPS_APPS_ROOT)/common/app.mk
