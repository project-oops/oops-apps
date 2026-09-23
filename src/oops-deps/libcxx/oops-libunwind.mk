# libunwind: the DWARF unwinder a throwing title brings with it.
#
# Include from a title's Makefile, after `oops-libcxx.mk` and before `common/cxx.mk`:
#
#   include $(OOPS_LIBCXX)/oops-libunwind.mk
#   EXTRA_TARGET_LDFLAGS += $(OOPS_LIBUNWIND_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_LIBUNWIND_LIB)
#
# # Why a title needs its own
#
# The platform exports none of it. obSCEne swept all 19 Itanium unwind and C++ ABI symbols
# across `libkernel`, `libSceLibcInternal` and `self` and every one came back absent at `0x0`
# (REQ-20260921T0953Z-e3f7, sweep 20260921-run17). The platform runs an MSVC-lineage SEH
# unwinder, which is not the ABI clang emits for this target. So there is nothing to import and
# nothing to fall back to: a title that throws carries an unwinder or does not throw. (D005)
#
# The source is the checkout `oops-libcxx.mk` already pins - libunwind is a directory in the same
# llvm-project commit as libc++, so this costs a wider `UPSTREAM_SPARSE` and no second pin.
#
# # _LIBUNWIND_IS_BAREMETAL, which is the whole design decision in one flag
#
# libunwind does not carry a frame table; it finds one at run time. On an ordinary ELF platform
# it does that through `dl_iterate_phdr` (or `_dl_find_object`), and `AddressSpace.hpp` includes
# `dlfcn.h` and `link.h` to do it. **Neither compiles here**, and that is not an accident of the
# sysroot: `dl_iterate_phdr` is measured absent from `libSceLibcInternal`
# (`20260921-run17-eboot.obs.log:13132`), and whether any library answers it at all is the open
# question on the obSCEne bus as REQ-20260921T1830Z-b4d1.
#
# Baremetal mode is the route that needs no answer. Instead of asking a dynamic linker where the
# frame information is, libunwind reads four symbols the *link* provides:
#
#     __eh_frame_start  __eh_frame_end  __eh_frame_hdr_start  __eh_frame_hdr_end
#
# That turns a run-time question about the platform into a link-time fact about the title, which
# is the direction this collection prefers everywhere else. It is also why this is usable before
# the bus answers: if `dl_iterate_phdr` turns out to be available, the stock route becomes an
# option and this stays correct either way.
#
# `link/eh-frame.ld` supplies those four symbols. A title that links this without it fails at the
# link naming them, which is the loud failure rather than an unwinder that finds an empty table
# and reports every frame as the end of the stack.

ifndef OOPS_LIBUNWIND_DIR
OOPS_LIBUNWIND_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_LIBUNWIND_UPSTREAM ?= $(OOPS_LIBUNWIND_DIR)/upstream
OOPS_LIBUNWIND_BUILD    ?= $(OOPS_LIBUNWIND_DIR)/build-unwind
OOPS_LIBUNWIND_LIB      := $(OOPS_LIBUNWIND_BUILD)/libunwind.a
OOPS_LIBUNWIND_SRC      := $(OOPS_LIBUNWIND_UPSTREAM)/libunwind/src

# The link script that answers the four symbols above.
OOPS_LIBUNWIND_LDSCRIPT := $(OOPS_LIBUNWIND_DIR)/link/eh-frame.ld

OOPS_LIBUNWIND_INCLUDE := -I$(OOPS_LIBUNWIND_UPSTREAM)/libunwind/include

# `--whole-archive`: the personality routine and the `_Unwind_*` entry points are reached from
# exception tables rather than from calls, so a linker pulling objects on demand takes none of
# them. Same reasoning `cxx.mk` gives for its own archive, one layer down.
OOPS_LIBUNWIND_LDFLAGS := -Wl,--whole-archive $(OOPS_LIBUNWIND_LIB) -Wl,--no-whole-archive \
                          -Wl,-T,$(OOPS_LIBUNWIND_LDSCRIPT)

