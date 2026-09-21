# C++ for a ported title. Include from a title's Makefile, before `common/app.mk`:
#
#   include $(OOPS_APPS_ROOT)/common/cxx.mk
#
#   OOPS_CXX_SRCS        += $(wildcard upstream/*.cpp) shim/etr_shim.cpp
#   EXTRA_TARGET_CFLAGS  += $(OOPS_CXX_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_CXX_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_CXX_LIB)
#
# # Why an archive rather than sources in PAYLOAD_SRCS
#
# `app.mk` compiles every payload source in one command with one set of flags, and those flags
# are C: `-std=c11 -Wstrict-prototypes -Wmissing-prototypes`. None of that is meaningful for C++
# and `-std=c11` is actively wrong for it. Splitting the flags per source would mean teaching
# `app.mk` about languages; building the C++ side into an archive means it already knows enough.
#
# It is also the route `oops-sdl.mk` and `oops-sdl12.mk` take for the same reason one layer over:
# somebody else's code, compiled under flags that suit it.
#
# # What -fno-exceptions -fno-rtti is doing here
#
# It is not a house style. It is what the C++ titles in the queue actually need, and it was
# measured before it was chosen: **Extreme Tux Racer has 0 `throw` and 0 `dynamic_cast`.**
# Armagetron has 68 and 167, plus boost, and is explicitly *not* covered by this file - see
# `src/oops-titles/README.md`, which moved it to last for exactly this reason.
#
# A title that needs exceptions does not quietly get a runtime that half-supports them: with
# these flags the compiler refuses `throw` outright, at the line that wrote it.

ifndef OOPS_CXX_MK_DIR
OOPS_CXX_MK_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif

OOPS_CXX_BUILD ?= build/cxx
OOPS_CXX_LIB   := $(OOPS_CXX_BUILD)/libcxxtitle.a

TARGET_CXX ?= clang++

# `cxxrt.cpp` is always in. It is the `new`, `delete`, static-init guards and `__cxa_*` that any
# C++ program needs from below the standard library, and a title that forgot it would link
# cleanly - `--unresolved-symbols=ignore-all` - and fault on its first allocation.
OOPS_CXX_RT_SRC := $(OOPS_CXX_MK_DIR)/cxxrt.cpp

# `-nostdinc++` is deliberate and load-bearing. Without it the build machine's libstdc++ headers
# are found, they are Linux and glibc-bound, and a title would compile against a standard library
# that cannot exist on the target. A C++ standard library is brought in the way every other
# dependency is - pinned - and named by the title, not picked up off the host.
OOPS_CXX_STD ?= c++11
OOPS_CXX_INCLUDE :=

OOPS_CXX_FLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                 -nostdinc++ -fno-exceptions -fno-rtti -fPIC -fno-stack-protector \
                 -std=$(OOPS_CXX_STD) -O2 -w \
                 $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE) \
                 $(OOPS_CXX_INCLUDE) $(EXTRA_TARGET_CFLAGS)

# `--whole-archive`, for the reason `oops-sdl.mk` gives: `app.mk` puts LDFLAGS before the sources
# on the link line, and a static archive seen before the objects that need it contributes nothing.
OOPS_CXX_LDFLAGS := -Wl,--whole-archive $(OOPS_CXX_LIB) -Wl,--no-whole-archive

$(OOPS_CXX_LIB): $(OOPS_CXX_SRCS) $(OOPS_CXX_RT_SRC) $(MAKEFILE_LIST)
	@mkdir -p $(OOPS_CXX_BUILD)
	@rm -f $@
	@n=0; for src in $(OOPS_CXX_RT_SRC) $(OOPS_CXX_SRCS); do \
	    n=$$((n+1)); \
	    $(TARGET_CXX) $(OOPS_CXX_FLAGS) -c -o $(OOPS_CXX_BUILD)/cxx$$n.o "$$src" || exit 1; \
	done; \
	echo "cxx: compiled $$n sources"
	@ar_tool=$$(command -v $(AR) 2>/dev/null || command -v llvm-ar 2>/dev/null || command -v ar); \
	 "$$ar_tool" rcs $@ $(OOPS_CXX_BUILD)/cxx*.o
	@echo "cxx: $@"

.PHONY: cxx-clean
cxx-clean:
	@rm -rf $(OOPS_CXX_BUILD)
	@echo "cxx: removed $(OOPS_CXX_BUILD)"
