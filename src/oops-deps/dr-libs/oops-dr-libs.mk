# dr_libs for a ported title. Include it from a title's Makefile:
#
#   OOPS_DR_LIBS ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/dr-libs)
#   include $(OOPS_DR_LIBS)/oops-dr-libs.mk
#   EXTRA_TARGET_CFLAGS += $(OOPS_DR_LIBS_INCLUDE)
#
# Header-only, so there is no archive and nothing to link: a consumer defines DR_WAV_IMPLEMENTATION
# in exactly one translation unit and the code compiles there.
ifndef OOPS_DR_LIBS_MK
OOPS_DR_LIBS_MK := 1

OOPS_DR_LIBS_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
OOPS_DR_LIBS_INCLUDE := -I$(OOPS_DR_LIBS_DIR)/upstream

endif
