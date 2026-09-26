# tinyxml2 build integration.
#
#   OOPS_TINYXML2 ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/tinyxml2)
#   include $(OOPS_TINYXML2)/oops-tinyxml2.mk
#   EXTRA_TARGET_CFLAGS  += $(OOPS_TINYXML2_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_TINYXML2_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_TINYXML2_LIB)
#
# One C++ translation unit, compiled unpatched with `TARGET_CXX`. It needs libc++'s headers, so
# include `oops-libcxx.mk` first.
ifndef OOPS_TINYXML2_DIR
OOPS_TINYXML2_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_TINYXML2_UPSTREAM ?= $(OOPS_TINYXML2_DIR)/upstream
OOPS_TINYXML2_BUILD ?= $(OOPS_TINYXML2_DIR)/build
OOPS_TINYXML2_INCLUDE := -I$(OOPS_TINYXML2_UPSTREAM)
OOPS_TINYXML2_LIB := $(OOPS_TINYXML2_BUILD)/libtinyxml2.a
OOPS_TINYXML2_LDFLAGS := $(OOPS_TINYXML2_LIB)
OOPS_TINYXML2_SRCS := $(OOPS_TINYXML2_UPSTREAM)/tinyxml2.cpp
OOPS_TINYXML2_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                       -nostdinc++ -fPIC -fno-stack-protector -std=c++20 -O2 -w \
                       $(OOPS_SDK_INCLUDE) $(OOPS_LIBCXX_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE) \
                       $(OOPS_POSIX_INCLUDE) $(OOPS_TINYXML2_INCLUDE)

# `ar` is handed the object rather than a directory glob (see `common/deps.mk`).
$(OOPS_TINYXML2_LIB): $(OOPS_TINYXML2_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_TINYXML2_BUILD)
	@rm -f $@
	@$(TARGET_CXX) $(OOPS_TINYXML2_CFLAGS) -c -o $(OOPS_TINYXML2_BUILD)/tinyxml2.o \
	    $(OOPS_TINYXML2_UPSTREAM)/tinyxml2.cpp
	@a=$$(command -v $(AR) 2>/dev/null || command -v ar); \
	 "$$a" rcs $@ $(OOPS_TINYXML2_BUILD)/tinyxml2.o
	@echo "tinyxml2: $@"

.PHONY: tinyxml2-clean
tinyxml2-clean:
	@rm -rf $(OOPS_TINYXML2_BUILD)
