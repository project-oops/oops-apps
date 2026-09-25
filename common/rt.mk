# The compiler-runtime helpers, for titles that need them. Include from a title's Makefile:
#
#   include $(OOPS_APPS_ROOT)/common/rt.mk
#   PAYLOAD_SRCS += $(OOPS_RT_SRCS)
#
# # What this is, and why it is not in oops-sdk
#
# clang lowers a handful of operations to calls into compiler-rt's builtins archive rather than to
# instructions. The build container has that archive for `linux/` and `windows/` only, not for
# `x86_64-unknown-freebsd`, so on this target those calls have nothing to bind to - and a payload
# link does not report an unresolved symbol, so the first sign is a fault on the console.
#
# It sits here rather than in `oops-sdk` because it is not the C library: nothing in it has a
# standard header or a name a program would write. `common/rt/rt.c` says which helpers are present
# and why the rest are not.
#
# # There is no include directory
#
# Nothing includes this. A caller does not call `__udivti3`; the compiler does, from an ordinary
# `/` on a 128-bit value. The only thing a title has to do is put the object in the link.
#
# # It is tested on the host
#
#   sh $(OOPS_APPS_ROOT)/common/rt/tests/run.sh
#
# Against the host's own compiler-rt, which is the only oracle that is not the subject - see the
# comment at the top of `rt_test.c` for the version of that test that passed while proving nothing.

ifndef OOPS_RT_DIR
OOPS_RT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))/rt
endif

OOPS_RT_SRCS := $(OOPS_RT_DIR)/rt.c

.PHONY: rt-test
rt-test:
	@sh $(OOPS_RT_DIR)/tests/run.sh
