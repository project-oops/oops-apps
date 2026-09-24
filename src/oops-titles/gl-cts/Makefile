# The OpenGL CTS, built for the console.
#
# `upstream.lock` says what is fetched and why; `README.md` says where the port stands. This file
# builds the **framework** - dEQP's own machinery - plus the platform layer in `shim/`, which is
# the only code this port writes.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# The fetch has to happen while this file is being read, because the source lists below wildcard
# directories that do not exist in a fresh checkout. `common/upstream.mk` explains the ordering
# trap in full.
include $(OOPS_APPS_ROOT)/common/upstream.mk

OOPS_RENDERER = mesa

# # Hosted, and everything downstream follows from it
#
# `OOPS_RENDERER = mesa` sets `USE_MESA`, which makes `app.mk` empty `OOPS_SDK_LIBC_INCLUDE` and
# take the C library from the Mesa sysroot instead. The C++ library has to match: a hosted title
# links `OOPS_LIBCXX_HOSTED = 1`'s archives, which are built against that same sysroot. Linking
# the freestanding ones would resolve every symbol and read the wrong bytes - they disagree about
# `FILE` and `clock_t`. `oops-deps/libcxx/oops-libcxx.mk` sets that out at length.
OOPS_LIBCXX_HOSTED = 1

OOPS_LIBCXX ?= $(abspath ../../oops-deps/libcxx)
OOPS_MESA   ?= $(abspath $(OOPS_APPS_ROOT)/../oops-mesa)

UP := upstream/framework

