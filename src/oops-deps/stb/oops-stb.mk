# stb_image build integration.
#
#   OOPS_STB ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/stb)
#   include $(OOPS_STB)/oops-stb.mk
#   EXTRA_TARGET_CFLAGS += $(OOPS_STB_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_STB_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_STB_LIB)
#
# `build/stb_impl.c` is the one translation unit that defines `STB_IMAGE_IMPLEMENTATION` before
# including `stb_image.h`, generated as libultraship's CMake generates it. It is plain C and
# compiles against the SDK's libc alone.
ifndef OOPS_STB_DIR
OOPS_STB_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_STB_UPSTREAM ?= $(OOPS_STB_DIR)/upstream
OOPS_STB_BUILD ?= $(OOPS_STB_DIR)/build
OOPS_STB_INCLUDE := -I$(OOPS_STB_UPSTREAM)
OOPS_STB_LIB := $(OOPS_STB_BUILD)/libstb.a
OOPS_STB_LDFLAGS := $(OOPS_STB_LIB)
OOPS_STB_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                  -nostdlibinc -fPIC -O2 -w $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE) \
                  $(OOPS_POSIX_INCLUDE) $(OOPS_STB_INCLUDE)

$(OOPS_STB_LIB): $(OOPS_STB_UPSTREAM)/stb_image.h $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_STB_BUILD)
	@rm -f $@
	@printf '#define STB_IMAGE_IMPLEMENTATION\n#include "stb_image.h"\n' \
	    > $(OOPS_STB_BUILD)/stb_impl.c
	@$(TARGET_CC) $(OOPS_STB_CFLAGS) -c -o $(OOPS_STB_BUILD)/stb_impl.o \
	    $(OOPS_STB_BUILD)/stb_impl.c
	@a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $(OOPS_STB_BUILD)/stb_impl.o
	@echo "stb: $@"

.PHONY: stb-clean
stb-clean:
	@rm -rf $(OOPS_STB_BUILD)
