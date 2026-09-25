# prism-processor build integration.
#
#   OOPS_PRISM ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/prism-processor)
#   include $(OOPS_PRISM)/oops-prism.mk
#   EXTRA_TARGET_CFLAGS += $(OOPS_PRISM_INCLUDE)
#   EXTRA_LDFLAGS       += $(OOPS_PRISM_LDFLAGS)
#
# The shader template processor. `libultraship`'s GL backend expands the F3D combiner into GLSL
# through it at run time, so this is on the rendering path. Needs spdlog, so include
# `oops-spdlog.mk` first.
ifndef OOPS_PRISM_DIR
OOPS_PRISM_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_PRISM_UPSTREAM ?= $(OOPS_PRISM_DIR)/upstream
OOPS_PRISM_BUILD ?= $(OOPS_PRISM_DIR)/build

# Upstream's three `include_directories`, in its order. The repository root is on the list because
# a consumer writes `#include <prism/processor.h>` and `src/` is where `prism/` lives - both paths
# are needed, and neither is redundant.
OOPS_PRISM_INCLUDE := -I$(OOPS_PRISM_UPSTREAM) -I$(OOPS_PRISM_UPSTREAM)/src
OOPS_PRISM_LIB := $(OOPS_PRISM_BUILD)/libprism.a
OOPS_PRISM_LDFLAGS := $(OOPS_PRISM_LIB)

# Upstream globs `src/**` and `lib/strhash64/*`; there is no `lib/` in the tree at this revision,
# so the glob is `src/` and these six files are all of it.
#
# `main.cpp` is here because upstream's glob catches it and because it is *empty* without
# `PRISM_STANDALONE` - the whole file is inside that `#ifdef`. Leaving it out would be the same
# build; leaving it in means a future revision that puts something outside the guard is compiled
# rather than silently skipped.
OOPS_PRISM_CXX_SRCS := \
    $(OOPS_PRISM_UPSTREAM)/src/main.cpp \
    $(OOPS_PRISM_UPSTREAM)/src/prism/ast.cpp \
    $(OOPS_PRISM_UPSTREAM)/src/prism/lexer.cpp \
    $(OOPS_PRISM_UPSTREAM)/src/prism/processor.cpp \
    $(OOPS_PRISM_UPSTREAM)/src/prism/utils/gv.cpp
OOPS_PRISM_C_SRCS := \
    $(OOPS_PRISM_UPSTREAM)/src/prism/utils/invoke.c

# **Microsoft's GSL is not fetched, and upstream would not fetch it here either.** Its
# `FetchContent_Declare` sits behind `if(NOT PRISM_STANDALONE AND EXISTS
# "/mnt/c/WINDOWS/system32/wsl.exe")` - a test for whether the build is running under WSL, which
# is as odd as it looks. Nothing in `src/` includes a `gsl/` header or names `gsl::`, so on every
# machine without that file prism already builds without it. This is one of those machines.

OOPS_PRISM_TARGET = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                    -fPIC -O2 -w
OOPS_PRISM_OWN    = $(OOPS_PRISM_INCLUDE) $(OOPS_SPDLOG_INCLUDE)
OOPS_PRISM_CFLAGS = $(OOPS_PRISM_TARGET) -nostdlibinc -std=gnu11 $(OOPS_PRISM_OWN) \
                    $(OOPS_POSIX_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)
# **`$(OOPS_LIBCXX_INCLUDE)` comes before the C headers, and that ordering is the whole build.**
# libc++ ships its own `<math.h>` and `<stdlib.h>` which wrap the C library's; `<cmath>` includes
# `<math.h>` expecting to find libc++'s first and stops with an explicit error when it finds the C
# one instead. It also carries the `-nostdinc++ -nostdlibinc` this needs, so those are not repeated.
OOPS_PRISM_CXXFLAGS = $(OOPS_PRISM_TARGET) -std=c++20 $(OOPS_PRISM_OWN) \
                      $(OOPS_LIBCXX_INCLUDE) \
                      $(OOPS_POSIX_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

$(OOPS_PRISM_LIB): $(OOPS_PRISM_C_SRCS) $(OOPS_PRISM_CXX_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_PRISM_BUILD)
	@rm -f $@
	@n=0; objs=""; \
	 for s in $(OOPS_PRISM_C_SRCS); do n=$$((n+1)); o=$(OOPS_PRISM_BUILD)/p$$n.o; \
	   $(TARGET_CC) $(OOPS_PRISM_CFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 for s in $(OOPS_PRISM_CXX_SRCS); do n=$$((n+1)); o=$(OOPS_PRISM_BUILD)/p$$n.o; \
	   $(TARGET_CXX) $(OOPS_PRISM_CXXFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 echo "prism: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs
	@echo "prism: $@"

.PHONY: prism-clean
prism-clean:
	@rm -rf $(OOPS_PRISM_BUILD)
