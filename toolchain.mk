# The clang major-version pin (oops-mesa#D013), checked before anything compiles.
#
# Included by `common/app.mk`. Both `CC` (host self-tests) and `TARGET_CC` (the title) are
# checked; `TARGET_CXX` is checked in `common/cxx.mk`. Only the major version is pinned:
# libc++ 21's headers need clang 21 (`src/oops-deps/libcxx/upstream.lock`). The version is
# read with `-dumpversion`, because the first line of `--version` carries a vendor prefix.

OOPS_CLANG_MAJOR := 21

# Goals that compile nothing and run without a compiler.
OOPS_TOOLCHAIN_SKIP_GOALS := clean distclean help

ifeq ($(filter $(OOPS_TOOLCHAIN_SKIP_GOALS),$(MAKECMDGOALS)),)

# The last word of the variable, since a compiler may carry a cache prefix.
define oops_cc_major
$(firstword $(subst ., ,$(shell $(lastword $(1)) -dumpversion 2>/dev/null)))
endef

OOPS_HOST_CC_MAJOR   := $(call oops_cc_major,$(CC))
OOPS_TARGET_CC_MAJOR := $(call oops_cc_major,$(TARGET_CC))

ifneq ($(OOPS_HOST_CC_MAJOR),$(OOPS_CLANG_MAJOR))
$(error toolchain: this build is pinned to clang $(OOPS_CLANG_MAJOR) and the host compiler \
`$(lastword $(CC))` reports major "$(OOPS_HOST_CC_MAJOR)" (empty means it is not runnable). \
The two runners that carry the pin are WSL `oops-builder` and the container \
`silkeh/clang:$(OOPS_CLANG_MAJOR)`; see oops-mesa#D013. Do not work around this by passing CC - the \
split between runners is the defect it was written for)
endif

ifneq ($(OOPS_TARGET_CC_MAJOR),$(OOPS_CLANG_MAJOR))
$(error toolchain: this build is pinned to clang $(OOPS_CLANG_MAJOR) and the target compiler \
`$(lastword $(TARGET_CC))` reports major "$(OOPS_TARGET_CC_MAJOR)" (empty means it is not \
runnable). See oops-mesa#D013)
endif

endif
