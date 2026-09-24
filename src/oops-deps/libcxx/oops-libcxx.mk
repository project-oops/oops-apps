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
# # It was two source files and is now thirty-odd, and the reason it grew
#
# For as long as the only consumer was ACO, a link asked libc++ for exactly two things -
# `__libcpp_verbose_abort` and the out-of-line `basic_string` instantiations - and the list was
# the two files those live in. That was measured with `nm -u` rather than reasoned about.
#
# The GL CTS asks for streams, which means localization, which means most of the library
# (`oops-apps#D007`). The list below is what compiles; it is still not `$(wildcard src/*.cpp)`,
# for the reason given above it.
#
# # Two configurations, which is the thing to understand before changing anything here
#
# `OOPS_LIBCXX_HOSTED` picks between a build against oops-sdk's freestanding C library and one
# against the Mesa sysroot's real one. They differ in include paths, in exceptions and RTTI, in
# whether `-ffreestanding` applies, and in whether `rune_table.c` is compiled - and they land in
# different directories because a title that linked the wrong one would resolve every symbol and
# read the wrong bytes. Each fork below says why it differs.
#
# # -nostdlibinc, in the freestanding build, and why it is not optional there
#
# Without it the build machine's `/usr/include` stays on the search path, and libc++ finds
# glibc's headers for a FreeBSD freestanding target: `<wchar.h>` pulling in `bits/wordsize.h` was
# how this first showed up. With it, the only C library in play is oops-sdk's, which is the
# point. The hosted build does not use it, because `--sysroot` does the same job properly: it
# points the whole search at the sysroot rather than subtracting the host's.

ifndef OOPS_LIBCXX_DIR
OOPS_LIBCXX_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_LIBCXX_UPSTREAM ?= $(OOPS_LIBCXX_DIR)/upstream
# # Two builds of the same source, and they cannot be mixed
#
# `OOPS_LIBCXX_HOSTED = 1` before including this file selects the **hosted** configuration: built
# against the Mesa sysroot's C library rather than oops-sdk's freestanding one, for a title that
# sets `USE_MESA`. Everything else takes the freestanding build, which is the default and is what
# `cxx-throw` and any plain C++ title link.
#
# **They disagree about `FILE` and `clock_t`** - the sysroot's `__clock_t` is `int` where
# oops-sdk's is `int64_t`, which is the same collision `common/app.mk` describes for the C
# headers. A title that linked the wrong one would resolve every symbol and read the wrong
# bytes, which is why they land in different directories rather than overwriting each other.
#
# The hosted build is the smaller change of the two: the sysroot has real `locale.h` and
# `runetype.h`, and `librune.a` already defines `_DefaultRuneLocale`, so almost none of
# `include/freestanding/` is wanted. See `oops-apps#D007` for what the freestanding one needed
# instead.
#
# **The locale *backend* is the exception, and it belongs to both.** `include/__locale_dir/`
# sits outside `freestanding/` on purpose: the sysroot having `<xlocale.h>` is not the same as
# the console exporting `newlocale`, `strtod_l` and the other seventeen `_l` names, and it does
# not. Which backend libc++ uses is a question about the *target*, not about which headers are
# on disk - the header beside that path says so at length.
ifeq ($(OOPS_LIBCXX_HOSTED),1)
OOPS_LIBCXX_BUILD ?= $(OOPS_LIBCXX_DIR)/build-hosted
OOPS_MESA_SYSROOT ?= $(abspath $(OOPS_LIBCXX_DIR)/../../../../oops-mesa/toolchain/sysroot)
else
OOPS_LIBCXX_BUILD ?= $(OOPS_LIBCXX_DIR)/build
endif

# # Two directories, because there are two kinds of title
#
# `include/` holds what libc++ needs **whatever C library is underneath**: `__config_site` and
# `__assertion_handler`, both of which CMake would normally generate.
#
# `include/__locale_dir/support/freebsd.h` is also in the always-on half, and that is not an
# oversight: it selects libc++'s *locale backend*, which is a question about what the target
# exports rather than about which headers exist. See the file itself.
#
# `include/freestanding/` holds the shims that stand in for a C library this SDK does not have -
# `locale.h`, `nl_types.h`, `runetype.h`, `inttypes.h` and the `mbstate_t` typedef. They are
# correct for a title built against oops-sdk's freestanding libc and
# **actively wrong for a hosted one**, where the Mesa sysroot already has the real versions:
# putting them on a hosted title's include path shadows FreeBSD's `locale.h` and `runetype.h`
# and breaks 128 of the CTS framework's 220 sources, with errors that name neither this
# directory nor the sysroot.
#
# `common/app.mk` says the same thing one layer up about oops-sdk's `include/libc`, which it
# empties for `USE_MESA` titles because the sysroot's `__clock_t` is `int` where oops-sdk's
# `clock_t` is `int64_t`. This is that rule applied to the C++ library's half.
#
# **The archive this file builds is the freestanding one.** A hosted C++ title needs libc++ built
# against the sysroot instead, and cannot link this one - they disagree about `FILE`. Nothing
# builds that yet; `gl-cts` is the first title to want it.
ifeq ($(OOPS_LIBCXX_HOSTED),1)
# No `-nostdlibinc` and no `include/freestanding`: the sysroot supplies the C headers and they
# are the real ones. Shadowing them is what broke 128 of the CTS framework's 220 sources.
OOPS_LIBCXX_INCLUDE := \
    -nostdinc++ \
    -I$(OOPS_LIBCXX_DIR)/include \
    -I$(OOPS_LIBCXX_UPSTREAM)/libcxx/include \
    -I$(OOPS_LIBCXX_UPSTREAM)/libcxx/src
