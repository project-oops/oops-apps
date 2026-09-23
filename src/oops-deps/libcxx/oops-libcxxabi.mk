# libc++abi: the Itanium C++ ABI a throwing title brings with it.
#
# Include from a title's Makefile, after `oops-libcxx.mk` and `oops-libunwind.mk`:
#
#   include $(OOPS_LIBCXX)/oops-libcxxabi.mk
#   EXTRA_TARGET_LDFLAGS += $(OOPS_LIBCXXABI_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_LIBCXXABI_LIB)
#
# # What this provides that the platform does not
#
# `__cxa_throw`, `__cxa_begin_catch`, `__cxa_end_catch`, `__cxa_allocate_exception` and
# `__gxx_personality_v0` - five of the nineteen symbols obSCEne measured absent at `0x0` across
# every system library (REQ-20260921T0953Z-e3f7). libunwind supplies the other fourteen. Between
# them the set is complete, which is what makes `-fexceptions` a real option here rather than a
# flag that compiles and then faults. (D005)
#
# Same pinned checkout as libc++ and libunwind: one llvm-project commit, three directories.
#
# # Three sources are deliberately not built
#
# `cxa_noexception.cpp` - **upstream builds this one *or* `cxa_exception.cpp`, never both.** Its
# CMake picks between them on `LIBCXXABI_ENABLE_EXCEPTIONS`, and a `wildcard` over the directory
# takes both and gets a duplicate-symbol link error on
# `__cxa_increment_exception_refcount`/`__cxa_decrement_exception_refcount`. This archive is the
# exceptions build by definition - it exists to make `throw` work - so the real one is
# `cxa_exception.cpp` and the stub is excluded.
#
# `stdlib_new_delete.cpp` - `common/cxxrt.cpp` already defines `operator new` and `operator
# delete` over the SDK heap, and it has to, because it is what a non-throwing C++ title uses too.
# Building both would be a duplicate-symbol link error, and taking libc++abi's instead would
# route a title's allocations away from the SDK heap.
#
# `cxa_thread_atexit.cpp` - registers destructors for `thread_local` objects, through a TLS key
# API (`__libcpp_tls_key`) the freestanding libc does not have and `_LIBCPP_HAS_THREADS 0` says
# it will not get. A title that declares a `thread_local` with a destructor gets a link error
# naming `__cxa_thread_atexit`, which is the honest failure: the destructor genuinely would not
# have run.
#
# All three are exclusions with a reason, not a list of what happened to compile. Note that the
# first was invisible until something *linked* the archive: all nineteen sources compile
# perfectly well on their own, and compiling is not the test.

ifndef OOPS_LIBCXXABI_DIR
OOPS_LIBCXXABI_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_LIBCXXABI_UPSTREAM ?= $(OOPS_LIBCXXABI_DIR)/upstream
OOPS_LIBCXXABI_BUILD    ?= $(OOPS_LIBCXXABI_DIR)/build-cxxabi
OOPS_LIBCXXABI_LIB      := $(OOPS_LIBCXXABI_BUILD)/libc++abi.a
OOPS_LIBCXXABI_SRCDIR   := $(OOPS_LIBCXXABI_UPSTREAM)/libcxxabi/src

OOPS_LIBCXXABI_EXCLUDE := cxa_noexception.cpp stdlib_new_delete.cpp cxa_thread_atexit.cpp
OOPS_LIBCXXABI_SRCS := $(filter-out $(addprefix $(OOPS_LIBCXXABI_SRCDIR)/,$(OOPS_LIBCXXABI_EXCLUDE)), \
                                    $(wildcard $(OOPS_LIBCXXABI_SRCDIR)/*.cpp))

# `--whole-archive`: the personality routine is found through the exception table, never called,
# so on-demand archive extraction leaves it out and every throw lands in `std::terminate`.
OOPS_LIBCXXABI_LDFLAGS := -Wl,--whole-archive $(OOPS_LIBCXXABI_LIB) -Wl,--no-whole-archive

# `-isystem` for the C library, not `-I`, and the order matters. libc++'s `<cstdlib>` reaches the
# C library with `#include_next <stdlib.h>`, which walks the *system* include chain - so the C
# library must be on it, and this dependency's own `include/` must come first so its `stdlib.h`
# (which adds `aligned_alloc`) is found and pulls oops-sdk's in behind it. With the C library on
# plain `-I` instead, seventeen of libc++abi's nineteen sources fail with
# `<cstdlib> tried including <stdlib.h>`, which names neither the cause nor the fix.
#
# `-fexceptions -frtti`, obviously: this is the library that implements both.
OOPS_LIBCXXABI_FLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                       -nostdlibinc -fPIC -fno-stack-protector -O2 -w -std=c++20 \
                       -fexceptions -frtti \
                       -isystem $(OOPS_LIBCXX_DIR)/include \
                       -I$(OOPS_LIBCXX_DIR)/include/freestanding \
                       -isystem $(OOPS_SDK_DIR)/include/libc \
                       -I$(OOPS_SDK_DIR)/include \
                       -nostdinc++ \
                       -I$(OOPS_LIBCXX_UPSTREAM)/libcxx/include \
                       -I$(OOPS_LIBCXX_UPSTREAM)/libcxx/src \
                       -I$(OOPS_LIBCXXABI_UPSTREAM)/libcxxabi/include \
                       -I$(OOPS_LIBCXXABI_UPSTREAM)/libcxxabi/src \
                       -I$(OOPS_LIBCXXABI_UPSTREAM)/libunwind/include \
                       -D_LIBCXXABI_BUILDING_LIBRARY -D_LIBCPP_BUILDING_LIBRARY \
                       -D_LIBCXXABI_HAS_NO_THREADS -D_LIBCPP_HAS_NO_THREADS

TARGET_CXX ?= clang++

# `ar` is handed the list rather than the directory - `common/deps.mk` says what the glob cost.
$(OOPS_LIBCXXABI_LIB): $(OOPS_LIBCXXABI_SRCS) $(lastword $(MAKEFILE_LIST)) \
                       $(OOPS_LIBCXX_DIR)/include/__config_site
	@mkdir -p $(OOPS_LIBCXXABI_BUILD)
	@rm -f $@
	@n=0; objs=""; for src in $(OOPS_LIBCXXABI_SRCS); do \
	    n=$$((n+1)); o=$(OOPS_LIBCXXABI_BUILD)/abi$$n.o; \
	    $(TARGET_CXX) $(OOPS_LIBCXXABI_FLAGS) -c -o "$$o" "$$src" || exit 1; objs="$$objs $$o"; \
	done; \
	echo "libc++abi: compiled $$n sources"; \
	ar_tool=$$(command -v $(AR) 2>/dev/null || command -v llvm-ar 2>/dev/null || command -v ar); \
	"$$ar_tool" rcs $@ $$objs
	@echo "libc++abi: $@"

.PHONY: libcxxabi-clean
libcxxabi-clean:
	@rm -rf $(OOPS_LIBCXXABI_BUILD)
	@echo "libc++abi: removed build-cxxabi/"
