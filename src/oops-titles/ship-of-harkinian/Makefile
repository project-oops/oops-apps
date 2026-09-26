# Ship of Harkinian, through oops-sdk.
#
#   make survey          the GL entry points and dependencies the checked-out tree needs
#   make compile-survey  compile every source with -fsyntax-only, grouped by first error
#   make generated       the generated headers
#
# The port plan is in README.md.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# --- the toolchain ------------------------------------------------------------
#
# This title does not include `common/app.mk`, so it defines the toolchain variables here with
# the same defaults. The survey's GL check reads `$(OOPS_SDK_DIR)`.
OOPS_SDK ?= $(abspath $(OOPS_APPS_ROOT)/../oops-sdk)
OOPS_SDK_DIR := $(abspath $(OOPS_SDK))
OOPS_SDK_INCLUDE ?= -I$(OOPS_SDK_DIR)/include -I$(OOPS_SDK_DIR) -DOOPS_TARGET=3
OOPS_SDK_LIBC_INCLUDE ?= -I$(OOPS_SDK_DIR)/include/libc
TARGET_CC ?= clang
TARGET_CXX ?= clang++
AR ?= ar

# The fetch runs while this file is read (`common/upstream.mk`). It is included before the
# rules that name files under `upstream/`.
include $(OOPS_APPS_ROOT)/common/upstream.mk

# --- GL entry points ------------------------------------------------------------
#
# The entry points the port calls beyond the common set, checked against `oops-gl` by
# `survey`. The two framebuffer calls come from libultraship's `gfx_opengl.cpp`;
# `glRenderbufferStorageMultisample` only runs with a non-zero `msaa_level`. The
# vertex-array family comes from ImGui's GL3 backend, which uses vertex array objects
# unconditionally on desktop GL (the ones in `gfx_opengl.cpp` sit behind `__APPLE__` or
# `USE_OPENGLES`).
SOH_GL_GAP := glBlitFramebuffer glRenderbufferStorageMultisample \
              glGenVertexArrays glBindVertexArray glDeleteVertexArrays glGetStringi

# The third-party libraries upstream fetches, minus the Apple-only one, including those the
# `libultraship` submodule declares (`imgui`). `survey` checks each for a directory under
# `src/oops-deps/`.
SOH_DEPS := sdl2 zlib libzip stb nlohmann-json thread-pool tinyxml2 spdlog \
            stormlib libgfxd prism-processor imgui

# --- generated headers ------------------------------------------------------
#
# The CVAR names are generated from upstream's CMake files by `gen-cvars.sh`, so they follow
# upstream. `ship/install_config.h` is a fixed choice and lives in `shim/include/`.
#
# CMake passes the CVAR names as `add_compile_definitions` and no source includes a header for
# them (`Controller.cpp:69` uses `CVAR_PREFIX_CONTROLLERS` directly), so the generated header
# is force-included with `-include`, which avoids quoting each value through make and the
# shell.
#
# `ENABLE_OPENGL` selects libultraship's renderer; `Gui.cpp` includes the matching ImGui
# backend behind it. OpenGL is the only renderer on this target.
#
# `F3DEX_GBI_2` is the microcode Ocarina of Time uses, set by upstream at `CMakeLists.txt:159` and
# in each of soh's four target definition blocks. Without it `gbi.h` defines neither `G_TRI2` nor
# `G_GEOMETRYMODE`, which the display-list viewer reads.
#
# The three spdlog and stb settings are upstream's own, and they matter here rather than being
# inherited noise: this platform has no thread-local storage for them to use.
SOH_DEFS := -DENABLE_OPENGL -DF3DEX_GBI_2 -DNDEBUG \
            -DSPDLOG_NO_THREAD_ID -DSPDLOG_NO_TLS -DSTBI_NO_THREAD_LOCALS

SOH_GEN_DIR := $(CURDIR)/build/gen
SOH_CVARS_H := $(SOH_GEN_DIR)/ship/cvars_generated.h
SOH_GEN_INCLUDE := -I$(SOH_GEN_DIR) -I$(CURDIR)/shim/include \
                   -include ship/cvars_generated.h

