# Ship of Harkinian, through oops-sdk.
#
#   make survey   what this port still needs, measured against the tree that is checked out
#   make elf      the payload - NOT YET, see the state section of README.md
#
# **This title does not build yet, and the Makefile says so rather than pretending.** The fetch
# and the pin work; the sources do not compile against this toolchain until the SDK gains vertex
# array objects and the dependencies below are vendored. `make survey` is what is useful today:
# it measures the gap against the checked-out tree, so the number moves as the work lands instead
# of being re-guessed.
#
# The order of work is in README.md. Read it before adding to this file.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# --- the toolchain, because this title does not include `common/app.mk` yet ------
#
# `app.mk` is what normally defines these, and a title that has no `elf` rule does not include it.
# So they are defined here, with the same defaults `app.mk` uses.
#
# **This was not a formality.** The `survey` target below already named `$(OOPS_SDK_DIR)`, which
# expanded to nothing, so its GL check globbed `/src/gl/*.c`, matched nothing, and printed MISSING
# for every symbol - including `glBlitFramebuffer`, which `oops-gl` has had since 2026-09-25. An
# arm that reports the same answer whether or not the thing is there is not a check, and this one
# went unquestioned because it agreed with what was expected.
OOPS_SDK ?= $(abspath $(OOPS_APPS_ROOT)/../oops-sdk)
OOPS_SDK_DIR := $(abspath $(OOPS_SDK))
OOPS_SDK_INCLUDE ?= -I$(OOPS_SDK_DIR)/include -I$(OOPS_SDK_DIR) -DOOPS_TARGET=3
OOPS_SDK_LIBC_INCLUDE ?= -I$(OOPS_SDK_DIR)/include/libc
TARGET_CC ?= clang
TARGET_CXX ?= clang++
AR ?= ar

# The fetch runs while this file is read - see `common/upstream.mk`. It is included explicitly
# because the survey below names files under `upstream/`, and a title that does that has to ask
# for the fetch before the lists that use it.
include $(OOPS_APPS_ROOT)/common/upstream.mk

# --- what is still missing --------------------------------------------------
#
# **The GL gap, and it is the SDK's rather than this title's.**
#
# `libultraship`'s `gfx_opengl.cpp` calls 54 entry points and `oops-gl` defines all but the first
# two below. The vertex-array family is *not* from that file - it sits there inside
# `#if defined(__APPLE__) || defined(USE_OPENGLES)`, which this port does not compile. It is here
# because **ImGui's GL3 backend uses vertex array objects unconditionally on desktop GL**, and
# libultraship links that backend. Grepping the port measured the port; compiling the link
# measured the link. See README.md.
#
# `glRenderbufferStorageMultisample` is further behind a runtime `msaa_level`, so the one that is
# unconditionally load-bearing from `gfx_opengl.cpp` is `glBlitFramebuffer`.
SOH_GL_GAP := glBlitFramebuffer glRenderbufferStorageMultisample \
              glGenVertexArrays glBindVertexArray glDeleteVertexArrays glGetStringi

# The third-party libraries upstream fetches, minus the Apple-only one. A name that already has a
# directory under `src/oops-deps/` is done; the rest are the vendoring queue.
#
# `imgui` was missing from this list until a compile survey found it: it is declared and used
# inside the `libultraship` submodule, so the grep that built this list - run over the
# superproject - could not see it.
SOH_DEPS := sdl2 zlib libzip stb nlohmann-json thread-pool tinyxml2 spdlog \
            stormlib libgfxd prism-processor imgui

# --- generated headers ------------------------------------------------------
#
# CMake hands libultraship two things this build has to produce another way. Both are generated
# rather than checked in, for the reason each script gives: they are derived from files upstream
# owns, and a stale copy of either is wrong in a way that compiles.
#
# `ship/install_config.h` is the exception - it is a *decision* rather than a derivation, so it
# lives in `shim/include/` as ordinary source.
# **`-include`, because upstream delivers these on the command line.** CMake passes the CVAR names
# as `add_compile_definitions`, so no source includes a header for them - `Controller.cpp:69`
# simply writes `CVAR_PREFIX_CONTROLLERS ".Port%d.HasConfig"` and expects the macro to be there.
# Reproducing that as twenty-two `-D` flags would mean quoting each value through make and the
# shell; one force-include of a generated header is the same thing with the quoting done once, in
# C, where it belongs.
# --- what the build selects ---------------------------------------------------
#
# `ENABLE_OPENGL` is how libultraship picks a renderer. Its CMake sets one of `ENABLE_OPENGL`,
# `ENABLE_DX11`, `ENABLE_DX12` or the Metal path, and `Gui.cpp` includes the matching ImGui
# backend behind that `#ifdef`. Without it the file compiles right up to
# `ImGui_ImplSDL2_InitForOpenGL` and then does not know what that is - which reads like a missing
# header and is really an unselected renderer.
#
# There is one renderer on this target, so this is not a choice so much as a statement.
SOH_DEFS := -DENABLE_OPENGL

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
# Every dependency's build file, for the include variables each exports. This is the same set the
# eventual `elf` rule needs, assembled here first because `compile-survey` below is what proves it
# is right - a missing `-I` shows up as a file that does not compile, named, rather than as a
# link-time surprise.
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
include $(OOPS_APPS_ROOT)/common/posix.mk
include $(OOPS_APPS_ROOT)/common/rt.mk

