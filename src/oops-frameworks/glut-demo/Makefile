# glut-demo - a GLUT program built for Prospero / Trinity by compiling it.
#
#   make elf        the plain-ELF payload a homebrew loader runs
#   make eboot      signed eboot.bin container via selfish
#   make title      complete title package (.zip) for homebrew installation
#
# glut_demo_main.c is ordinary GLUT code with one payload entry point at the bottom, so
# a port of the same shape links when this one does.

OOPS_APPS_ROOT ?= $(abspath ../../..)
EXTRA_TARGET_CFLAGS += -I$(OOPS_APPS_ROOT)/common
EXTRA_CFLAGS += -I$(OOPS_APPS_ROOT)/common

OOPS_RENDERER = gl1
OOPS_FEATURES := display glu glut input keyboard mouse draw

PAYLOAD_SRCS = glut_demo_main.c

ENTRY_POINT := glut_demo_start

include $(OOPS_APPS_ROOT)/common/app.mk
