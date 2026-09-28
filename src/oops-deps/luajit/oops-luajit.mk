# LuaJIT build integration.
#
#   OOPS_LUAJIT ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/luajit)
#   include $(OOPS_LUAJIT)/oops-luajit.mk
#   EXTRA_TARGET_LDFLAGS += $(OOPS_LUAJIT_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_LUAJIT_LIB)
#
# LuaJIT generates its interpreter at build time: `minilua` runs DynASM over `vm_x64.dasc` to
# make `buildvm`, and `buildvm` writes the VM as assembly (`lj_vm.S`) and the library tables
# (`lj_*def.h`). So this does not transcribe a source list; it runs upstream's own `src/Makefile`
# as a cross build - `HOST_CC` for the two host tools, our target flags for the library - over a
# copy of the tree in `build/`, since that Makefile builds in place and the fetched tree stays
# untouched.
#
# The switches, all upstream's own:
#   LUAJIT_DISABLE_JIT       the interpreter only: no code is generated at run time, so nothing
#                            needs executable memory
#   LUAJIT_DISABLE_FFI       the FFI's callbacks are machine code written at run time, for the
#                            same reason; LÖVE's Lua uses the FFI only when `require("ffi")`
#                            succeeds, and falls back when it does not
#   LUAJIT_USE_SYSMALLOC     `malloc`, not LuaJIT's own `mmap`-based allocator
#   LUAJIT_NO_UNWIND         errors unwind through LuaJIT's own frames, not the C++ unwinder
#   LUAJIT_DISABLE_PROFILE   the sampling profiler is driven by `setitimer` and `SIGPROF`
ifndef OOPS_LUAJIT_MK
OOPS_LUAJIT_MK := 1

ifndef OOPS_LUAJIT_DIR
OOPS_LUAJIT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_LUAJIT_UPSTREAM ?= $(OOPS_LUAJIT_DIR)/upstream
OOPS_LUAJIT_BUILD ?= $(OOPS_LUAJIT_DIR)/build
OOPS_LUAJIT_INCLUDE := -I$(OOPS_LUAJIT_UPSTREAM)/src
OOPS_LUAJIT_LIB := $(OOPS_LUAJIT_BUILD)/src/libluajit.a
OOPS_LUAJIT_LDFLAGS := $(OOPS_LUAJIT_LIB)
OOPS_LUAJIT_XCFLAGS := -DLUAJIT_DISABLE_JIT -DLUAJIT_DISABLE_FFI -DLUAJIT_USE_SYSMALLOC \
                       -DLUAJIT_NO_UNWIND -DLUAJIT_DISABLE_PROFILE
OOPS_LUAJIT_TARGET = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                     -nostdlibinc -fPIC -O2 $(OOPS_POSIX_INCLUDE) $(OOPS_SDK_INCLUDE) \
                     $(OOPS_SDK_LIBC_INCLUDE)
# The host compiler for `minilua` and `buildvm`: the build machine's own clang.
OOPS_LUAJIT_HOST_CC ?= clang
# Upstream's Makefile is run through this rather than `$(MAKE)` written in the recipe. make
# executes a line naming `$(MAKE)` even under `-n`, while only printing the lines before it, and
# `bin/oops-apps dist` probes with `make -n dist`: the build ran into a directory the probe had
# not made, and the release failed with `build.log: Directory nonexistent`. Named through a
# variable, the line is an ordinary one.
OOPS_LUAJIT_MAKE := $(MAKE)

$(OOPS_LUAJIT_LIB): $(OOPS_LUAJIT_UPSTREAM)/.oops-upstream-stamp $(lastword $(MAKEFILE_LIST))
	@rm -rf $(OOPS_LUAJIT_BUILD)
	@mkdir -p $(OOPS_LUAJIT_BUILD)
	@cp -r $(OOPS_LUAJIT_UPSTREAM)/src $(OOPS_LUAJIT_UPSTREAM)/dynasm $(OOPS_LUAJIT_BUILD)/
	@$(OOPS_LUAJIT_MAKE) --no-print-directory -C $(OOPS_LUAJIT_BUILD)/src libluajit.a \
	    HOST_CC=$(OOPS_LUAJIT_HOST_CC) CC=clang TARGET_SYS=Other BUILDMODE=static \
	    TARGET_CFLAGS="$(OOPS_LUAJIT_TARGET)" XCFLAGS="$(OOPS_LUAJIT_XCFLAGS)" Q= E=@: \
	    > $(OOPS_LUAJIT_BUILD)/build.log 2>&1 \
	    || { tail -20 $(OOPS_LUAJIT_BUILD)/build.log; exit 1; }
	@echo "luajit: $@"

.PHONY: luajit-clean
luajit-clean:
	@rm -rf $(OOPS_LUAJIT_BUILD)

endif