# # The build order is the dependency order, and it is upstream's
#
# `delibs` is the bottom: C base libraries with no C++ and no knowledge of GL. `qphelper` writes
# the `.qpa` result log. `common` is `tcu`, the test framework proper. `opengl` is `glu`, the GL
# object wrappers, with `wrapper/` holding the generated entry-point table. `referencerenderer`
# is `rr`, a software rasteriser the image-comparison tests draw the expected answer with.
#
# Each list is a wildcard rather than an enumeration, because upstream owns the file set and a
# file appearing in a version bump should arrive rather than be missed. The exclusions below are
# the exception, and each says why.
CTS_C_ALL   := $(wildcard $(UP)/delibs/debase/*.c) \
               $(wildcard $(UP)/delibs/depool/*.c) \
               $(wildcard $(UP)/delibs/deutil/*.c) \
               $(wildcard $(UP)/delibs/dethread/*.c) \
               $(wildcard $(UP)/delibs/dethread/unix/*.c) \
               $(wildcard $(UP)/delibs/deimage/*.c) \
               $(wildcard $(UP)/qphelper/*.c)

CTS_CXX_ALL := $(wildcard $(UP)/delibs/decpp/*.cpp) \
               $(wildcard $(UP)/common/*.cpp) \
               $(wildcard $(UP)/opengl/*.cpp) \
               $(wildcard $(UP)/opengl/wrapper/*.cpp) \
               $(wildcard $(UP)/opengl/simplereference/*.cpp) \
               $(wildcard $(UP)/referencerenderer/*.cpp) \
               $(wildcard $(UP)/xexml/*.cpp) \
               $(UP)/platform/tcuMain.cpp

# # What is left out, and why each one
#
# **`standalone_test.c` and `rsgTest.cpp`** are dEQP's own self-test programs and each has its
# own `main()`. `tcuMain.cpp` is the real entry point; keeping all three is a duplicate symbol.
#
# **`tcuImageIO.cpp`** needs libpng, which is not ported. It reads and writes reference images,
# so an image-comparison test that wants one will not find it - but nothing else stops, and
# porting libpng for it is a separate piece of work.
#
# **`deSocket.c`** needs `<netinet6/in6.h>`, absent from the staged sysroot. It exists so a host
# can drive tests over a wire; a title running its own cases needs none of it.
CTS_EXCLUDE := $(UP)/delibs/dethread/standalone_test.c \
               $(UP)/randomshaders/rsgTest.cpp \
               $(UP)/common/tcuImageIO.cpp \
               $(UP)/delibs/deutil/deSocket.c

CTS_C_SRCS   := $(filter-out $(CTS_EXCLUDE),$(CTS_C_ALL))
CTS_CXX_SRCS := $(filter-out $(CTS_EXCLUDE),$(CTS_CXX_ALL))

CTS_INCLUDES := \
    -I$(UP)/delibs/debase -I$(UP)/delibs/depool -I$(UP)/delibs/deutil \
    -I$(UP)/delibs/dethread -I$(UP)/delibs/decpp -I$(UP)/delibs/deimage \
    -I$(UP)/qphelper -I$(UP)/common -I$(UP)/opengl -I$(UP)/referencerenderer \
    -I$(UP)/egl -I$(UP)/opengl/wrapper -I$(UP)/egl/wrapper -I$(UP)/xexml \
    -Ishim/include

# # `<fenv.h>` comes from msun's source tree, because the sysroot has none
#
# `deMath.c` sets the rounding mode through `fegetround`/`fesetround` to compute reference values
# for the GLSL rounding tests, so it is not optional and a wrong one would produce a wrong
# reference rather than a failure. FreeBSD keeps the header per-architecture and amd64 uses
# `msun/x86/fenv.h`, which oops-mesa already stages from the same pinned checkout as `libm.a`.
#
# **This reaches past the sysroot into the source tree beside it and should not have to.**
# Staging `fenv.h` into `sysroot/usr/include` is oops-mesa's to do; when it does, this line goes.
CTS_INCLUDES += -I$(OOPS_MESA)/toolchain/msun-src/msun/x86

# # `_XOPEN_SOURCE=600`, pinned from both sides
#
# At least **500**, because `deThreadUnix.c:32` is
# `#if !defined(_XOPEN_SOURCE) || (_XOPEN_SOURCE < 500)` over `#error "You are using too old
# posix API!"`. `_POSIX_C_SOURCE` is a different macro and does not satisfy it.
#
# At most **600**, because FreeBSD's `<unistd.h>` guards `usleep` with
# `(__XSI_VISIBLE && __XSI_VISIBLE <= 600) || __BSD_VISIBLE`: POSIX 2008 removed it, 700 hides
# it, and `deThreadUnix.c` calls it.
#
# Setting it also drives `__BSD_VISIBLE` to 0, which is separately needed: FreeBSD's
# `<unistd.h>` declares `fflagstostr(u_long)` and `select(..., fd_set *, ...)` in BSD-visible
# blocks *without including `<sys/types.h>` itself*, so a unit including `<unistd.h>` alone meets
# them with no `u_long` in scope.
CTS_DEFINES := -D_XOPEN_SOURCE=600 -DDEQP_TARGET_NAME=\"OOPS\"

# `-include malloc_np.h`: `deMemory.c` calls `malloc_usable_size`, which FreeBSD declares in
# `<malloc_np.h>` - the jemalloc extensions - and not in `<stdlib.h>`, which is what upstream
# includes on this platform. This puts the declaration in scope without shadowing `<stdlib.h>`,
# which is a trap this port has already fallen into once.
CTS_C_EXTRA := -include malloc_np.h

# The platform layer and the generated loader table. `shim/gl_loader_table.cpp` is produced by
# `tools/gen-gl-loader.py` from Mesa's own archive, so it is generated at build time rather than
# committed - see the rule below.
CTS_SHIM_CXX := shim/tcuOopsPlatform.cpp shim/gl_loader_table.cpp

# # Upstream's C builds into an archive, under upstream's flags
#
# This repository compiles C with `-Werror -Wconversion -Wsign-conversion -Wstrict-prototypes`,
# and dEQP does not survive that - `deDefs.c` stops on the first file, at a `deCtz32` result
# assigned to an `int32_t`.
#
# **The archive is not a way of turning the warnings off, it is a way of turning them off for
# the right code.** Adding `-Wno-` flags to `PAYLOAD_SRCS` would silence them for `shim/`, which
# *is* ours and should be held to the repository's standard. Neverball reached the same
# structure for the same reason and says so at greater length.
#
# The C++ needs no equivalent: `common/cxx.mk` already compiles with `-w`, because a C++ title
# here is always somebody else's code.
OOPS_CTS_BUILD := build/cts
OOPS_CTS_LIB   := $(OOPS_CTS_BUILD)/libdeqp_c.a

OOPS_CXX_EXCEPTIONS = 1
OOPS_CXX_SRCS       = $(CTS_CXX_SRCS) $(CTS_SHIM_CXX)

PAYLOAD_SRCS        = shim/gl_cts_entry.c

include $(OOPS_LIBCXX)/oops-libcxx.mk
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk
include $(OOPS_APPS_ROOT)/common/cxx.mk

OOPS_CXX_INCLUDE     += $(OOPS_LIBCXX_INCLUDE) $(CTS_INCLUDES) $(CTS_DEFINES)

# # `CTS_DEFINES` is deliberately NOT in `EXTRA_TARGET_CFLAGS`
#
# That variable reaches every C source in the payload, and the payload of a Mesa title includes
# **oops-mesa's own runtime shim**. `_XOPEN_SOURCE` drives `__BSD_VISIBLE` to 0, and
# `src/runtime/libc_absent.c` needs it on: it uses `_SC_NPROCESSORS_CONF`, `id_t`, `cpuset.h`
# and `__bitcountl`, all BSD-visible. Setting it globally turned a clean build of oops-mesa's
# runtime into twenty errors in somebody else's file.
#
# So the define goes where it is needed and nowhere else: upstream's C archive below, and the
# C++ through `OOPS_CXX_INCLUDE`. `shim/gl_cts_entry.c` includes no dEQP header and wants
# neither.

# `--whole-archive` because the framework's entry points are reached from each other rather than
# from `main` in a way the linker can follow on demand - `tcuMain.cpp` pulls in the test
# hierarchy, which pulls in everything else by registration. Same reasoning `cxx.mk` gives for
# its own archive.
EXTRA_TARGET_LDFLAGS += -Wl,--whole-archive $(OOPS_CTS_LIB) -Wl,--no-whole-archive \
                        $(OOPS_CXX_LDFLAGS) $(OOPS_LIBCXXABI_LDFLAGS) $(OOPS_LIBUNWIND_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(OOPS_CTS_LIB) \
                        $(OOPS_CXX_LIB) $(OOPS_LIBCXXABI_LIB) $(OOPS_LIBUNWIND_LIB)

include $(OOPS_APPS_ROOT)/common/app.mk

# Upstream's flags: `-w`, and the sysroot the hosted build uses. `$(OOPS_MESA_INCLUDE)` carries
# `--sysroot=`, which is what makes this a hosted compile rather than a freestanding one.
OOPS_CTS_CFLAGS = -target x86_64-unknown-freebsd -fPIC -fno-stack-protector -O2 -w -std=c11 \
                  $(OOPS_MESA_INCLUDE) $(OOPS_SDK_INCLUDE) \
                  $(CTS_INCLUDES) $(CTS_DEFINES) $(CTS_C_EXTRA)

OOPS_CTS_OBJS := $(patsubst %,$(OOPS_CTS_BUILD)/%.o,$(CTS_C_SRCS))

$(OOPS_CTS_BUILD)/%.c.o: %.c
	@mkdir -p $(dir $@)
	@$(TARGET_CC) $(OOPS_CTS_CFLAGS) -MMD -MP -c -o $@ $<

$(OOPS_CTS_LIB): $(OOPS_CTS_OBJS)
	@rm -f $@
	@ar_tool=$$(command -v $(AR) 2>/dev/null || command -v llvm-ar 2>/dev/null || command -v ar); \
	 "$$ar_tool" rcs $@ $(OOPS_CTS_OBJS)
	@echo "gl-cts: $(words $(OOPS_CTS_OBJS)) C objects -> $@"

-include $(OOPS_CTS_OBJS:.o=.d)

# The loader table is generated from two things that both move: upstream's `glwInit*.inl`, and
# whatever Mesa's `libglapi_bridge.a` currently defines. Regenerating it on a bump is the point,
# so it depends on both.
MESA_GLAPI := $(OOPS_MESA)/build/mesa/src/mesa/glapi/glapi/libglapi_bridge.a

shim/gl_loader_table.cpp: tools/gen-gl-loader.py $(MESA_GLAPI) \
                          $(UP)/opengl/wrapper/glwInitGL33.inl $(UP)/opengl/wrapper/glwInitExtGL.inl
	@python3 tools/gen-gl-loader.py $(MESA_GLAPI) > $@

.PHONY: survey
# Compile every framework source one at a time and report what each failure is waiting for. The
# same instrument as the libc++ survey behind `oops-apps#D007`, for the same reason: a port this
# size needs a cause histogram, not a first error.
survey:
	@tools/survey.sh
