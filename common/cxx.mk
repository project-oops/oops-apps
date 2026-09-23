# C++ for a ported title. Include from a title's Makefile, before `common/app.mk`:
#
#   include $(OOPS_APPS_ROOT)/common/upstream.mk        # only if the sources are fetched
#
#   OOPS_CXX_SRCS        := $(wildcard upstream/*.cpp) shim/etr_shim.cpp
#   include $(OOPS_APPS_ROOT)/common/cxx.mk
#
#   EXTRA_TARGET_CFLAGS  += $(OOPS_CXX_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_CXX_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_CXX_LIB)
#
# **`OOPS_CXX_SRCS` goes above the include, and that is checked below.** This file names the
# sources as prerequisites of the archive, and make expands a rule's prerequisites when it
# *reads* the rule - so a title that set them afterwards got an archive with no prerequisites at
# all. It then rebuilt only when a makefile changed, and a title's own C++ could be edited all
# day without the archive noticing. Extreme Tux Racer did exactly that until 2026-09-23, and
# these lines used to show it: the example above had the include first.
#
# Flags are different and may still be appended afterwards - `OOPS_CXX_INCLUDE` and
# `EXTRA_TARGET_CFLAGS` reach the compiler through `OOPS_CXX_FLAGS`, which is a recursive
# variable read when the recipe runs. It is the *file list* that has to be settled first.
#
# # Why an archive rather than sources in PAYLOAD_SRCS
#
# `app.mk` compiles every payload source under **one** set of flags, and those flags are C:
# `-std=c11 -Wstrict-prototypes -Wmissing-prototypes`. None of that is meaningful for C++ and
# `-std=c11` is actively wrong for it. That each source now gets its own `clang` invocation
# (2026-09-23) does not change this: the flags are still one list, and varying them per source
# would mean teaching `app.mk` which language a file is. Building the C++ side into an archive
# means it already knows enough.
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

# The archive's headers are prerequisites too, for the reason `common/deps.mk` gives: a title
# whose C++ reaches an `oops-sdk` header would otherwise not rebuild when that header changed,
# and the ELF beside it would relink against the *previous* archive without a word.
include $(OOPS_CXX_MK_DIR)/deps.mk

# The ordering contract from the header of this file, enforced rather than described. An empty
# list here means either "included before the sources were named" - the silent, expensive case -
# or a title that included this file for no reason. Both are worth stopping at.
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

# # Exceptions and RTTI: off by default, and opted into per title
#
#   OOPS_CXX_EXCEPTIONS = 1
#
# **Off is still the right default** and the paragraph above is unchanged: the titles in the
# queue were counted, Extreme Tux Racer throws nothing, and a title that does not need this
# should not carry an unwinder. What has changed is that "on" is now reachable at all.
#
# Turning it on is not just two compiler flags. The platform exports **none** of the 19 Itanium
# unwind and C++ ABI symbols - obSCEne swept `libkernel`, `libSceLibcInternal` and `self` and
# found every one absent at `0x0` (REQ-20260921T0953Z-e3f7). So a throwing title must also link
# libunwind and libc++abi, built from the same pinned llvm-project checkout as libc++, and a
# linker script that tells the unwinder where its frame table is:
#
#   OOPS_CXX_EXCEPTIONS  = 1
#   include $(OOPS_LIBCXX)/oops-libunwind.mk
#   include $(OOPS_LIBCXX)/oops-libcxxabi.mk
#   EXTRA_TARGET_LDFLAGS += $(OOPS_LIBUNWIND_LDFLAGS) $(OOPS_LIBCXXABI_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_LIBUNWIND_LIB) $(OOPS_LIBCXXABI_LIB)
#
# `oops-apps#D005` is why those three pieces and not a different three.
#
# **This file cannot check that you did the rest**, and that is worth stating where the flag is:
# `app.mk` links with `--unresolved-symbols=ignore-all`, so a title that sets
# `OOPS_CXX_EXCEPTIONS` and forgets the archives links perfectly cleanly and faults on its first
# `throw`. Verify the built ELF rather than the exit code:
#
#   nm <title>.elf | grep -E '__cxa_throw|_Unwind_RaiseException|__eh_frame_start'
ifeq ($(OOPS_CXX_EXCEPTIONS),1)
# `-DOOPS_CXX_EXCEPTIONS` is not decoration: `cxxrt.cpp` uses it to *stop* defining the four
# symbols libc++abi defines properly (`__cxa_pure_virtual`, `__cxa_guard_*`). Without it the two
# archives collide at the link. See the block comment in `cxxrt.cpp`.
OOPS_CXX_EH_FLAGS := -fexceptions -frtti -DOOPS_CXX_EXCEPTIONS=1
else
OOPS_CXX_EH_FLAGS := -fno-exceptions -fno-rtti
endif

OOPS_CXX_FLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                 -nostdinc++ $(OOPS_CXX_EH_FLAGS) -fPIC -fno-stack-protector \
                 -std=$(OOPS_CXX_STD) -O2 -w \
                 $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE) \
                 $(OOPS_CXX_INCLUDE) $(EXTRA_TARGET_CFLAGS)

# `--whole-archive`, for the reason `oops-sdl.mk` gives: `app.mk` puts LDFLAGS before the sources
# on the link line, and a static archive seen before the objects that need it contributes nothing.
OOPS_CXX_LDFLAGS := -Wl,--whole-archive $(OOPS_CXX_LIB) -Wl,--no-whole-archive

# **The objects are named, not globbed.** They used to be `cxx1.o`, `cxx2.o`, ... in the order
# the recipe happened to visit the sources, and `ar` collected them with `cxx*.o`. Two things
# follow from that and neither is good. Numbering by position means *removing* a source renumbers
# every object after it, so all of them recompile; and the glob then archives whatever is in the
# directory, including the highest-numbered object left over from when the list was longer. A
# title that dropped a file kept its code - the symbols stayed in the archive, the link stayed
# clean, and the only evidence was an object file nothing had written that run.
#
# `common/deps.mk` names an object after its source, so a removed source leaves an object that
# nothing lists and `ar` is handed the list rather than the directory.
OOPS_CXX_OBJS := $(call oops_objs,$(OOPS_CXX_BUILD)/obj,$(OOPS_CXX_RT_SRC) $(OOPS_CXX_SRCS))
$(call oops_ar_check,$(OOPS_CXX_OBJS))
-include $(OOPS_CXX_OBJS:.o=.d)
$(call oops_obj_rules,$(OOPS_CXX_BUILD)/obj,TARGET_CXX,OOPS_CXX_FLAGS,$(OOPS_CXX_RT_SRC) $(OOPS_CXX_SRCS))

# `$(oops_makefiles)` is this file's view of them: `app.mk` is included by the title *after* this
# one, so it is not in the list yet and a change to it does not rebuild the archive. The flags
# that matter here come from this file and the title's own Makefile, both of which are.
$(OOPS_CXX_LIB): $(OOPS_CXX_OBJS) $(oops_makefiles)
	@mkdir -p $(OOPS_CXX_BUILD)
	@rm -f $@
	@ar_tool=$$(command -v $(AR) 2>/dev/null || command -v llvm-ar 2>/dev/null || command -v ar); \
	 "$$ar_tool" rcs $@ $(OOPS_CXX_OBJS)
	@echo "cxx: $@ ($(words $(OOPS_CXX_OBJS)) objects)"

.PHONY: cxx-clean
cxx-clean:
	@rm -rf $(OOPS_CXX_BUILD)
	@echo "cxx: removed $(OOPS_CXX_BUILD)"
