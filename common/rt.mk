# The compiler-runtime helpers, for titles that need them. Include from a title's Makefile:
#
#   include $(OOPS_APPS_ROOT)/common/rt.mk
#   PAYLOAD_SRCS += $(OOPS_RT_SRCS)
#
# clang lowers some operations (128-bit division, for one) to compiler-rt builtins, and the
# build container has no builtins archive for `x86_64-unknown-freebsd`. There is no header;
# the compiler emits the calls. `common/rt/rt.c` lists the helpers. `make rt-test` checks
# them against the host's own compiler-rt.

ifndef OOPS_RT_DIR
OOPS_RT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))/rt
endif

OOPS_RT_SRCS := $(OOPS_RT_DIR)/rt.c

.PHONY: rt-test
rt-test:
	@sh $(OOPS_RT_DIR)/tests/run.sh