# The port's own headers, taken from `soh/CMakeLists.txt`'s `target_include_directories` and
# `libultraship/src/CMakeLists.txt` rather than inferred.
#
# **Inferring them cost a survey.** A first pass guessed `upstream/` and `upstream/soh`, and 340 of
# `soh/soh`'s 401 sources failed on `z64.h`, `z64item.h`, `variables.h` and `global.h` - every one
# of which is in `soh/include/`, which was not on the list. That reads as a port with a vast gap
# and is a harness with a missing `-I`. The rule this keeps re-teaching: read the build system's
# own list, do not reconstruct it.
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
               $(OOPS_THREADPOOL_INCLUDE) $(OOPS_IMGUI_INCLUDE) \
               $(OOPS_STORMLIB_INCLUDE) $(OOPS_LIBZIP_INCLUDE) $(OOPS_ZLIB_INCLUDE) \
               $(OOPS_SDL_INCLUDE) $(OOPS_SDL_PREFIX_INCLUDE)

# `$(OOPS_LIBCXX_INCLUDE)` before the C headers - libc++ ships its own `<math.h>` and `<stdlib.h>`
# and `<cmath>` stops with an explicit error if it reaches the C ones first. It carries
# `-nostdinc++ -nostdlibinc` itself.
#
# **`$(OOPS_POSIX_INCLUDE)` comes before `$(SOH_INCLUDE)`, and that is not cosmetic.** Both it and
# `oops-deps/sdl2/include` contain a `unistd.h`. SDL's exists only to declare `_exit` for its own
# `SDL.c`; the shim's declares the whole set a port uses. With SDL's directory first, spdlog's
# `os-inl.h:395` calls `::getpid()` and there is no such declaration - nine files in one
# subdirectory failed that way, and the error names `getpid` rather than the header, so nothing
# points at the include path. The shim's header carries a comment about this hazard from the last
# time it bit, in the other direction.
SOH_CXXFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib -fPIC \
               -O2 -w -std=c++20 -D__PROSPERO__=1 \
               $(OOPS_LIBCXX_INCLUDE) $(OOPS_POSIX_INCLUDE) \
               $(SOH_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

# --- compile-survey ------------------------------------------------------------
#
#   make compile-survey            the whole port
#   make compile-survey SOH_SURVEY_DIR=upstream/libultraship/src   one tree
#
# Every source on its own, `-fsyntax-only`, counted - and the failures grouped by their **first**
# error, because one missing header produces a hundred of the rest and the question here is which
# blockers exist rather than how many errors they cause. That grouping is what turned 83 failures
# into "SDL2 prefix" and "ImGui" in one pass.
#
# **A clean survey is not a working port.** `-fsyntax-only` does not link, and a payload link does
# not report an unresolved symbol - so a name that no object defines becomes a jump into nothing on
# the console rather than an error here. The archives each get an `nm --undefined-only` census for
# that reason; this instrument answers a narrower question than it looks like it does.
SOH_SURVEY_DIR ?= upstream/libultraship/src upstream/soh

.PHONY: compile-survey
compile-survey: $(SOH_CVARS_H)
	@ok=0; bad=0; : > $(CURDIR)/build/survey-errors.txt; \
	 for f in $$(find $(SOH_SURVEY_DIR) -name '*.cpp' | sort); do \
	    if $(TARGET_CXX) $(SOH_CXXFLAGS) -fsyntax-only "$$f" 2>$(CURDIR)/build/survey-e.txt; then \
	        ok=$$((ok+1)); \
	    else \
	        bad=$$((bad+1)); \
	        grep -m1 'error:' $(CURDIR)/build/survey-e.txt \
	          | sed 's|.*error: ||' | cut -c1-90 >> $(CURDIR)/build/survey-errors.txt; \
	    fi; \
	 done; \
	 printf '\n%s: %s compile, %s fail\n' "$(SOH_SURVEY_DIR)" "$$ok" "$$bad"; \
	 if [ "$$bad" -gt 0 ]; then \
	    printf '\n-- blockers, by first error, most files first --\n'; \
	    sort $(CURDIR)/build/survey-errors.txt | uniq -c | sort -rn | head -25; \
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

# `make` with no target should say what the state is rather than fail obscurely.
.DEFAULT_GOAL := survey
