# cxx-throw - does a C++ exception survive a round trip on this platform?
#
#   make check      run the five checks against the build machine's own C++ runtime, then
#                   verify the target ELF really carries an unwinder (any machine)
#   make elf        the plain-ELF payload a homebrew loader runs on the console
#   make dist       stages conforming release artifact under dist/
#
# Configuration & release formats loaded from app.env
#
# # What this probe is for
#
# The platform exports **none** of the 19 Itanium unwind and C++ ABI symbols - obSCEne swept
# `libkernel`, `libSceLibcInternal` and `self` and every one came back absent at `0x0`
# (REQ-20260921T0953Z-e3f7). It runs an MSVC-lineage SEH unwinder, which is not the ABI clang
# emits. So a throwing title carries its own libunwind and libc++abi, and this is the smallest
# thing that says whether that works. (oops-apps#D005)

OOPS_APPS_ROOT ?= $(abspath ../../..)
OOPS_LIBCXX    ?= $(abspath ../../oops-deps/libcxx)

# The C++ side, built into an archive by `cxx.mk` - see its header for why an archive and not
# sources in PAYLOAD_SRCS.
OOPS_CXX_EXCEPTIONS = 1
OOPS_CXX_SRCS       = boundary.cpp probe.cpp

include $(OOPS_LIBCXX)/oops-libcxx.mk
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk
include $(OOPS_APPS_ROOT)/common/cxx.mk

OOPS_CXX_INCLUDE     += $(OOPS_LIBCXX_INCLUDE)
EXTRA_TARGET_LDFLAGS += $(OOPS_CXX_LDFLAGS) $(OOPS_LIBCXXABI_LDFLAGS) $(OOPS_LIBUNWIND_LDFLAGS)

# **`OOPS_CXX_INCLUDE` deliberately does NOT go into `EXTRA_TARGET_CFLAGS`**, although `cxx.mk`'s
# own header shows that line. It is right for a title whose C sources include C++-adjacent
# headers and wrong here, and getting it wrong is loud in a confusing way: libc++'s include
# directory carries its own `<stddef.h>`, so putting it on the C payload compile makes
# oops-sdk's `<stddef.h>` resolve to libc++'s, and every SDK C source then fails inside
# `__config`. This app's C side includes no C++ headers, so it wants none of them.
PAYLOAD_EXTRA_DEPS   += $(OOPS_CXX_LIB) $(OOPS_LIBCXXABI_LIB) $(OOPS_LIBUNWIND_LIB)

# The link order matters and is not alphabetical: libc++abi calls into libunwind, so the
# unwinder comes last. Both are `--whole-archive` (their own fragments say why): the personality
# routine and the `_Unwind_*` entry points are reached from exception tables rather than from
# calls, so on-demand extraction takes neither.

# Target payload sources. No display, no input: the question is answered by a number on klog.
PAYLOAD_SRCS = cxx_throw_main.c \
               $(OOPS_SDK_DIR)/src/system/system.c \
               $(OOPS_SDK_DIR)/src/system/freestd.c \
               $(OOPS_SDK_DIR)/src/system/syscall.c \
               $(OOPS_SDK_DIR)/src/memory/memory.c \
               $(OOPS_SDK_DIR)/src/memory/heap.c \
               $(OOPS_SDK_DIR)/src/time/time.c

ENTRY_POINT := cxx_throw_start

include $(OOPS_APPS_ROOT)/common/app.mk

# # The host check, and why it is built by hand here
#
# `app.mk`'s `HOST_TEST_SRCS` compiles with `$(CC) -std=c11`, which cannot build a C++ probe.
# Rather than teach `app.mk` about a second language for one app, this app builds its own host
# binary with `clang++` against the *build machine's* C++ runtime.
#
# **That is the point of it, not a compromise.** obSCEne's fifth rule for adding a check is to
# run it against a known-good implementation before believing anything it says, and this is
# that: if a check fails here, the check is wrong, because the build machine's libstdc++
# unwinds correctly. It says nothing about the console and is not meant to.
HOST_PROBE := $(BUILD)/cxx-throw_hostprobe

$(HOST_PROBE): probe.cpp boundary.cpp cxx_throw.h | $(BUILD)
	@clang++ -std=c++17 -O1 -Wall -Wextra -o $@ \
	    host_main.cpp probe.cpp boundary.cpp
	@echo "cxx-throw: host probe built"

.PHONY: check-host check-target-symbols
check-host: $(HOST_PROBE)
	@$(HOST_PROBE)

# # The check that actually matters, and why a clean link is not it
#
# `app.mk` links with `-Wl,--unresolved-symbols=ignore-all`. So a payload that failed to link an
# unwinder **still links**, and every one of these symbols resolves to zero at run time. The
# build succeeding is therefore no evidence at all; the symbol table is.
#
# This is the same discipline oops-sdk records for cross-module calls, arrived at the same way:
# a clean exit code proving nothing.
check-target-symbols: $(BUILD)/$(APP_NAME).elf
	@missing=0; \
	for s in __cxa_throw __cxa_begin_catch __cxa_allocate_exception __gxx_personality_v0 \
	         _Unwind_RaiseException _Unwind_Resume \
	         __eh_frame_start __eh_frame_end; do \
	    if nm $(BUILD)/$(APP_NAME).elf 2>/dev/null | grep -qE " [TtRrDd] $$s$$"; then \
	        echo "  ok       $$s"; \
	    else \
	        echo "  MISSING  $$s"; missing=$$((missing+1)); \
	    fi; \
	done; \
	if [ "$$missing" -ne 0 ]; then \
	    echo "cxx-throw: $$missing unwinder symbol(s) absent - the payload would fault on its first throw" >&2; \
	    exit 1; \
	fi; \
	echo "cxx-throw: the payload carries its own unwinder"

check: check-host check-target-symbols
