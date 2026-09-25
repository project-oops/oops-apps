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
               $(wildcard $(UP)/randomshaders/*.cpp) \
               $(wildcard $(UP)/xexml/*.cpp) \
               $(UP)/platform/tcuMain.cpp

# # What is left out, and why each one
#
# **`standalone_test.c` and `rsgTest.cpp`** are dEQP's own self-test programs and each has its
# own `main()`. `tcuMain.cpp` is the real entry point; keeping all three is a duplicate symbol.
#
# `rsgTest.cpp`'s line was a **no-op until 2026-09-24**: it excluded a file from a directory no
# wildcard above collected, so it named something that was never a candidate. `randomshaders/`
# is now in the list, which is what makes the exclusion mean anything. It went in because
# `modules/glshared` needs it - `glsRandomShaderCase`, `glsRandomShaderProgram` and
# `glsFragOpInteractionCase` include `rsgShader.hpp` and its siblings, and those three were the
# only files in the whole build that would not compile once the test modules arrived.
#
# Adding the directory rather than dropping the three is the choice worth stating: nothing in
# `external/openglcts` refers to them, so excluding them would have worked and would have left
# `glshared` a different library from the one upstream builds, for a saving of 24 sources and
# 464 KB that were already fetched.
#
# **`tcuImageIO.cpp`** needs libpng, which is not ported. It reads and writes reference images,
# so an image-comparison test that wants one will not find it - but nothing else stops, and
# porting libpng for it is a separate piece of work.
#
# **`deSocket.c`** needs `<netinet6/in6.h>`, absent from the staged sysroot. It exists so a host
# can drive tests over a wire; a title running its own cases needs none of it. **`deSocket.cpp`
# goes with it** - it is `decpp`'s C++ wrapper over exactly those functions, and leaving it in
# was the difference between "excluded" and "excluded and still referenced": fourteen
# `deSocket_*` and `deSocketAddress_*` names came out of the import check with nowhere to
# resolve.
CTS_EXCLUDE := $(UP)/delibs/dethread/standalone_test.c \
               $(UP)/randomshaders/rsgTest.cpp \
               $(UP)/common/tcuImageIO.cpp \
               $(UP)/delibs/deutil/deSocket.c \
               $(UP)/delibs/decpp/deSocket.cpp

# # The test modules - the tests themselves
#
# Everything above is dEQP's machinery; this is the OpenGL CTS. `MOD` is 311 sources in twenty-two
# directories and `GLS` is dEQP's shared GL test utilities, which is here because four tests in
# `MOD/common` include it directly - `upstream.lock` says which.
#
# Wildcarded three levels deep because `glesext/` nests one level further than the rest
# (`glesext/geometry_shader`, `glesext/tessellation_shader`, and nine more) and `common/` nests
# `subgroups` and `meshShader`. Enumerating them would be a list to maintain against somebody
# else's tree; this way a directory that appears in a version bump arrives with it.
MOD := upstream/external/openglcts/modules
GLS := upstream/modules/glshared

CTS_MOD_ALL := $(wildcard $(MOD)/*.cpp) $(wildcard $(MOD)/*/*.cpp) $(wildcard $(MOD)/*/*/*.cpp) \
               $(wildcard $(GLS)/*.cpp)

# # What is left out of the test modules, and why each one
#
# **`runner/`** is upstream's `cts-runner`, a separate executable that drives this one repeatedly
# across the mustpass config lists. `glcTestRunnerMain.cpp` has its own `main()` - a duplicate
# symbol - and nothing outside `runner/` refers to anything in it. A title here has no process to
# spawn, so the multi-config loop belongs in `/app0/cts-args.txt` and a host, not in the binary.
#
# **`pch.cpp`** is a precompiled-header stub: one `#include "pch.h"` and nothing else.
#
# **`glcTestPackageRegistry.cpp`** is replaced by `shim/gl_cts_registry.cpp`. It registers six
# packages out of dEQP's own `modules/egl`, `modules/gles2`, `modules/gles3` and
# `modules/gles31`, which the lock does not fetch. The shim registers the other twenty-five
# exactly as upstream constructs them and lists the six with what each would cost.
#
# **`common/glcSpirvUtils.cpp`** is replaced by `shim/gl_cts_spirv_stub.cpp`. It is the only file
# in the tree needing glslang and spirv-tools, which upstream fetches with its own script rather
# than carrying, so no sparse path can reach them. Both files say the rest.
#
# **`glcTestPackageEntry.cpp` is NOT excluded** and must not be: it is the constructor that calls
# `glcts::registerPackages()`, and it is the entire mechanism by which a package reaches the
# registry.
# **`gl4cContextFlushControlTests.cpp` is disabled upstream and this matches upstream.** Its two
# lines in `modules/gl/CMakeLists.txt` are commented out, and so is
# `gl4cTestPackages.cpp:389`'s `addChild(new gl4cts::ContextFlushControl::Tests(...))` - so no
# package contains it and no build compiles it. It cannot compile: it calls
# `platform.getContextFlushControlContextAttributes()` and `platform.createRenderContext()` on a
# `glu::Platform`, and `gluPlatform.hpp` declares neither. `getContextFlushControlContextAttributes`
# appears three times in the whole of VK-GL-CTS, all three in this file.
#
# It is listed here because these lists are wildcards, and a wildcard takes what upstream's
# CMakeLists declines to. Comparing every `CMakeLists.txt` under the modules against the files
# beside it finds exactly four disagreements: this, `pch.cpp`, and the two `main`s in `runner/`.
# All four are named in this block, and nothing else on disk is left out of the build.
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

# Every directory under the test modules that holds a header, found the same way the sources
# are. Upstream's `modules/CMakeLists.txt` and the eleven under `glesext/` each name their own
# with `include_directories`, and the includes are flat - `esextcTestCaseBase.hpp`, not
# `../esextcTestCaseBase.hpp` - so the set has to be complete rather than representative.
CTS_INCLUDES += $(addprefix -I,$(patsubst %/,%,$(sort \
    $(dir $(wildcard $(MOD)/*.hpp $(MOD)/*/*.hpp $(MOD)/*/*/*.hpp $(GLS)/*.hpp)))))

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
CTS_SHIM_CXX := shim/tcuOopsPlatform.cpp shim/gl_loader_table.cpp shim/gl_cts_gaps.cpp \
                shim/gl_cts_registry.cpp shim/gl_cts_spirv_stub.cpp

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

# # The import manifest, which `mkmodule` needs and `common/symbols.txt` cannot supply
#
# A packaged title carries a vendor segment saying which platform library resolves each imported
# name. `common/symbols.txt` is the hand-kept list for freestanding titles; a hosted one imports
# far more than it carries, so oops-mesa generates the manifest from obSCEne's mined corpus
# instead. mesa-demos and mesa-cube do the same and say why at greater length.
#
# **`make imports` after a link, then `make title`.** Without `build/imports.txt` mkmodule builds
# no vendor segment and the container is refused when it loads - which mesa-winsys-probe paid a
# launch to discover.
SYMBOLS_FILE := $(abspath build/imports.txt)

# # The link script, and the two things `native_eboot.ld` does not give a hosted title
#
# The same local copy `mesa-cube` and the two Mesa probes carry, for the same two reasons, and
# both were paid for again here before this line existed.
#
# **`PT_TLS`**: SELFish's `native_eboot.ld` emits no TLS segment and Mesa cannot link without one,
# because its GL dispatch is thread-local by construction. All four copies go when
# `REQ-20260915T0001Z-8d72` on the SELFish bus resolves.
#
# **`.init_array` and its `PROVIDE_HIDDEN` bounds**: a hosted title has no crt to walk them, so
# `oops_mesa_run_init_array()` reads `__init_array_start`/`__init_array_end` - and with
# `native_eboot.ld` those sections do not exist, the symbols resolve to nothing, and the walk
# faults. On hardware that presented as `fault address: ...(xotext:"eboot.bin"+0x26a870)`
# immediately after the title's first log line: a data read from the execute-only text segment,
# because the "array" it was walking was code.
#
# **This title needs the constructors more than any other.** ACO's opcode table is one, and
# `external/openglcts/modules/glcTestPackageEntry.cpp` is another - it is nothing but
# `RegisterCTSPackages g_registerCTS;`, which is how every CTS package enters the registry.
TARGET_LD_SCRIPT := $(abspath local_tls.ld)

OOPS_CXX_EXCEPTIONS = 1

# # `c++17`, because the test modules are written in it
#
# `common/cxx.mk` defaults to `c++11` and the dEQP *framework* is happy there - which is why this
# line did not exist while the framework was the whole port. The test modules are not:
# `glcBufferObjectsTests.cpp:265` is `std::make_unique`, which is C++14, and `glcFramebufferBlitTests.cpp`
# wants C++17 container semantics a dozen lines further on.
#
# **`tools/survey.sh` must use the same value, and the day it did not cost a whole build.** The
# survey compiled all 305 test sources at `c++17` and reported two failures, so the modules were
# taken as clean; the build then ran at `c++11` and stopped on the first `make_unique`. An
# instrument set up differently from the thing it measures cannot report the difference - it is
# the check arm that cannot fail, and this one confirmed something false for an hour.
OOPS_CXX_STD = c++17
OOPS_CXX_SRCS       = $(CTS_CXX_SRCS) $(CTS_SHIM_CXX)

OOPS_FEATURES := display thread

PAYLOAD_SRCS        = shim/gl_cts_entry.c \
                      shim/gl_cts_open.c

include $(OOPS_LIBCXX)/oops-libcxx.mk
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk
include $(OOPS_APPS_ROOT)/common/cxx.mk

OOPS_CXX_INCLUDE     += $(OOPS_LIBCXX_INCLUDE) $(CTS_INCLUDES) $(CTS_DEFINES)

# # `-ftls-model=initial-exec`, because dEQP's GL dispatch is thread-local
#
# `glwWrapper.cpp` keeps the current function table in a `__thread` variable:
#
#     DE_THREAD_LOCAL const glw::Functions *s_functions = nullptr;
#
# Compiled general-dynamic - the default for `-fPIC` - writing it goes through `__tls_get_addr`,
# and on this platform that call reaches libkernel and faults. Measured on hardware 2026-09-25:
# SIGSEGV reading address 0x8, `rip` inside `libkernel.sprx`, called from
# `glw::setCurrentThreadFunctions+0x19` under `deqp::Context::createRenderContext` - the first
# thing a test package does after the GL context exists.
#
# **oops-mesa already carries this flag for the same reason.** Its meson command line is
# `-ftls-model=initial-exec`, which is why Mesa's own thread-local dispatch works in `mesa-cube`;
# this title's C++ was the part that did not have it. `initial-exec` resolves a thread-local
# through the thread pointer at a link-time offset, so nothing is called at all.
#
# It goes on the C++ rather than in `EXTRA_TARGET_CFLAGS` for the reason the `CTS_DEFINES` note
# below gives: that variable also reaches oops-mesa's runtime shim, and this title has no business
# changing how somebody else's sources are built.
OOPS_CXX_INCLUDE     += -ftls-model=initial-exec

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
# **`$(OOPS_LIBCXX_LDFLAGS)` is the one to not forget.** `OOPS_CXX_LDFLAGS` is `cxx.mk`'s
# archive - this title's own C++ objects - and is a different thing from libc++ itself.
# `cxx-throw` lists the other three and not this one, correctly, because it links libc++abi and
# libunwind without needing the standard library; copying that line here produced a binary with
# 315 undefined `std::` symbols and no link error, because a hosted title is linked
# `--unresolved-symbols=ignore-all`.
EXTRA_TARGET_LDFLAGS += -Wl,--whole-archive $(OOPS_CTS_LIB) -Wl,--no-whole-archive \
                        $(OOPS_CXX_LDFLAGS) $(OOPS_LIBCXX_LDFLAGS) \
                        $(OOPS_LIBCXXABI_LDFLAGS) $(OOPS_LIBUNWIND_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(OOPS_CTS_LIB) $(OOPS_CXX_LIB) $(OOPS_LIBCXX_LIB) \
                        $(OOPS_LIBCXXABI_LIB) $(OOPS_LIBUNWIND_LIB)

include $(OOPS_APPS_ROOT)/common/app.mk

# # `liboopsmesa_cxx` comes out of the link, because this title brings the real thing
#
# oops-mesa carries a two-object C++ support archive in its sysroot - `cxx_support.o` and
# `upstream_stdexcept.o` - built when there was no C++ runtime here at all. Its own roadmap says
# why: Mesa is compiled `-fno-exceptions`, ACO needs eight definitions, and `__cxa_pure_virtual`
# is referenced from vtables. A handful of stand-ins was the right answer then.
#
# It is not the right answer for a title that links libc++ and libc++abi. Both define
# `std::logic_error`, `std::runtime_error`, `std::domain_error` and their destructors, so the
# link stops on a dozen duplicate symbols - and the copy to keep is libc++abi's, which is the
# one the exception machinery in this title actually unwinds through.
#
# Filtered here rather than in oops-mesa, because oops-mesa is right to ship it: every other
# Mesa title still needs it, and none of them brings a C++ runtime. This is the first that does.
#
# `TARGET_LDFLAGS` is recursively expanded and reads `EXTRA_TARGET_LDFLAGS` at link time, so
# filtering after the include reaches the link line.
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

# The loader table is generated from two things that both move: upstream's `glwInit*.inl`, and
# whatever Mesa's `libglapi_bridge.a` currently defines. Regenerating it on a bump is the point,
# so it depends on both.
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
# Compile every framework and test-module source one at a time and report what each failure is
# waiting for. The same instrument as the libc++ survey behind `oops-apps#D007`, for the same
# reason: a port this size needs a cause histogram, not a first error.
survey:
	@tools/survey.sh

# # Stage the tests' resources into the packaged title
#
# The shaders, reference images and `.nspv` binaries the tests open at run time. They are read
# through `tcu::Archive` relative to `--deqp-archive-dir`, which `shim/gl_cts_entry.c` sets to
# `/app0` - the title's own directory - and the tests name them `gl_cts/data/...`, so the tree
# lands at `/app0/gl_cts` with its shape unchanged.
#
# Separate from `title` for mesa-demos' reason and one of its own. `make title` is what every
# other consumer runs and must not carry this; and a `pros restore` costs per *file* rather than
# per byte - 1775 files took 81 minutes once - so what goes across the wire is worth counting
# before it is sent.
#
# **`mustpass/` is excluded, and it is 138 MB of the 150.** Those are the case lists
# `external/openglcts/modules/runner` reads to drive this binary repeatedly across the
# conformance configurations - host-side input to a runner that `CTS_MOD_EXCLUDE` leaves out,
# not a resource any test opens. Excluding it takes the staged tree from 899 files to 233 and
# from 150 MB to 13 MB, and no test can tell the difference. Selecting *cases* stays a run-time
# choice in `/app0/cts-args.txt`; this selects nothing.
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
