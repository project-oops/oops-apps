# Which C library a dependency compiles against: included by each `src/oops-deps/*/oops-*.mk`.
#
# A title is one of two kinds (`common/app.mk`). A freestanding one compiles against oops-sdk's
# `include/libc` and `common/posix`, with `-nostdlibinc` so nothing reaches the build machine's
# headers. A hosted one - `OOPS_RENDERER = mesa`, which sets `USE_MESA` - compiles against the
# Mesa sysroot's FreeBSD C library, whose names resolve at load. A library compiled one way
# cannot link into a title of the other: the two disagree about `FILE`, `clock_t` and more, and
# the link would resolve every name and read the wrong bytes. `../src/oops-deps/libcxx` says the
# same at length for the C++ library.
#
# So each dependency builds into `build` or `build-hosted` by the including title's kind, with
# `OOPS_DEP_SYS` in place of the system half of its flags:
#
#   OOPS_ZLIB_BUILD ?= $(OOPS_ZLIB_DIR)/build$(OOPS_DEP_BUILD_SUFFIX)
#   OOPS_ZLIB_CFLAGS = -target x86_64-unknown-freebsd -nostdlib -fPIC -O2 -w \
#                      $(OOPS_ZLIB_INCLUDE) $(OOPS_DEP_SYS)
#
# A dependency whose freestanding flags are ordered differently - which header wins depends on
# the order - keeps its own list and hands it to `oops_dep_sys`, which returns it unchanged for a
# freestanding title and the sysroot for a hosted one:
#
#   OOPS_FT_CFLAGS = ... $(call oops_dep_sys,-ffreestanding -fno-builtin -nostdlibinc \
#                        $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE))
#
# A title sets `OOPS_RENDERER` before including any dependency, which every title here does. The
# sysroot variable is oops-mesa's (`oops-mesa.mk`, included by `app.mk`); `OOPS_DEP_SYS` is
# recursive, so it is read when a recipe runs, after that.
ifndef OOPS_DEP_SYS_MK
OOPS_DEP_SYS_MK := 1

OOPS_DEPS_HOSTED ?= $(if $(filter mesa,$(OOPS_RENDERER))$(filter 1,$(USE_MESA)),1,0)

ifeq ($(OOPS_DEPS_HOSTED),1)
OOPS_DEP_BUILD_SUFFIX := -hosted
OOPS_DEP_SYS = --sysroot=$(OOPS_MESA_SYSROOT) $(OOPS_SDK_INCLUDE)
else
OOPS_DEP_BUILD_SUFFIX :=
OOPS_DEP_SYS = -ffreestanding -fno-builtin -nostdlibinc $(OOPS_POSIX_INCLUDE) $(OOPS_SDK_INCLUDE) \
               $(OOPS_SDK_LIBC_INCLUDE)
endif

oops_dep_sys = $(if $(filter 1,$(OOPS_DEPS_HOSTED)),--sysroot=$(OOPS_MESA_SYSROOT) \
                   $(OOPS_SDK_INCLUDE),$(1))

endif
