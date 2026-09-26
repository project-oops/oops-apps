# C++ for a ported title, built into one archive under C++ flags. Include it from a title's
# Makefile before `common/app.mk`, with OOPS_CXX_SRCS set above the include:
#
#   OOPS_CXX_SRCS        := $(wildcard upstream/*.cpp) shim/etr_shim.cpp
#   include $(OOPS_APPS_ROOT)/common/cxx.mk
#   EXTRA_TARGET_CFLAGS  += $(OOPS_CXX_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_CXX_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_CXX_LIB)
#
# Exceptions and RTTI are off by default, so the compiler rejects `throw` at its line.

ifndef OOPS_CXX_MK_DIR
OOPS_CXX_MK_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif

# Header dependencies, so a change to an oops-sdk header rebuilds the archive.
include $(OOPS_CXX_MK_DIR)/deps.mk

# make expands a rule's prerequisites when it reads the rule, so sources named after this
# include would give an archive that never rebuilds. Flags may still be appended later.
ifeq ($(strip $(OOPS_CXX_SRCS)),)
$(error common/cxx.mk: OOPS_CXX_SRCS is empty. Set it *above* this include - the archive names \
        those files as prerequisites, and make reads a rule's prerequisites when it reads the \
        rule, so naming them afterwards gives an archive that never rebuilds when your own C++ \
        changes. If the sources come from `upstream/`, include common/upstream.mk first so that \
        the wildcard has something to match on a fresh checkout)
endif

OOPS_CXX_BUILD ?= build/cxx
OOPS_CXX_LIB   := $(OOPS_CXX_BUILD)/libcxxtitle.a

TARGET_CXX ?= clang++

# `new`, `delete`, static-init guards and `__cxa_*`. The link ignores unresolved symbols, so
# a title without it would link and fault on its first allocation.
OOPS_CXX_RT_SRC := $(OOPS_CXX_MK_DIR)/cxxrt.cpp

# `-nostdinc++` keeps the build machine's libstdc++ headers out; a title names a pinned
# standard library instead.
OOPS_CXX_STD ?= c++11
OOPS_CXX_INCLUDE :=

# OOPS_CXX_EXCEPTIONS = 1 turns on exceptions and RTTI. The platform exports no Itanium
# unwind or C++ ABI symbols, so the title also links libunwind and libc++abi
# (oops-apps#D005):
#
#   include $(OOPS_LIBCXX)/oops-libunwind.mk
#   include $(OOPS_LIBCXX)/oops-libcxxabi.mk
#   EXTRA_TARGET_LDFLAGS += $(OOPS_LIBUNWIND_LDFLAGS) $(OOPS_LIBCXXABI_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_LIBUNWIND_LIB) $(OOPS_LIBCXXABI_LIB)
#
# The link ignores unresolved symbols, so check the ELF, not the exit code:
#   nm <title>.elf | grep -E '__cxa_throw|_Unwind_RaiseException|__eh_frame_start'
ifeq ($(OOPS_CXX_EXCEPTIONS),1)
# `-DOOPS_CXX_EXCEPTIONS` makes `cxxrt.cpp` leave the symbols libc++abi defines to it.
OOPS_CXX_EH_FLAGS := -fexceptions -frtti -DOOPS_CXX_EXCEPTIONS=1
else
OOPS_CXX_EH_FLAGS := -fno-exceptions -fno-rtti
endif

# libc++'s include directory precedes the C library's: its <errno.h> and friends wrap the C
# headers with `#include_next`, and `<cerrno>` refuses to build without them.
# `$(OOPS_SDK_INCLUDE)` goes first; its `oops/...` headers share no names with either.
# **The same three identity defines `app.mk` gives the C half** (its `TARGET_CFLAGS`, line 215).
# A C++ entry-point shim wants `OOPS_APP_ID` for exactly what a C one wants it for - `oops_log_init`,
# the disk sink, and `/data/homebrew/<id>` - and until 2026-09-26 only C had it, so the C++ shim
# failed on `use of undeclared identifier 'OOPS_APP_ID'` with nothing saying where the name lives.
#
# They are safe here although `app.mk` has not been read yet: `OOPS_CXX_FLAGS` is recursive (`=`),
# so these expand when a compile runs, by which time `app.mk` has set all three.
OOPS_CXX_APP_DEFINES = -DOOPS_APP_ID=\"$(TITLE_ID)\" -DOOPS_APP_NAME=\"$(APP_NAME)\" \
                       -D'OOPS_APP_VERSION="$(BUILD_VERSION)"'

OOPS_CXX_FLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                 -nostdinc++ $(OOPS_CXX_EH_FLAGS) -fPIC -fno-stack-protector \
                 -std=$(OOPS_CXX_STD) -O2 -w \
                 $(OOPS_CXX_APP_DEFINES) \
                 $(OOPS_SDK_INCLUDE) \
                 $(OOPS_CXX_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE) \
                 $(EXTRA_TARGET_CFLAGS)

# `--whole-archive`: `app.mk` puts LDFLAGS before the objects that need the archive.
OOPS_CXX_LDFLAGS := -Wl,--whole-archive $(OOPS_CXX_LIB) -Wl,--no-whole-archive

# Objects are named after their sources and `ar` is given the list, so a removed source
# leaves nothing behind in the archive.
OOPS_CXX_OBJS := $(call oops_objs,$(OOPS_CXX_BUILD)/obj,$(OOPS_CXX_RT_SRC) $(OOPS_CXX_SRCS))
-include $(OOPS_CXX_OBJS:.o=.d)
$(call oops_obj_rules,$(OOPS_CXX_BUILD)/obj,TARGET_CXX,OOPS_CXX_FLAGS,$(OOPS_CXX_RT_SRC) $(OOPS_CXX_SRCS))

# OOPS_CXX_LINK_OBJECTS = 1 links the objects directly, for a title whose sources share
# file names (see `oops_ar_check` in `common/deps.mk`). The archive is whole-archive, so
# the link is the same; `OOPS_CXX_LIB` becomes the object list.
ifeq ($(OOPS_CXX_LINK_OBJECTS),1)
OOPS_CXX_LIB     := $(OOPS_CXX_OBJS)
OOPS_CXX_LDFLAGS := $(OOPS_CXX_OBJS)
else
$(call oops_ar_check,$(OOPS_CXX_OBJS))

# `app.mk` is included after this file, so it is not in `$(oops_makefiles)` here.
$(OOPS_CXX_LIB): $(OOPS_CXX_OBJS) $(oops_makefiles)
	@mkdir -p $(OOPS_CXX_BUILD)
	@rm -f $@
	@ar_tool=$$(command -v $(AR) 2>/dev/null || command -v llvm-ar 2>/dev/null || command -v ar); \
	 "$$ar_tool" rcs $@ $(OOPS_CXX_OBJS)
	@echo "cxx: $@ ($(words $(OOPS_CXX_OBJS)) objects)"
endif

.PHONY: cxx-clean
cxx-clean:
	@rm -rf $(OOPS_CXX_BUILD)
	@echo "cxx: removed $(OOPS_CXX_BUILD)"
