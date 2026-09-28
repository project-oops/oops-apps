# libmodplug build integration.
#
#   OOPS_MODPLUG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libmodplug)
#   include $(OOPS_MODPLUG)/oops-libmodplug.mk
#   EXTRA_TARGET_LDFLAGS += $(OOPS_MODPLUG_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_MODPLUG_LIB)
#
# Every source in `src/`, which is upstream's CMake list, as C++ against the pinned libc++ - so a
# title including this also links libc++ (`src/oops-deps/libcxx`). The defines are the ones its
# CMakeLists.txt sets after its configure checks: the three headers and `sinf` all exist here.
# `MODPLUG_STATIC` drops the export decoration a shared build wants.
ifndef OOPS_MODPLUG_MK
OOPS_MODPLUG_MK := 1

ifndef OOPS_MODPLUG_DIR
OOPS_MODPLUG_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_MODPLUG_UPSTREAM ?= $(OOPS_MODPLUG_DIR)/upstream
OOPS_MODPLUG_BUILD ?= $(OOPS_MODPLUG_DIR)/build
# Callers include `<libmodplug/modplug.h>`, the installed layout, where the tree has
# `src/modplug.h`; `build/prefix/libmodplug/` is that header copied under the installed name,
# as `oops-sdl.mk` does for `SDL2/`. Order a consumer's objects after `OOPS_MODPLUG_PREFIX_STAMP`.
OOPS_MODPLUG_PREFIX_DIR := $(OOPS_MODPLUG_BUILD)/prefix
OOPS_MODPLUG_PREFIX_STAMP := $(OOPS_MODPLUG_PREFIX_DIR)/.stamp
OOPS_MODPLUG_INCLUDE := -I$(OOPS_MODPLUG_UPSTREAM)/src -I$(OOPS_MODPLUG_PREFIX_DIR) -DMODPLUG_STATIC

$(OOPS_MODPLUG_PREFIX_STAMP): $(OOPS_MODPLUG_UPSTREAM)/src/modplug.h
	@mkdir -p $(OOPS_MODPLUG_PREFIX_DIR)/libmodplug
	@cp $(OOPS_MODPLUG_UPSTREAM)/src/modplug.h $(OOPS_MODPLUG_PREFIX_DIR)/libmodplug/
	@touch $@
OOPS_MODPLUG_LIB := $(OOPS_MODPLUG_BUILD)/libmodplug.a
OOPS_MODPLUG_LDFLAGS := $(OOPS_MODPLUG_LIB)
OOPS_MODPLUG_SRCS := $(wildcard $(OOPS_MODPLUG_UPSTREAM)/src/*.cpp)
OOPS_MODPLUG_CFLAGS = -x c++ -std=c++11 -target x86_64-unknown-freebsd -ffreestanding \
                      -fno-builtin -nostdlib -nostdinc++ -nostdlibinc -fPIC -O2 -w \
                      -DMODPLUG_BUILD=1 -DHAVE_STDINT_H -DHAVE_STRINGS_H -DHAVE_SINF \
                      $(OOPS_LIBCXX_INCLUDE) $(OOPS_MODPLUG_INCLUDE) \
                      -I$(OOPS_MODPLUG_UPSTREAM)/src/libmodplug $(OOPS_POSIX_INCLUDE) \
                      $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

$(OOPS_MODPLUG_LIB): $(OOPS_MODPLUG_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_MODPLUG_BUILD)
	@rm -f $@
	@n=0; objs=""; for s in $(OOPS_MODPLUG_SRCS); do n=$$((n+1)); o=$(OOPS_MODPLUG_BUILD)/mp$$n.o; \
	   $(TARGET_CC) $(OOPS_MODPLUG_CFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 echo "modplug: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs
	@echo "modplug: $@"

.PHONY: modplug-clean
modplug-clean:
	@rm -rf $(OOPS_MODPLUG_BUILD)

endif
