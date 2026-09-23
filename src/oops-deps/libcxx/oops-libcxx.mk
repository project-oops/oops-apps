# libc++ build integration: the C++ standard library, pinned. Include from a title's Makefile
# before `common/cxx.mk`:
#
#   OOPS_LIBCXX ?= $(abspath ../../oops-deps/libcxx)
#   include $(OOPS_LIBCXX)/oops-libcxx.mk
#
#   OOPS_CXX_INCLUDE     += $(OOPS_LIBCXX_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_LIBCXX_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_LIBCXX_LIB)
#
# # Why this is four source files and not forty-five
#
# libc++ ships 45 `.cpp` files, 11367 lines. With localization off - see `include/__config_site`
# for that decision and what it costs - `std::string`, the containers and the algorithms are
# almost entirely headers, and a link of them asks for exactly two things libc++ itself must
# provide: `__libcpp_verbose_abort`, and the out-of-line `basic_string` instantiations the
# headers declare `extern template`.
#
# That was measured with `nm -u` on a compiled object rather than reasoned about, and the file
# list below is what those two symbols live in. A title that needs more will fail to link and
# name what is missing, which is the right way round: `common/app.mk`'s undefined-symbol check
# reads the link, and this list grows from evidence rather than from `ls src/`.
#
# # -nostdlibinc, and why it is not optional
#
# Without it the build machine's `/usr/include` stays on the search path, and libc++ finds
# glibc's headers for a FreeBSD freestanding target: `<wchar.h>` pulling in `bits/wordsize.h` was
# how this first showed up. With it, the only C library in play is oops-sdk's, which is the
# point. The same flag would have saved several rounds of confusion in `oops-sdl.mk`.

ifndef OOPS_LIBCXX_DIR
OOPS_LIBCXX_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_LIBCXX_UPSTREAM ?= $(OOPS_LIBCXX_DIR)/upstream
OOPS_LIBCXX_BUILD ?= $(OOPS_LIBCXX_DIR)/build

# Ours first: `__config_site` and `__assertion_handler` are normally CMake-generated, and
# `sys/_types/_mbstate_t.h` answers libc++'s own second-choice route to `mbstate_t`.
OOPS_LIBCXX_INCLUDE := \
    -nostdinc++ -nostdlibinc \
    -I$(OOPS_LIBCXX_DIR)/include \
    -I$(OOPS_LIBCXX_UPSTREAM)/libcxx/include

OOPS_LIBCXX_LIB := $(OOPS_LIBCXX_BUILD)/libc++.a
OOPS_LIBCXX_LDFLAGS := $(OOPS_LIBCXX_LIB)

# The two symbols a string-and-containers link actually needs, and the files they are in.
OOPS_LIBCXX_SRCS := \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/verbose_abort.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/string.cpp

# `_LIBCPP_BUILDING_LIBRARY` is what libc++'s own sources are compiled with; without it they
# build as a consumer would and the out-of-line instantiations never get emitted.
OOPS_LIBCXX_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                     -fno-exceptions -fno-rtti -fPIC -std=c++20 -O2 -w \
                     -D_LIBCPP_BUILDING_LIBRARY -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS \
                     $(OOPS_LIBCXX_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

TARGET_CXX ?= clang++

# `ar` is handed the list rather than the directory - `common/deps.mk` says what the glob cost.
$(OOPS_LIBCXX_LIB): $(OOPS_LIBCXX_SRCS) $(lastword $(MAKEFILE_LIST)) \
                    $(OOPS_LIBCXX_DIR)/include/__config_site
	@mkdir -p $(OOPS_LIBCXX_BUILD)
	@rm -f $@
	@n=0; objs=""; for src in $(OOPS_LIBCXX_SRCS); do \
	    n=$$((n+1)); o=$(OOPS_LIBCXX_BUILD)/cxx$$n.o; \
	    $(TARGET_CXX) $(OOPS_LIBCXX_CFLAGS) -c -o "$$o" "$$src" || exit 1; objs="$$objs $$o"; \
	done; \
	echo "libc++: compiled $$n sources"; \
	ar_tool=$$(command -v $(AR) 2>/dev/null || command -v llvm-ar 2>/dev/null || command -v ar); \
	"$$ar_tool" rcs $@ $$objs
	@echo "libc++: $@"

.PHONY: libcxx-clean libcxx-upstream libcxx-upstream-clean
libcxx-clean:
	@rm -rf $(OOPS_LIBCXX_BUILD)
	@echo "libc++: removed build/"

libcxx-upstream:
	@UPSTREAM_SPARSE="$$(sed -n 's/^UPSTREAM_SPARSE=//p' $(OOPS_LIBCXX_DIR)/upstream.lock)" \
	 $(OOPS_LIBCXX_DIR)/../../../common/upstream-fetch.sh \
	    "$$(sed -n 's/^UPSTREAM_KIND=//p' $(OOPS_LIBCXX_DIR)/upstream.lock)" \
	    "$$(sed -n 's/^UPSTREAM_URL=//p'  $(OOPS_LIBCXX_DIR)/upstream.lock)" \
	    "$$(sed -n 's/^UPSTREAM_REV=//p'  $(OOPS_LIBCXX_DIR)/upstream.lock)" \
	    "$(OOPS_LIBCXX_UPSTREAM)" "$(OOPS_LIBCXX_DIR)/patches"

libcxx-upstream-clean:
	@rm -rf $(OOPS_LIBCXX_UPSTREAM)
	@echo "libc++: removed upstream/"