$(SOH_CVARS_H): $(CURDIR)/gen-cvars.sh \
                upstream/CMake/soh-cvars.cmake upstream/CMake/lus-cvars.cmake \
                upstream/libultraship/cmake/cvars.cmake
	@sh $(CURDIR)/gen-cvars.sh $(CURDIR)/upstream $@

.PHONY: generated
generated: $(SOH_CVARS_H)

# --- the include set -----------------------------------------------------------
#
# Every dependency's build file, for the include variables each exports.
OOPS_LIBCXX ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libcxx)
include $(OOPS_LIBCXX)/oops-libcxx.mk
OOPS_SDL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2)
include $(OOPS_SDL)/oops-sdl.mk
OOPS_ZLIB ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/zlib)
include $(OOPS_ZLIB)/oops-zlib.mk
OOPS_SPDLOG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/spdlog)
include $(OOPS_SPDLOG)/oops-spdlog.mk
OOPS_IMGUI ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/imgui)
include $(OOPS_IMGUI)/oops-imgui.mk
OOPS_PRISM ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/prism-processor)
include $(OOPS_PRISM)/oops-prism.mk
OOPS_STORMLIB ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/stormlib)
include $(OOPS_STORMLIB)/oops-stormlib.mk
OOPS_LIBZIP ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libzip)
include $(OOPS_LIBZIP)/oops-libzip.mk
OOPS_TINYXML2 ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/tinyxml2)
include $(OOPS_TINYXML2)/oops-tinyxml2.mk
OOPS_GFXD ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libgfxd)
include $(OOPS_GFXD)/oops-libgfxd.mk
OOPS_STB ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/stb)
include $(OOPS_STB)/oops-stb.mk
OOPS_JSON ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/nlohmann-json)
include $(OOPS_JSON)/oops-nlohmann-json.mk
OOPS_THREADPOOL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/thread-pool)
include $(OOPS_THREADPOOL)/oops-thread-pool.mk
# Header-only, and pinned at the commit soh's own FetchContent_Declare names.
# `AudioSampleFactory.cpp` defines DR_WAV_IMPLEMENTATION and compiles it there.
OOPS_DR_LIBS ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/dr-libs)
include $(OOPS_DR_LIBS)/oops-dr-libs.mk
include $(OOPS_APPS_ROOT)/common/posix.mk
include $(OOPS_APPS_ROOT)/common/rt.mk

# The port's own headers, taken from `soh/CMakeLists.txt`'s `target_include_directories` and
# `libultraship/src/CMakeLists.txt`.
SOH_OWN_INCLUDE := -I$(CURDIR)/upstream \
                   -I$(CURDIR)/upstream/libultraship/src \
                   -I$(CURDIR)/upstream/libultraship/include \
                   -I$(CURDIR)/upstream/soh \
                   -I$(CURDIR)/upstream/soh/include \
                   -I$(CURDIR)/upstream/soh/src \
                   -I$(CURDIR)/upstream/soh/assets \
                   -I$(CURDIR)/upstream/ZAPDTR/ZAPD/resource/type

SOH_INCLUDE := $(SOH_OWN_INCLUDE) $(SOH_GEN_INCLUDE) $(SOH_DEFS) \
               $(OOPS_SPDLOG_INCLUDE) $(OOPS_JSON_INCLUDE) $(OOPS_TINYXML2_INCLUDE) \
               $(OOPS_PRISM_INCLUDE) $(OOPS_GFXD_INCLUDE) $(OOPS_STB_INCLUDE) \
               $(OOPS_THREADPOOL_INCLUDE) $(OOPS_IMGUI_INCLUDE) $(OOPS_DR_LIBS_INCLUDE) \
               $(OOPS_STORMLIB_INCLUDE) $(OOPS_LIBZIP_INCLUDE) $(OOPS_ZLIB_INCLUDE) \
               $(OOPS_SDL_INCLUDE) $(OOPS_SDL_PREFIX_INCLUDE)

