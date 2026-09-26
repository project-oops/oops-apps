# prism-processor build integration.
#
#   OOPS_PRISM ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/prism-processor)
#   include $(OOPS_PRISM)/oops-prism.mk
#   EXTRA_TARGET_CFLAGS += $(OOPS_PRISM_INCLUDE)
#   EXTRA_LDFLAGS       += $(OOPS_PRISM_LDFLAGS)
#
# The shader template processor `libultraship`'s GL backend uses to expand the F3D combiner into
# GLSL at run time. Needs spdlog, so include `oops-spdlog.mk` first.
ifndef OOPS_PRISM_DIR
OOPS_PRISM_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_PRISM_UPSTREAM ?= $(OOPS_PRISM_DIR)/upstream
OOPS_PRISM_BUILD ?= $(OOPS_PRISM_DIR)/build

# Upstream's `include_directories`, in its order.
OOPS_PRISM_INCLUDE := -I$(OOPS_PRISM_UPSTREAM) -I$(OOPS_PRISM_UPSTREAM)/src
OOPS_PRISM_LIB := $(OOPS_PRISM_BUILD)/libprism.a
OOPS_PRISM_LDFLAGS := $(OOPS_PRISM_LIB)

# Upstream globs `src/**` and `lib/strhash64/*`; the pinned tree has no `lib/`. `main.cpp` is
# kept as upstream's glob keeps it; without `PRISM_STANDALONE` it compiles to nothing.
OOPS_PRISM_CXX_SRCS := \
    $(OOPS_PRISM_UPSTREAM)/src/main.cpp \
    $(OOPS_PRISM_UPSTREAM)/src/prism/ast.cpp \
    $(OOPS_PRISM_UPSTREAM)/src/prism/lexer.cpp \
    $(OOPS_PRISM_UPSTREAM)/src/prism/processor.cpp \
    $(OOPS_PRISM_UPSTREAM)/src/prism/utils/gv.cpp
OOPS_PRISM_C_SRCS := \
    $(OOPS_PRISM_UPSTREAM)/src/prism/utils/invoke.c

# Microsoft's GSL is not fetched: upstream fetches it only when building under WSL, and nothing
# in `src/` includes a `gsl/` header or names `gsl::`.

OOPS_PRISM_TARGET = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                    -fPIC -O2 -w
OOPS_PRISM_OWN    = $(OOPS_PRISM_INCLUDE) $(OOPS_SPDLOG_INCLUDE)
OOPS_PRISM_CFLAGS = $(OOPS_PRISM_TARGET) -nostdlibinc -std=gnu11 $(OOPS_PRISM_OWN) \
                    $(OOPS_POSIX_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)
# `$(OOPS_LIBCXX_INCLUDE)` comes before the C headers: `<cmath>` requires libc++'s wrapping
# `<math.h>` ahead of the C library's. It also carries `-nostdinc++ -nostdlibinc`.
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
