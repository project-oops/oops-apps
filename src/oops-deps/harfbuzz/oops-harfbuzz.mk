# HarfBuzz build integration.
#
#   OOPS_HARFBUZZ ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/harfbuzz)
#   include $(OOPS_HARFBUZZ)/oops-harfbuzz.mk
#   EXTRA_TARGET_LDFLAGS += $(OOPS_HB_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_HB_LIB)
#   <your objects>: | $(OOPS_HB_PREFIX_STAMP)
#
# `src/harfbuzz.cc`, upstream's single-translation-unit build of the whole library, as C++
# against the pinned libc++, with FreeType (`HAVE_FREETYPE`) for `hb-ft.h` - include
# `oops-freetype.mk` first. HarfBuzz uses neither exceptions nor RTTI.
#
# Callers include it both ways - `<hb.h>` and `<harfbuzz/hb-ft.h>` - so the include flags carry
# `src/` and a `harfbuzz/`-prefixed view of its headers, copied into `build/prefix/` the way
# `oops-sdl.mk` does for `SDL2/`. The stamp is order-only.
#
# Freestanding or hosted by the including title (`common/dep-sys.mk`); a hosted title also sets
# `OOPS_LIBCXX_HOSTED`, which gives it the libc++ headers that match.
ifndef OOPS_HB_MK
OOPS_HB_MK := 1

ifndef OOPS_HB_DIR
OOPS_HB_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
include $(OOPS_HB_DIR)/../../../common/dep-sys.mk
OOPS_HB_UPSTREAM ?= $(OOPS_HB_DIR)/upstream
OOPS_HB_BUILD ?= $(OOPS_HB_DIR)/build$(OOPS_DEP_BUILD_SUFFIX)
OOPS_HB_PREFIX_DIR := $(OOPS_HB_BUILD)/prefix
OOPS_HB_PREFIX_STAMP := $(OOPS_HB_PREFIX_DIR)/.stamp
OOPS_HB_INCLUDE := -I$(OOPS_HB_UPSTREAM)/src -I$(OOPS_HB_PREFIX_DIR)
OOPS_HB_LIB := $(OOPS_HB_BUILD)/libharfbuzz.a
OOPS_HB_LDFLAGS := $(OOPS_HB_LIB)
OOPS_HB_SRCS := $(OOPS_HB_UPSTREAM)/src/harfbuzz.cc
OOPS_HB_CXXFLAGS = -x c++ -std=c++17 -target x86_64-unknown-freebsd -nostdlib -nostdinc++ \
                   -fPIC -O2 -w -fno-exceptions -fno-rtti \
                   -DHAVE_FREETYPE=1 $(OOPS_FT_INCLUDE) $(OOPS_LIBCXX_INCLUDE) $(OOPS_DEP_SYS)

$(OOPS_HB_PREFIX_STAMP): $(wildcard $(OOPS_HB_UPSTREAM)/src/hb*.h)
	@rm -rf $(OOPS_HB_PREFIX_DIR)/harfbuzz
	@mkdir -p $(OOPS_HB_PREFIX_DIR)/harfbuzz
	@cp $(OOPS_HB_UPSTREAM)/src/hb*.h $(OOPS_HB_PREFIX_DIR)/harfbuzz/
	@touch $@

$(OOPS_HB_LIB): $(OOPS_HB_SRCS) $(OOPS_HB_DIR)/oops-harfbuzz.mk
	@mkdir -p $(OOPS_HB_BUILD)
	@rm -f $@
	$(TARGET_CC) $(OOPS_HB_CXXFLAGS) -c -o $(OOPS_HB_BUILD)/harfbuzz.o $(OOPS_HB_SRCS)
	@a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $(OOPS_HB_BUILD)/harfbuzz.o
	@echo "harfbuzz: $@"

.PHONY: harfbuzz-clean
harfbuzz-clean:
	@rm -rf $(OOPS_HB_BUILD)

endif