# `$(OOPS_LIBCXX_INCLUDE)` before the C headers - libc++ ships its own `<math.h>` and `<stdlib.h>`
# and `<cmath>` stops with an explicit error if it reaches the C ones first. It carries
# `-nostdinc++ -nostdlibinc` itself.
#
# `$(OOPS_POSIX_INCLUDE)` comes before `$(SOH_INCLUDE)`: both it and `oops-deps/sdl2/include`
# have a `unistd.h`, and SDL's declares only `_exit`. spdlog's `os-inl.h:395` needs the
# shim's `getpid`.
# `-Wno-c++11-narrowing` on top of `-w`, because narrowing in an initialiser list is a default
# *error* rather than a warning, so `-w` does not reach it. Upstream narrows a float to uint8_t in a
# colour literal and two integer constants past their type.
#
# `-include cctype` for the two sources that call `std::toupper` without including it: libstdc++
# reaches it transitively and libc++ does not.
SOH_CXXFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib -fPIC \
               -O2 -w -Wno-c++11-narrowing -std=c++20 -D__PROSPERO__=1 -include cctype \
               $(OOPS_LIBCXX_INCLUDE) $(OOPS_POSIX_INCLUDE) \
               $(SOH_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

# --- compile-survey ------------------------------------------------------------
#
#   make compile-survey            the whole port
#   make compile-survey SOH_SURVEY_DIR=upstream/libultraship/src   one tree
#
# Every source on its own with `-fsyntax-only`, the failures grouped by their first error,
# since one missing header causes the rest.
#
# It does not link, and a payload link does not report unresolved symbols, so each archive
# also needs an `nm --undefined-only` check.
SOH_SURVEY_DIR ?= upstream/libultraship/src upstream/soh

# The sources upstream selects out per platform, at `soh/CMakeLists.txt:147-165`. Each speech
# synthesizer wants a system library this target has no equivalent of - SAPI is Windows, Darwin is
# macOS, ESpeak is a Linux package - and the extractor is a host tool rather than payload code.
# Crowd control needs a socket server. The same list drives the survey and the payload.
SOH_EXCLUDE_RE := speechsynthesizer/(SAPI|Darwin|ESpeak)|soh/Extractor/|Enhancements/crowd-control/

.PHONY: compile-survey
compile-survey: $(SOH_CVARS_H)
	@ok=0; bad=0; : > $(CURDIR)/build/survey-errors.txt; \
	 for f in $$(find $(SOH_SURVEY_DIR) -name '*.cpp' | grep -vE '$(SOH_EXCLUDE_RE)' | sort); do \
	    if $(TARGET_CXX) $(SOH_CXXFLAGS) -fsyntax-only "$$f" 2>$(CURDIR)/build/survey-e.txt; then \
	        ok=$$((ok+1)); \
	    else \
	        bad=$$((bad+1)); \
	        printf '%s\t%s\n' "$$f" \
	          "$$(grep -a -m1 'error:' $(CURDIR)/build/survey-e.txt \
	              | sed 's|.*error: ||' | cut -c1-90)" \
	          >> $(CURDIR)/build/survey-errors.txt; \
	    fi; \
	 done; \
	 printf '\n%s: %s compile, %s fail\n' "$(SOH_SURVEY_DIR)" "$$ok" "$$bad"; \
	 if [ "$$bad" -gt 0 ]; then \
	    printf '\n-- blockers, by first error, most files first --\n'; \
	    cut -f2 $(CURDIR)/build/survey-errors.txt | sort | uniq -c | sort -rn | head -25; \
	    printf '\n-- the files, in build/survey-errors.txt --\n'; \
	    head -25 $(CURDIR)/build/survey-errors.txt; \
	 fi

.PHONY: survey
survey:
	@echo "ship-of-harkinian: upstream $(UPSTREAM_REF) @ $(UPSTREAM_REV)"
	@printf '\n-- submodules\n'
	@for m in libultraship ZAPDTR OTRExporter; do \
	    if [ -n "$$(ls -A upstream/$$m 2>/dev/null)" ]; then s=present; else s="MISSING"; fi; \
	    printf '   %-14s %s\n' "$$m" "$$s"; \
	done
	@printf '\n-- source volume to compile\n'
	@for p in soh/soh libultraship/src ZAPDTR OTRExporter; do \
	    printf '   %-18s c:%-5s cpp:%-5s\n' "$$p" \
	        "$$(find upstream/$$p -name '*.c' 2>/dev/null | wc -l)" \
	        "$$(find upstream/$$p -name '*.cpp' 2>/dev/null | wc -l)"; \
	done
	@printf '\n-- GL entry points oops-gl still lacks\n'
	@gl=$(OOPS_SDK_DIR)/src/gl; missing=0; \
	 for s in $(SOH_GL_GAP); do \
	    if grep -rqE "^[A-Za-z_][A-Za-z0-9_ *]*\b$$s\(" $$gl/*.c 2>/dev/null; then \
	        printf '   %-34s defined\n' "$$s"; \
	    else \
	        printf '   %-34s MISSING\n' "$$s"; missing=$$((missing+1)); \
	    fi; \
	 done; printf '   -> %s of %s still to add\n' "$$missing" "$(words $(SOH_GL_GAP))"
	@printf '\n-- dependencies\n'
	@have=0; need=0; \
	 for d in $(SOH_DEPS); do \
	    if [ -d $(OOPS_APPS_ROOT)/src/oops-deps/$$d ]; then \
	        printf '   %-18s vendored\n' "$$d"; have=$$((have+1)); \
	    else \
	        printf '   %-18s to vendor\n' "$$d"; need=$$((need+1)); \
	    fi; \
	 done; printf '   -> %s vendored, %s to go\n' "$$have" "$$need"

# --- the payload ------------------------------------------------------------
#
# libultraship has no build here, so the payload is behind SOH_ARMED. app.env carries
# FORMATS=title and app.mk is included either way, so `make title` is a real target and only
# the source lists are conditional.
#
# main is at upstream/soh/src/code/main.c:60 in a C translation unit, so the shim is C. A
# freestanding C++ main is mangled and a C shim misses it silently, the payload link ignoring
# unresolved symbols.
ifeq ($(SOH_ARMED),1)
OOPS_CXX_EXCEPTIONS = 1
OOPS_CXX_STD        = c++20
OOPS_CXX_SRCS := $(sort $(wildcard $(CURDIR)/upstream/soh/soh/*.cpp) \
                        $(wildcard $(CURDIR)/upstream/libultraship/src/*/*.cpp))
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk
# libc++ owns new and delete for a title linking the full archive; cxxrt's copy sits outside
# libc++'s __lcxx_override section and fails the guard its new carries.
EXTRA_TARGET_CFLAGS += -DOOPS_CXX_EXTERNAL_NEW_DELETE
include $(OOPS_APPS_ROOT)/common/cxx.mk
OOPS_CXX_INCLUDE += $(OOPS_LIBCXX_INCLUDE) $(SOH_INCLUDE) $(SOH_GEN_INCLUDE)
PAYLOAD_SRCS := shim/soh_start.c $(OOPS_POSIX_SRCS)
EXTRA_TARGET_LDFLAGS += $(OOPS_CXX_LDFLAGS) $(OOPS_SDL_LDFLAGS) \
                        $(OOPS_LIBCXX_LDFLAGS) $(OOPS_LIBCXXABI_LDFLAGS) \
                        $(OOPS_LIBUNWIND_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(OOPS_CXX_LIB) $(OOPS_SDL_LIB) \
                        $(OOPS_LIBCXX_LIB) $(OOPS_LIBCXXABI_LIB) $(OOPS_LIBUNWIND_LIB)
endif

EXTRA_TARGET_CFLAGS += $(SOH_INCLUDE) $(SOH_GEN_INCLUDE)
OOPS_FEATURES := display input keyboard mouse audio suspend thread draw

include $(OOPS_APPS_ROOT)/common/app.mk

# The survey is the default until the payload links.
.DEFAULT_GOAL := survey
