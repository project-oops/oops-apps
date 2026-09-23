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

# # The sources, and why the list is explicit rather than a wildcard
#
# It was two files - `verbose_abort.cpp` and `string.cpp` - for as long as the only consumer was
# ACO, which needs eight definitions and no streams. The GL CTS needs streams
# (`oops-apps#D007`), so localization is on in `__config_site` and this list grew to what that
# requires.
#
# **It is still not `$(wildcard src/*.cpp)`**, and the reason is in the survey behind D007: of
# the 45 sources upstream ships, 32 compile for this target and 13 do not. Nine of those are
# threading, which `_LIBCPP_HAS_THREADS 0` switches off, and the rest want a platform facility
# that is genuinely absent. A glob would add them, fail the build, and invite somebody to switch
# a `__config_site` line to make the error go away - which is how a library ends up claiming a
# facility it cannot deliver. Naming them means adding one is a decision.
#
# Re-run `tools/libcxx-survey.sh` after touching `__config_site` or oops-sdk's C library; it
# compiles all 45 and prints what each failure is waiting for.
OOPS_LIBCXX_SRCS := \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/algorithm.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/any.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/bind.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/call_once.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/error_category.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/exception.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/fstream.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/functional.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/hash.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/ios.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/ios.instantiations.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/iostream.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/locale.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/memory.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/memory_resource.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/new.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/new_handler.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/new_helpers.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/optional.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/ostream.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/print.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/random_shuffle.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/regex.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/stdexcept.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/string.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/strstream.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/system_error.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/typeinfo.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/valarray.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/variant.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/vector.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/verbose_abort.cpp \
    $(OOPS_LIBCXX_DIR)/src/rune_table.c

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