# Every source in the directory. Most of them gate themselves out on this target - the ARM EHABI,
# SjLj, wasm, SEH and AIX files each compile to an object of a few hundred bytes with nothing in
# it - and listing them all rather than the two with content means a future libunwind that moves
# a definition does not silently lose it.
OOPS_LIBUNWIND_C_SRCS   := $(wildcard $(OOPS_LIBUNWIND_SRC)/*.c)
OOPS_LIBUNWIND_CXX_SRCS := $(wildcard $(OOPS_LIBUNWIND_SRC)/*.cpp)
OOPS_LIBUNWIND_ASM_SRCS := $(wildcard $(OOPS_LIBUNWIND_SRC)/*.S)

# `-nostdlibinc` is not optional, and this cost an hour to rediscover after `oops-libcxx.mk` had
# already written it down: without it the build machine's /usr/include stays on the search path,
# glibc's `features-time64.h` is found for a FreeBSD freestanding target, and it fails on
# `bits/wordsize.h`. The only C library in play must be oops-sdk's.
#
# `-fno-exceptions` on the unwinder itself is not a contradiction. libunwind *implements*
# throwing; it does not throw.
#
# # `-fasynchronous-unwind-tables`, and why the unwinder is the one library that cannot skip it
#
# **Not throwing is not the same as not being unwound through.** `_Unwind_RaiseException` takes
# the context, then its very first act is to step *past its own frame* - upstream's comment in
# `unwind_phase1` is literally "skip over first which is `_Unwind_RaiseException`". That step is
# a DWARF step, so it needs an FDE for `_Unwind_RaiseException`. `_Unwind_Resume` is the same on
# the cleanup path.
#
# This target emits none by default: clang for `x86_64-unknown-freebsd` with these flags leaves
# asynchronous unwind tables off, which is why a pure C title here has no `.eh_frame` section at
# all. libc++abi is compiled *with* exceptions and so got tables anyway - and that asymmetry is
# what made this so hard to see. `__cxa_throw`, `__cxa_begin_catch` and `__gxx_personality_v0`
# were all covered by FDEs; `_Unwind_RaiseException` and `_Unwind_Resume` were not, and they are
# the two that matter first.
#
# The symptom was a `throw` reaching std::terminate with no handler consulted at all - including
# `catch (...)` - because the first `__unw_step` returned end-of-stack and phase 1 concluded
# there were no frames to search. Measured on hardware 2026-09-23, after a readable frame table
# had already ruled out the two earlier causes.
OOPS_LIBUNWIND_FLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                       -nostdlibinc -fPIC -fno-stack-protector -O2 -w \
                       -fasynchronous-unwind-tables \
                       -isystem $(OOPS_LIBCXX_DIR)/include \
                       -I$(OOPS_LIBCXX_DIR)/include/freestanding \
                       -isystem $(OOPS_SDK_DIR)/include/libc \
                       -I$(OOPS_SDK_DIR)/include \
                       $(OOPS_LIBUNWIND_INCLUDE) -I$(OOPS_LIBUNWIND_SRC) \
                       -D_LIBUNWIND_IS_BAREMETAL=1 \
                       -D_LIBUNWIND_HAS_NO_THREADS \
                       -D_LIBUNWIND_USE_DLADDR=0

OOPS_LIBUNWIND_CXXFLAGS = $(OOPS_LIBUNWIND_FLAGS) -nostdinc++ \
                          -I$(OOPS_LIBCXX_UPSTREAM)/libcxx/include \
                          -std=c++20 -fno-exceptions -fno-rtti

TARGET_CC  ?= clang
TARGET_CXX ?= clang++

# `ar` is handed the list rather than the directory - `common/deps.mk` says what the glob cost.
# The counter runs across both loops, so the C, assembly and C++ objects share one numbering and
# the list is built in the same order they are archived.
$(OOPS_LIBUNWIND_LIB): $(OOPS_LIBUNWIND_C_SRCS) $(OOPS_LIBUNWIND_CXX_SRCS) \
                       $(OOPS_LIBUNWIND_ASM_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_LIBUNWIND_BUILD)
	@rm -f $@
	@n=0; objs=""; \
	 for src in $(OOPS_LIBUNWIND_C_SRCS) $(OOPS_LIBUNWIND_ASM_SRCS); do \
	     n=$$((n+1)); o=$(OOPS_LIBUNWIND_BUILD)/unw$$n.o; \
	     $(TARGET_CC) $(OOPS_LIBUNWIND_FLAGS) -c -o "$$o" "$$src" || exit 1; objs="$$objs $$o"; \
	 done; \
	 for src in $(OOPS_LIBUNWIND_CXX_SRCS); do \
	     n=$$((n+1)); o=$(OOPS_LIBUNWIND_BUILD)/unw$$n.o; \
	     $(TARGET_CXX) $(OOPS_LIBUNWIND_CXXFLAGS) -c -o "$$o" "$$src" || exit 1; objs="$$objs $$o"; \
	 done; \
	 echo "libunwind: compiled $$n sources"; \
	 ar_tool=$$(command -v $(AR) 2>/dev/null || command -v llvm-ar 2>/dev/null || command -v ar); \
	 "$$ar_tool" rcs $@ $$objs
	@echo "libunwind: $@"

.PHONY: libunwind-clean
libunwind-clean:
	@rm -rf $(OOPS_LIBUNWIND_BUILD)
	@echo "libunwind: removed build-unwind/"