else
OOPS_LIBCXX_INCLUDE := \
    -nostdinc++ -nostdlibinc \
    -I$(OOPS_LIBCXX_DIR)/include \
    -I$(OOPS_LIBCXX_DIR)/include/freestanding \
    -I$(OOPS_LIBCXX_UPSTREAM)/libcxx/include \
    -I$(OOPS_LIBCXX_UPSTREAM)/libcxx/src
endif

OOPS_LIBCXX_LIB := $(OOPS_LIBCXX_BUILD)/libc++.a
# `--whole-archive`, for the reason `cxx.mk:125` and `oops-sdl.mk` both give: **`app.mk` puts
# LDFLAGS before the sources on the link line**, so a plain archive there is searched before the
# objects that need it exist and contributes nothing.
#
# This was a bare archive until 2026-09-24 and nothing noticed, because no title had linked
# libc++ - `cxx-throw` takes libc++abi and libunwind without it. `gl-cts` is the first, and the
# symptom was not a link error: `app.mk` passes `--unresolved-symbols=ignore-all` for a hosted
# title, so it produced a 32 MB binary with **315 undefined symbols**, `std::basic_string::append`
# and `std::locale::use_facet` among them. That runs until the first one is called.
#
# libc++abi and libunwind beside it have always been `--whole-archive`; this makes the third one
# match.
OOPS_LIBCXX_LDFLAGS := -Wl,--whole-archive $(OOPS_LIBCXX_LIB) -Wl,--no-whole-archive

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
#
# # `exception.cpp`, `stdexcept.cpp` and `typeinfo.cpp` stay, and libc++abi's copies go instead
#
# libc++abi ships `stdlib_exception.cpp`, `stdlib_stdexcept.cpp` and `stdlib_typeinfo.cpp` for
# the case where it has to work *without* libc++. With both linked they collide with these, and
# `oops-libcxxabi.mk` excludes that family - see the reasoning there, including why keeping the
# wrong side of the pair links cleanly and leaves every `std::runtime_error` constructor
# undefined.
#
# `new_handler.cpp` is the exception: libc++abi defines `std::set_new_handler` in
# `cxa_default_handlers.cpp`, which is not optional, so this one is genuinely absent here.
#
# Nothing noticed until a title linked **both** archives for the first time. `cxx-throw` links
# libc++abi without libc++, so the collision had no way to appear; the CTS is the first thing to
# want the pair, and it appeared as eight `duplicate symbol: std::...` errors at the link.
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
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/string.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/strstream.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/stdexcept.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/system_error.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/typeinfo.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/valarray.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/variant.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/vector.cpp \
    $(OOPS_LIBCXX_UPSTREAM)/libcxx/src/verbose_abort.cpp

# `rune_table.c` is the freestanding build's answer to `_DefaultRuneLocale`, which
# `std::ctype<char>::classic_table()` returns. **The hosted build must not have it**: the Mesa
# sysroot's `librune.a` defines the same symbol, from FreeBSD's own `locale/table.c`, and two
# definitions is a duplicate-symbol link error - the good failure, but an avoidable one.
#
# `locale_shim.cpp` is here for the same reason and under the same guard: it defines the
# `<locale.h>` and `<nl_types.h>` entry points that `include/freestanding/` has declared all
# along, and the Mesa sysroot the hosted build links carries FreeBSD's real ones.
#
# Those headers each said "the definitions are in `locale_shim.cpp`" while no such file existed.
# A payload link ignores unresolved symbols, so nothing said so until a title reached its link
# and named the six libc++ references - `newlocale`, `freelocale`, `uselocale`, `catopen`,
# `catgets`, `catclose`.
ifneq ($(OOPS_LIBCXX_HOSTED),1)
OOPS_LIBCXX_SRCS += $(OOPS_LIBCXX_DIR)/src/rune_table.c \
                    $(OOPS_LIBCXX_DIR)/src/locale_shim.cpp
