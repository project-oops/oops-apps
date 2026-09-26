# The OpenGL CTS, built for the console.
#
# `upstream.lock` says what is fetched. This file builds dEQP's framework, the GL test modules
# and the platform layer in `shim/`, the only code this port writes.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# The fetch runs while this file is read: the source lists below wildcard its directories.
include $(OOPS_APPS_ROOT)/common/upstream.mk

OOPS_RENDERER = mesa

# `OOPS_RENDERER = mesa` takes the C library from the Mesa sysroot, so the C++ library is the
# hosted build against the same sysroot; the freestanding one disagrees about `FILE` and
# `clock_t` (see `oops-deps/libcxx/oops-libcxx.mk`).
OOPS_LIBCXX_HOSTED = 1

OOPS_LIBCXX ?= $(abspath ../../oops-deps/libcxx)
OOPS_MESA   ?= $(abspath $(OOPS_APPS_ROOT)/../oops-mesa)

UP := upstream/framework

# The framework in upstream's dependency order: `delibs` (C base libraries), `qphelper` (the
# `.qpa` log), `common` (tcu), `opengl` (glu, with the generated entry-point table in
# `wrapper/`) and `referencerenderer` (rr, the software rasteriser for expected images).
# Wildcards, so upstream owns the file set; the exclusions below each say why.
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
               $(wildcard $(UP)/randomshaders/*.cpp) \
               $(wildcard $(UP)/xexml/*.cpp) \
               $(UP)/platform/tcuMain.cpp

# Left out of the framework:
#   standalone_test.c, rsgTest.cpp - self-test programs with their own `main()`; `tcuMain.cpp`
#     is the entry point. `randomshaders/` is built because `modules/glshared` includes it.
#   tcuImageIO.cpp - needs libpng, which is not ported; tests wanting a reference image miss it.
#   deSocket.c, deSocket.cpp - need `<netinet6/in6.h>`, absent from the sysroot; the C++ file
#     wraps the C one.
CTS_EXCLUDE := $(UP)/delibs/dethread/standalone_test.c \
               $(UP)/randomshaders/rsgTest.cpp \
               $(UP)/common/tcuImageIO.cpp \
               $(UP)/delibs/deutil/deSocket.c \
               $(UP)/delibs/decpp/deSocket.cpp

# The test modules: `MOD` is the OpenGL CTS and `GLS` is dEQP's shared GL test utilities, which
# tests in `MOD/common` include. Three levels deep, because `glesext/` and `common/` nest one
# level further than the rest.
MOD := upstream/external/openglcts/modules
GLS := upstream/modules/glshared

CTS_MOD_ALL := $(wildcard $(MOD)/*.cpp) $(wildcard $(MOD)/*/*.cpp) $(wildcard $(MOD)/*/*/*.cpp) \
               $(wildcard $(GLS)/*.cpp)

# Left out of the test modules:
#   runner/ - upstream's `cts-runner`, a separate executable with its own `main()`; a title
#     cannot spawn processes, so run selection lives in `/app0/cts-args.txt`.
#   pch.cpp - a precompiled-header stub.
#   glcTestPackageRegistry.cpp - replaced by `shim/gl_cts_registry.cpp`, since it registers
#     dEQP packages from `modules/gles*` and `modules/egl`, which are not fetched.
#   common/glcSpirvUtils.cpp - replaced by `shim/gl_cts_spirv_stub.cpp`; it needs glslang.
#   gl4cContextFlushControlTests.cpp - disabled upstream (`modules/gl/CMakeLists.txt`); it
#     calls `glu::Platform` methods `gluPlatform.hpp` does not declare.
# `glcTestPackageEntry.cpp` stays: its constructor registers every package.
CTS_MOD_EXCLUDE := $(wildcard $(MOD)/runner/*.cpp) \
                   $(MOD)/pch.cpp \
                   $(MOD)/glcTestPackageRegistry.cpp \
                   $(MOD)/common/glcSpirvUtils.cpp \
                   $(MOD)/gl/gl4cContextFlushControlTests.cpp

CTS_MOD_SRCS := $(filter-out $(CTS_MOD_EXCLUDE),$(CTS_MOD_ALL))

CTS_C_SRCS   := $(filter-out $(CTS_EXCLUDE),$(CTS_C_ALL))
CTS_CXX_SRCS := $(filter-out $(CTS_EXCLUDE),$(CTS_CXX_ALL)) $(CTS_MOD_SRCS)

CTS_INCLUDES := \
    -I$(UP)/delibs/debase -I$(UP)/delibs/depool -I$(UP)/delibs/deutil \
    -I$(UP)/delibs/dethread -I$(UP)/delibs/decpp -I$(UP)/delibs/deimage \
    -I$(UP)/qphelper -I$(UP)/common -I$(UP)/opengl -I$(UP)/referencerenderer \
    -I$(UP)/egl -I$(UP)/opengl/wrapper -I$(UP)/egl/wrapper -I$(UP)/xexml \
    -I$(UP)/opengl/simplereference -I$(UP)/randomshaders \
    -Ishim/include

# Every test-module directory holding a header: upstream's includes are flat
# (`esextcTestCaseBase.hpp`, not `../esextcTestCaseBase.hpp`), so the set must be complete.
CTS_INCLUDES += $(addprefix -I,$(patsubst %/,%,$(sort \
    $(dir $(wildcard $(MOD)/*.hpp $(MOD)/*/*.hpp $(MOD)/*/*/*.hpp $(GLS)/*.hpp)))))

# `<fenv.h>` from msun's source tree (amd64 uses `msun/x86/fenv.h`), because the sysroot has
# none. `deMath.c` sets the rounding mode to compute reference values for the GLSL rounding
# tests, from the same pinned checkout as `libm.a`.
CTS_INCLUDES += -I$(OOPS_MESA)/toolchain/msun-src/msun/x86

# `_XOPEN_SOURCE=600`: at least 500 for `deThreadUnix.c:32`'s check, at most 600 because
# FreeBSD's `<unistd.h>` hides `usleep` above it. It also sets `__BSD_VISIBLE` to 0, which keeps
# `<unistd.h>`'s BSD declarations that need `u_long` out of units without `<sys/types.h>`.
CTS_DEFINES := -D_XOPEN_SOURCE=600 -DDEQP_TARGET_NAME=\"OOPS\"

# `deMemory.c` calls `malloc_usable_size`, which FreeBSD declares in `<malloc_np.h>`, not
# `<stdlib.h>`; force-including it leaves `<stdlib.h>` unshadowed.
CTS_C_EXTRA := -include malloc_np.h

# The platform layer and the loader table `tools/gen-gl-loader.py` generates at build time.
CTS_SHIM_CXX := shim/tcuOopsPlatform.cpp shim/gl_loader_table.cpp shim/gl_cts_gaps.cpp \
                shim/gl_cts_registry.cpp shim/gl_cts_spirv_stub.cpp

# Upstream's C builds into an archive under `-w`, so the repository's `-Werror -Wconversion`
# set still holds for `shim/`. `common/cxx.mk` already compiles C++ with `-w`.
OOPS_CTS_BUILD := build/cts
OOPS_CTS_LIB   := $(OOPS_CTS_BUILD)/libdeqp_c.a

# The import manifest is generated from obSCEne's corpus; a Mesa title imports more than
# `common/symbols.txt` carries. Run `make imports` after a link, then `make title`: without
# `build/imports.txt` mkmodule builds no vendor segment and the loader refuses the module.
SYMBOLS_FILE := $(abspath build/imports.txt)

# SELFish's `native_eboot.ld` plus a PT_TLS segment, which Mesa's thread-local GL dispatch
# needs, and `.init_array` bounds for `oops_mesa_run_init_array()`. Every CTS package enters
# the registry through a constructor (`glcTestPackageEntry.cpp`), as does ACO's opcode table.
# This copy keeps `.eh_frame`.
TARGET_LD_SCRIPT := $(abspath local_tls.ld)

OOPS_CXX_EXCEPTIONS = 1

# The test modules are C++17 (`std::make_unique` in `glcBufferObjectsTests.cpp:265`).
# `tools/survey.sh` must compile at the same standard.
OOPS_CXX_STD = c++17
OOPS_CXX_SRCS       = $(CTS_CXX_SRCS) $(CTS_SHIM_CXX)

OOPS_FEATURES := display thread

PAYLOAD_SRCS        = shim/gl_cts_entry.c

include $(OOPS_LIBCXX)/oops-libcxx.mk
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk
include $(OOPS_APPS_ROOT)/common/cxx.mk

OOPS_CXX_INCLUDE     += $(OOPS_LIBCXX_INCLUDE) $(CTS_INCLUDES) $(CTS_DEFINES)

# `-ftls-model=initial-exec`: `glwWrapper.cpp` keeps the GL function table in a `__thread`
# variable, and the general-dynamic model's `__tls_get_addr` faults in libkernel on this
# platform. oops-mesa builds Mesa with the same flag. It goes on the C++ only; see below.
OOPS_CXX_INCLUDE     += -ftls-model=initial-exec

# `CTS_DEFINES` stays out of `EXTRA_TARGET_CFLAGS`, which reaches oops-mesa's runtime shim:
# `src/runtime/libc_absent.c` needs `__BSD_VISIBLE`. The defines go to upstream's C archive
# and, through `OOPS_CXX_INCLUDE`, the C++; `shim/gl_cts_entry.c` includes no dEQP header.

# `--whole-archive`: the framework reaches its tests by registration, not calls the linker can
# follow. `OOPS_CXX_LDFLAGS` is this title's C++ archive; `OOPS_LIBCXX_LDFLAGS` is libc++
# itself, and the link ignores unresolved symbols, so omitting it still links.
EXTRA_TARGET_LDFLAGS += -Wl,--whole-archive $(OOPS_CTS_LIB) -Wl,--no-whole-archive \
                        $(OOPS_CXX_LDFLAGS) $(OOPS_LIBCXX_LDFLAGS) \
                        $(OOPS_LIBCXXABI_LDFLAGS) $(OOPS_LIBUNWIND_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(OOPS_CTS_LIB) $(OOPS_CXX_LIB) $(OOPS_LIBCXX_LIB) \
                        $(OOPS_LIBCXXABI_LIB) $(OOPS_LIBUNWIND_LIB)

include $(OOPS_APPS_ROOT)/common/app.mk

# oops-mesa's `liboopsmesa_cxx` stand-ins (`std::logic_error` and friends) duplicate libc++ and
# libc++abi, which this title links, so it is filtered out here; other Mesa titles need it.
# `TARGET_LDFLAGS` reads `EXTRA_TARGET_LDFLAGS` at link time, so filtering after the include
# reaches the link line.
EXTRA_TARGET_LDFLAGS := $(filter-out -loopsmesa_cxx,$(EXTRA_TARGET_LDFLAGS))

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

# The loader table depends on upstream's `glwInit*.inl` and on what Mesa's
# `libglapi_bridge.a` defines.
MESA_GLAPI := $(OOPS_MESA)/build/mesa/src/mesa/glapi/glapi/libglapi_bridge.a

shim/gl_loader_table.cpp: tools/gen-gl-loader.py $(MESA_GLAPI) \
                          $(UP)/opengl/wrapper/glwInitGL33.inl $(UP)/opengl/wrapper/glwInitExtGL.inl
	@python3 tools/gen-gl-loader.py $(MESA_GLAPI) > $@

$(SYMBOLS_FILE): $(BUILD)/$(APP_NAME).elf
	@if [ ! -f "$@" ]; then \
	  mkdir -p $(BUILD); \
	  OOPS_APPS_SYMBOLS=$(OOPS_APPS_ROOT)/common/symbols.txt \
	    bash $(OOPS_MESA)/tools/generate-imports.sh $< $@; \
	fi

.PHONY: imports
imports: $(BUILD)/$(APP_NAME).elf
	@mkdir -p $(BUILD)
	@OOPS_APPS_SYMBOLS=$(OOPS_APPS_ROOT)/common/symbols.txt \
	  bash $(OOPS_MESA)/tools/generate-imports.sh $< $(SYMBOLS_FILE)

.PHONY: survey
# Compile every framework and test-module source one at a time and report a histogram of
# failure causes (as for the libc++ survey, oops-apps#D007).
survey:
	@tools/survey.sh

# Stage the shaders, reference images and `.nspv` binaries the tests open at run time. They are
# read relative to `--deqp-archive-dir` (`/app0`, set in `shim/gl_cts_entry.c`) as
# `gl_cts/data/...`, so the tree lands at `/app0/gl_cts`. Separate from `title`; a `pros
# restore` costs per file. `mustpass/` is left out: it is the case lists `runner/` reads, not a
# resource any test opens. Case selection is in `/app0/cts-args.txt`.
GL_CTS_DATA := upstream/external/openglcts/data/gl_cts

.PHONY: stage-data
stage-data:
	@test -d $(BUILD)/title/$(TITLE_ID) || { \
	    echo "stage-data: no packaged title at $(BUILD)/title/$(TITLE_ID) - run 'make title' first"; \
	    exit 1; \
	}
	@test -d $(GL_CTS_DATA) || { \
	    echo "stage-data: $(GL_CTS_DATA) is missing - the upstream fetch has not run"; \
	    exit 1; \
	}
	@rm -rf $(BUILD)/title/$(TITLE_ID)/gl_cts
	@cd $(GL_CTS_DATA)/.. && find gl_cts -type f -not -path 'gl_cts/data/mustpass/*' -print0 \
	    | tar --null -cf - -T - \
	    | tar -xf - -C $(abspath $(BUILD))/title/$(TITLE_ID)
	@echo "stage-data: $$(find $(BUILD)/title/$(TITLE_ID)/gl_cts -type f | wc -l) files, $$(du -sh $(BUILD)/title/$(TITLE_ID)/gl_cts | cut -f1) -> /app0/gl_cts"