endif

# `_LIBCPP_BUILDING_LIBRARY` is what libc++'s own sources are compiled with; without it they
# build as a consumer would and the out-of-line instantiations never get emitted.
ifeq ($(OOPS_LIBCXX_HOSTED),1)
# `-fexceptions -frtti`, where the freestanding build has neither.
#
# The hosted consumer is the GL CTS, and exceptions are how dEQP reports anything but success -
# `tcu::Exception` derives from `std::runtime_error` and `TCU_THROW`/`TCU_FAIL`/`TCU_CHECK` all
# throw. With `-fno-exceptions` libc++'s internal `_LIBCPP_THROW` becomes an abort, so
# `std::vector::at` out of range would kill the run instead of raising something the framework
# catches. RTTI comes with it: a `catch` by base class is a run-time type walk.
#
# Not `-ffreestanding`, because this one is not. It is compiled against a C library that exists.
#
# # `_POSIX_C_SOURCE=200809L`, which is about visibility rather than about POSIX
#
# FreeBSD's `<unistd.h>` declares `fflagstostr(u_long)`, `select(..., fd_set *, ...)` and their
# kin behind `__BSD_VISIBLE`, and **does not include `<sys/types.h>` itself** - a BSD-visible
# consumer is expected to have included it first. libc++'s `chrono.cpp`, `thread.cpp`,
# `atomic.cpp` and `print.cpp` include `<unistd.h>` on its own, so those declarations arrive with
# no `u_long` and no `fd_set` in scope and four sources stop there.
#
# Defining `_POSIX_C_SOURCE` sets `__BSD_VISIBLE` to 0, so the declarations are not made at all -
# which is the right answer rather than a workaround, because nothing here calls them.
# Force-including `<sys/types.h>` was the other candidate and does *not* work: the BSD block is
# gated on more than the typedefs being present.
#
# `200809L` rather than something older because dEQP asks for at least `199309L` by name -
# `deThreadUnix.c` stops with "You are using too old posix API!" otherwise - and 2008 is the
# newest the staged sysroot answers to.

# # `LIBCXX_BUILDING_LIBCXXABI`: how libc++ is told which ABI library it has
#
# libc++ and libc++abi both know how to define `std::terminate`, `std::set_terminate`,
# `std::unexpected`, `std::type_info::~type_info` and the `std::logic_error` family. Which side
# provides them is not a choice made at link time - it is a **compile-time** decision, and this
# is the switch: with it set, `exception.cpp` includes `<cxxabi.h>`, `_LIBCPPABI_VERSION` becomes
# defined, and the `#elif defined(_LIBCPPABI_VERSION)` arms compile libc++'s copies away.
#
# Without it, the two libraries genuinely both define them and the link stops on a dozen
# duplicate symbols. Deleting sources from the list below to make that go away is the wrong fix
# and was tried first: the pairs are not interchangeable - libc++abi's `stdlib_stdexcept.cpp`
# has the *destructors* and libc++'s `stdexcept.cpp` has the *constructors* - so dropping the
# libc++ side cleared the errors and left every `std::runtime_error(const char *)` undefined,
# which `--unresolved-symbols=ignore-all` linked without a word.
#
# The include path is needed as well as the define: the `#include <cxxabi.h>` it unlocks has to
# find libc++abi's header.
OOPS_LIBCXXABI_UPSTREAM ?= $(OOPS_LIBCXX_DIR)/upstream
OOPS_LIBCXX_ABI_FLAGS := -DLIBCXX_BUILDING_LIBCXXABI \
                         -I$(OOPS_LIBCXXABI_UPSTREAM)/libcxxabi/include
OOPS_LIBCXX_CFLAGS = -target x86_64-unknown-freebsd --sysroot=$(OOPS_MESA_SYSROOT) \
                     -D_POSIX_C_SOURCE=200809L \
                     -fexceptions -frtti -fPIC -std=c++20 -O2 -w \
                     -D_LIBCPP_BUILDING_LIBRARY -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS \
                     $(OOPS_LIBCXX_ABI_FLAGS) \
                     $(OOPS_LIBCXX_INCLUDE) $(OOPS_SDK_INCLUDE)
else
OOPS_LIBCXX_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                     -fno-exceptions -fno-rtti -fPIC -std=c++20 -O2 -w \
                     -D_LIBCPP_BUILDING_LIBRARY -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS \
                     $(OOPS_LIBCXX_ABI_FLAGS) \
                     $(OOPS_LIBCXX_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)
endif

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
