# 2 Ship 2 Harkinian, through oops-sdk.
#
#   make survey          the GL entry points and dependencies the checked-out tree needs
#   make compile-survey  compile every source with -fsyntax-only, grouped by first error
#   make generated       the generated headers
#
# The port plan is in README.md.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# The entry symbol in shim/2s2h_start.c. Overridden because app.mk's default
# `2_ship_2_harkinian_start` begins with a digit, which is not a valid C identifier.
ENTRY_POINT := two_ship_two_harkinian_start

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
# The CVAR names are generated from upstream's CMake files by `common/gen-lus-cvars.sh`, so they
# follow upstream. `ship/install_config.h` is a fixed choice and lives in `shim/include/`.
#
# CMake passes the CVAR names as `add_compile_definitions` and no source includes a header for
# them (`Controller.cpp:69` uses `CVAR_PREFIX_CONTROLLERS` directly), so the generated header
# is force-included with `-include`, which avoids quoting each value through make and the
# shell.
#
# `ENABLE_OPENGL` selects libultraship's renderer; `Gui.cpp` includes the matching ImGui
# backend behind it. OpenGL is the only renderer on this target.
#
# `F3DEX_GBI_2` is the microcode Majora's Mask uses, set by upstream at `CMakeLists.txt:159` and
# in each of 2s2h's target definition blocks. Without it `gbi.h` defines neither `G_TRI2` nor
# `G_GEOMETRYMODE`, which the display-list viewer reads.
#
# The three spdlog and stb settings are upstream's own, and they matter here rather than being
# inherited noise: this platform has no thread-local storage for them to use.
#
# The three dr_libs headers open wide paths through `wcsrtombs` to reach `fopen`, and this platform
# has no wide-character functions: libc++ is built without them and `<wchar.h>` omits them
# deliberately. Samples come out of the archive rather than a file, so the file APIs are not wanted
# either, and `DR_*_NO_STDIO` removes both. dr_wav needs `NO_WCHAR` named as well; dr_flac says in
# its own header that `NO_WCHAR` is redundant once `NO_STDIO` is set.
SOH_DR_DEFS := -DDR_WAV_NO_STDIO -DDR_WAV_NO_WCHAR -DDR_MP3_NO_STDIO -DDR_FLAC_NO_STDIO

#
# `LOG_LEVEL_GAME_PRINTS` is what the game's own `osSyncPrintf` compiles down to, and upstream sets
# it to spdlog's `SPDLOG_LEVEL_OFF` in every one of its target blocks. 6 is that value, from
# spdlog's `common.h`.
#
# `OOPS_POSIX_HOME` is where the shim points `HOME` and `getpwuid`, and libultraship resolves its
# configuration, saves and the generated archive against it.
SOH_DEFS := -DENABLE_OPENGL -DF3DEX_GBI_2 -DNDEBUG -DLOG_LEVEL_GAME_PRINTS=6 $(SOH_DR_DEFS) \
            -DOOPS_POSIX_HOME=\"/app0\" \
            -DSPDLOG_NO_THREAD_ID -DSPDLOG_NO_TLS -DSTBI_NO_THREAD_LOCALS

SOH_GEN_DIR := $(CURDIR)/build/gen
SOH_CVARS_H := $(SOH_GEN_DIR)/ship/cvars_generated.h
SOH_GEN_INCLUDE := -I$(SOH_GEN_DIR) -I$(CURDIR)/shim/include \
                   -include ship/cvars_generated.h -include 2s2h_pch.h

# The three files in the order CMake reads them, first set() winning. `--expect` asserts the one
# value that differs between them: `gSettings.Controllers` from the superproject and `gControllers`
# from libultraship's defaults, both of which compile and run.
#
# Generated while this file is read, as `common/upstream.mk` runs its fetch, and for the same kind of
# reason: the header is force-included into every object, and app.mk's object rules take their
# prerequisites from `-MMD`, which cannot name a file that does not exist yet. The generator leaves
# the file alone when the content has not changed, so running it every invocation rebuilds nothing.
$(shell sh $(OOPS_APPS_ROOT)/common/gen-lus-cvars.sh \
            --expect CVAR_PREFIX_CONTROLLERS=gSettings.Controllers \
            $(SOH_CVARS_H) OOPS_SOH_CVARS_GENERATED_H \
            $(CURDIR)/upstream/CMake/2ship-cvars.cmake \
            $(CURDIR)/upstream/CMake/lus-cvars.cmake \
            $(CURDIR)/upstream/libultraship/cmake/cvars.cmake >&2)

# `src/boot/build.c`, the other file CMake generates (`mm/CMakeLists.txt:105`): the version and
# commit strings the crash handler, the gameplay-stats window and the randomizer's spoiler log print.
# The lock's own revision and tag are the answer `git describe` would give, without needing git in
# the container, so they go in verbatim. The branch is empty because a detached checkout at a commit
# is not on one.
SOH_VER_PARTS := $(subst ., ,$(UPSTREAM_REF))
SOH_BUILD_C := $(SOH_GEN_DIR)/build.c

$(SOH_BUILD_C): upstream.lock $(CURDIR)/Makefile
	@mkdir -p $(@D)
	@{ printf '/* Generated from upstream/mm/src/boot/build.c.in; see the Makefile. */\n'; \
	   printf '#include <libultraship/libultra.h>\n\n'; \
	   printf 'const char gBuildVersion[] = "Battler Bravo (%s)";\n' "$(UPSTREAM_REF)"; \
	   printf 'const u16 gBuildVersionMajor = %s;\n' "$(word 1,$(SOH_VER_PARTS))"; \
	   printf 'const u16 gBuildVersionMinor = %s;\n' "$(word 2,$(SOH_VER_PARTS))"; \
	   printf 'const u16 gBuildVersionPatch = %s;\n\n' "$(word 3,$(SOH_VER_PARTS))"; \
	   printf 'const char gGitBranch[] = "";\n'; \
	   printf 'const char gGitCommitHash[] = "%s";\n' "$(UPSTREAM_REV)"; \
	   printf 'const char gGitCommitTag[] = "%s";\n\n' "$(UPSTREAM_REF)"; \
	   printf 'const char gBuildTeam[] = "github.com/harbourmasters";\n'; \
	   printf 'const char gBuildDate[] = __DATE__ " " __TIME__;\n'; \
	   printf 'const char gBuildMakeOption[] = "";\n'; \
	 } > $@
	@echo "2s2h: generated $(notdir $@)"

# --- the include set -----------------------------------------------------------
#
# Every dependency's build file, for the include variables each exports.
#
# `OOPS_CXX_EXCEPTIONS` is set here rather than in the armed block below: `oops-libcxx.mk` reads it
# at include time to choose between `build-eh/libc++.a` and `build/libc++.a`, and the second one
# carries its own `std::terminate` and `std::set_new_handler` for a build with no libc++abi to
# provide them, which collide with libc++abi's when both are on the link line. Upstream throws and
# catches, so it is the exceptions archive either way.
OOPS_CXX_EXCEPTIONS = 1
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
# ZAPD writes textures out as PNG while it converts a ROM, and is the only reason this
# title needs libpng; nothing in the game itself reads one.
OOPS_PNG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libpng)
include $(OOPS_PNG)/oops-libpng.mk
OOPS_STB ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/stb)
include $(OOPS_STB)/oops-stb.mk
OOPS_JSON ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/nlohmann-json)
include $(OOPS_JSON)/oops-nlohmann-json.mk
OOPS_THREADPOOL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/thread-pool)
include $(OOPS_THREADPOOL)/oops-thread-pool.mk
# Header-only, and pinned at the commit 2s2h's own FetchContent_Declare names.
# `AudioSampleFactory.cpp` defines DR_WAV_IMPLEMENTATION and compiles it there.
OOPS_DR_LIBS ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/dr-libs)
include $(OOPS_DR_LIBS)/oops-dr-libs.mk
# The sample importer reads Vorbis streams beside the WAV ones. libvorbis carries libogg's include.
OOPS_VORBIS ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libvorbis)
include $(OOPS_VORBIS)/oops-libvorbis.mk
# Audio decodes Opus through opusfile, which upstream links unconditionally. opusfile
# pulls in opus and libogg itself.
OOPS_OPUSFILE ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/opusfile)
include $(OOPS_OPUSFILE)/oops-opusfile.mk
OOPS_MONOCYPHER ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/monocypher)
include $(OOPS_MONOCYPHER)/oops-monocypher.mk
include $(OOPS_APPS_ROOT)/common/posix.mk
include $(OOPS_APPS_ROOT)/common/rt.mk

# The port's own headers, taken from `mm/CMakeLists.txt`'s `target_include_directories` and
# `libultraship/src/CMakeLists.txt`.
SOH_OWN_INCLUDE := -I$(CURDIR)/upstream \
                   -I$(CURDIR)/upstream/libultraship/src \
                   -I$(CURDIR)/upstream/libultraship/include \
                   -I$(CURDIR)/upstream/mm \
                   -I$(CURDIR)/upstream/mm/include \
                   -I$(CURDIR)/upstream/mm/include/PR \
                   -I$(CURDIR)/upstream/mm/src \
                   -I$(CURDIR)/upstream/mm/2s2h \
                   -I$(CURDIR)/upstream/mm/assets \
                   -I$(CURDIR)/upstream/ZAPDTR/ZAPD/resource/type

SOH_INCLUDE := $(SOH_OWN_INCLUDE) $(SOH_GEN_INCLUDE) $(SOH_DEFS) \
               $(OOPS_SPDLOG_INCLUDE) $(OOPS_JSON_INCLUDE) $(OOPS_TINYXML2_INCLUDE) \
               $(OOPS_PRISM_INCLUDE) $(OOPS_GFXD_INCLUDE) $(OOPS_STB_INCLUDE) \
               $(OOPS_THREADPOOL_INCLUDE) $(OOPS_IMGUI_INCLUDE) $(OOPS_DR_LIBS_INCLUDE) \
               $(OOPS_VORBIS_INCLUDE) $(OOPS_OPUSFILE_INCLUDE) \
               $(OOPS_STORMLIB_INCLUDE) $(OOPS_LIBZIP_INCLUDE) $(OOPS_ZLIB_INCLUDE) \
               $(OOPS_SDL_INCLUDE) $(OOPS_SDL_PREFIX_INCLUDE) \
               $(OOPS_MONOCYPHER_INCLUDE) $(OOPS_PNG_INCLUDE)

# `$(OOPS_LIBCXX_INCLUDE)` before the C headers - libc++ ships its own `<math.h>` and `<stdlib.h>`
# and `<cmath>` stops with an explicit error if it reaches the C ones first. It carries
# `-nostdinc++ -nostdlibinc` itself.
#
# `$(OOPS_POSIX_INCLUDE)` comes before `$(SOH_INCLUDE)`: both it and `oops-deps/sdl2/include`
# have a `unistd.h`, and SDL's declares only `_exit`. spdlog's `os-inl.h:395` needs the
# shim's `getpid`.
# `-Wno-c++11-narrowing` on top of `-w`, because narrowing in an initialiser list is a default
# *error* rather than a warning, so `-w` does not reach it.
#
# `-include cctype` for the sources that call `std::toupper` without including it: libstdc++
# reaches it transitively and libc++ does not.
# The survey compiles with these and the build compiles with `OOPS_CXX_FLAGS`, so the include order
# below and the one `OOPS_CXX_INCLUDE` is given in the armed block are kept the same.
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
SOH_SURVEY_DIR ?= upstream/libultraship/src upstream/mm

# The sources upstream selects out per platform. Speech synthesizer wants system libraries
# this target has no equivalent of - SAPI is Windows, Darwin is macOS, ESpeak is Linux.
# Crowd control needs a socket server.
#
# `src/libultra/*` is excluded completely as upstream does in `mm/CMakeLists.txt:143`.
#
# `libultraship/libultra/os_pi.cpp` is excluded because 2s2h provides its own stubs.
#
# `src/boot/build.c` is excluded because this build generates its own (`SOH_BUILD_C`, above).
SOH_EXCLUDE_RE := speechsynthesizer/(SAPI|Darwin|ESpeak)|Enhancements/crowd-control/|src/boot/build\.c|src/dmadata/|src/elf_message/|src/libultra/|libultraship/libultra/os_pi\.cpp|libultraship/src/ship/(scripting|security)/

.PHONY: compile-survey
compile-survey:
	@# One compiler per core rather than one at a time: this is several hundred translation units
	@# and a serial pass takes long enough that it is easier to guess than to measure, which is the
	@# opposite of what a survey is for. Each writes its own error file, so the results do not
	@# interleave, and `set -e` is off because a failing compile is the thing being counted.
	@rm -rf $(CURDIR)/build/survey-out; mkdir -p $(CURDIR)/build/survey-out; \
	 : > $(CURDIR)/build/survey-errors.txt; \
	 find $(SOH_SURVEY_DIR) -name '*.cpp' | grep -vE '$(SOH_EXCLUDE_RE)' | sort \
	   > $(CURDIR)/build/survey-files.txt; \
	 total=$$(wc -l < $(CURDIR)/build/survey-files.txt); \
	 cat $(CURDIR)/build/survey-files.txt \
	 | xargs -P $$(nproc) -I{} sh -c \
	     'e=$(CURDIR)/build/survey-out/$$(echo "{}" | tr "/" "_").err; \
	      if ! $(TARGET_CXX) $(SOH_CXXFLAGS) -fsyntax-only "{}" 2>"$$e"; then \
	          printf "%s\t%s\n" "{}" \
	            "$$(grep -a -m1 "error:" "$$e" | sed "s|.*error: ||" | cut -c1-90)"; \
	      fi' >> $(CURDIR)/build/survey-errors.txt; \
	 bad=$$(wc -l < $(CURDIR)/build/survey-errors.txt); ok=$$((total - bad)); \
	 printf '\n%s: %s compile, %s fail\n' "$(SOH_SURVEY_DIR)" "$$ok" "$$bad"; \
	 if [ "$$bad" -gt 0 ]; then \
	    printf '\n-- blockers, by first error, most files first --\n'; \
	    cut -f2 $(CURDIR)/build/survey-errors.txt | sort | uniq -c | sort -rn | head -25; \
	    printf '\n-- the files, in build/survey-errors.txt --\n'; \
	    head -25 $(CURDIR)/build/survey-errors.txt; \
	 fi

.PHONY: survey
survey:
	@echo "2-ship-2-harkinian: upstream $(UPSTREAM_REF) @ $(UPSTREAM_REV)"
	@printf '\n-- submodules\n'
	@for m in libultraship ZAPDTR OTRExporter; do \
	    if [ -n "$$(ls -A upstream/$$m 2>/dev/null)" ]; then s=present; else s="MISSING"; fi; \
	    printf '   %-14s %s\n' "$$m" "$$s"; \
	done
	@printf '\n-- source volume to compile\n'
	@for p in mm/2s2h mm/src libultraship/src ZAPDTR OTRExporter; do \
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
# main is at upstream/mm/src/code/main.c in a C translation unit, so the shim is C.
# The lists are recursive and filtered by the same regex the survey uses, so what compiles and what
# links are the same set. Deferred (`=`) because the generated cvars header has to exist first.
SOH_CXX_ALL = $(shell find $(CURDIR)/upstream/mm/2s2h $(CURDIR)/upstream/libultraship/src \
                        -name '*.cpp' 2>/dev/null | grep -vE '$(SOH_EXCLUDE_RE)' | sort)

# ---------------------------------------------------------------------------
# ZAPD and OTRExporter: converting a ROM into `mm.o2r` on the device
#
# The one flag they need is `-DGAME_MM`: OTRExporter is shared with Ocarina of Time
# and selects its resource headers on `GAME_MM`/`GAME_OOT`.
#
# `CrashHandler.cpp` is the one exclusion. It wants `execinfo.h`, which is a desktop
# backtrace header, and its handler would install itself over `common/crashtrace.c`'s.
# `shim/zapd_stubs.cpp` answers the one symbol `Main.cpp` calls.
# ---------------------------------------------------------------------------
#
# `Utils/StringHelper.cpp` is excluded too: ZAPD and libultraship each vendor a copy of
# the same global `StringHelper`, and both on the link line is 18 duplicate symbols.
SOH_ZAPD_EXCLUDE_RE := ZAPD/CrashHandler\.cpp|ZAPD/Utils/StringHelper\.cpp

SOH_ZAPD_ALL = $(shell find $(CURDIR)/upstream/ZAPDTR/ZAPD \
                        $(CURDIR)/upstream/OTRExporter/OTRExporter \
                        -name '*.cpp' 2>/dev/null | grep -vE '$(SOH_ZAPD_EXCLUDE_RE)' | sort)

SOH_ZAPD_INCLUDE := -DGAME_MM \
                    -I$(CURDIR)/upstream/ZAPDTR/ZAPD \
                    -I$(CURDIR)/upstream/ZAPDTR/lib/libgfxd \
                    -I$(CURDIR)/upstream/ZAPDTR/lib/tinyxml2 \
                    -I$(CURDIR)/upstream/OTRExporter/OTRExporter \
                    -I$(CURDIR)/upstream/OTRExporter \
                    -I$(CURDIR)/upstream/libultraship/extern \
                    $(OOPS_PNG_INCLUDE)

SOH_C_ALL   = $(shell find $(CURDIR)/upstream/mm/src $(CURDIR)/upstream/mm/2s2h \
                        $(CURDIR)/upstream/libultraship/src \
                        -name '*.c' 2>/dev/null | grep -vE '$(SOH_EXCLUDE_RE)' | sort)

SOH_ARMED ?= 1
ifeq ($(SOH_ARMED),1)
OOPS_CXX_STD        = c++20
OOPS_CXX_SRCS = $(SOH_CXX_ALL) $(SOH_ZAPD_ALL) $(CURDIR)/shim/zapd_stubs.cpp
# Object names collide across the port's trees - ControlDeck, Controller, Menu and more appear in
# both 2s2h and libultraship - so the objects go on the link line rather than into an archive that
# would keep one of each. Set before the include, which is where `oops_ar_check` runs.
OOPS_CXX_LINK_OBJECTS = 1
OOPS_CXX_PCH = 0
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk
# libc++ owns new and delete for a title linking the full archive; cxxrt's copy sits outside
# libc++'s __lcxx_override section and fails the guard its new carries.
EXTRA_TARGET_CFLAGS += -DOOPS_CXX_EXTERNAL_NEW_DELETE
include $(OOPS_APPS_ROOT)/common/cxx.mk
# The same order `SOH_CXXFLAGS` gives the survey, so the two agree on which `unistd.h` wins.
OOPS_CXX_INCLUDE += $(OOPS_LIBCXX_INCLUDE) $(OOPS_POSIX_INCLUDE) \
                    $(SOH_INCLUDE) $(SOH_GEN_INCLUDE)
# Narrowing in an initialiser list is an error rather than a warning, and sources call `std::toupper`
# without including `<cctype>`, which libstdc++ reaches transitively and libc++ does not.
OOPS_CXX_FLAGS += -Wno-c++11-narrowing -include cctype

build/cxx/obj/app/upstream/ZAPDTR/%: OOPS_CXX_FLAGS += $(SOH_ZAPD_INCLUDE) -DZAPD_BUILD
build/cxx/obj/app/upstream/OTRExporter/%: OOPS_CXX_FLAGS += $(SOH_ZAPD_INCLUDE) -DZAPD_BUILD
build/cxx/obj/app/shim/zapd_stubs.o: OOPS_CXX_FLAGS += $(SOH_ZAPD_INCLUDE) -DZAPD_BUILD
PAYLOAD_SRCS = shim/2s2h_start.c $(SOH_BUILD_C) $(SOH_C_ALL) $(OOPS_POSIX_SRCS) \
               $(OOPS_APPS_ROOT)/common/crashtrace.c

# Each archive after everything that calls it. libultraship sits on top of all of them, so the
# order runs outward: the title's own C++, then the libraries it names, then the standard library.
EXTRA_TARGET_LDFLAGS += $(OOPS_CXX_LDFLAGS) $(OOPS_IMGUI_LDFLAGS) $(OOPS_STORMLIB_LDFLAGS) \
                        $(OOPS_LIBZIP_LDFLAGS) $(OOPS_TINYXML2_LDFLAGS) $(OOPS_SPDLOG_LDFLAGS) \
                        $(OOPS_PRISM_LDFLAGS) $(OOPS_GFXD_LDFLAGS) $(OOPS_STB_LDFLAGS) \
                        $(OOPS_VORBIS_LDFLAGS) $(OOPS_OPUSFILE_LDFLAGS) \
                        $(OOPS_PNG_LDFLAGS) $(OOPS_MONOCYPHER_LDFLAGS) \
                        $(OOPS_ZLIB_LDFLAGS) $(OOPS_SDL_LDFLAGS) \
                        $(OOPS_LIBCXX_LDFLAGS) $(OOPS_LIBCXXABI_LDFLAGS) \
                        $(OOPS_LIBUNWIND_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(OOPS_CXX_LIB) $(OOPS_IMGUI_LIB) $(OOPS_STORMLIB_LIB) \
                        $(OOPS_LIBZIP_LIB) $(OOPS_TINYXML2_LIB) $(OOPS_SPDLOG_LIB) \
                        $(OOPS_PRISM_LIB) $(OOPS_GFXD_LIB) $(OOPS_STB_LIB) \
                        $(OOPS_VORBIS_LIB) $(OOPS_OPUSFILE_LIB) $(OOPS_OPUS_LIB) \
                        $(OOPS_OGG_LIB) $(OOPS_PNG_LIB) $(OOPS_MONOCYPHER_LIB) \
                        $(OOPS_ZLIB_LIB) $(OOPS_SDL_LIB) \
                        $(OOPS_LIBCXX_LIB) $(OOPS_LIBCXXABI_LIB) $(OOPS_LIBUNWIND_LIB)
endif

# app.mk compiles at -Werror with a set stricter than a decompiled 2000 codebase meets.
SOH_WARNINGS := -Wno-deprecated-non-prototype -Wno-strict-prototypes -Wno-missing-prototypes \
                -Wno-conversion -Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-comment \
                -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-shadow \
                -Wno-sign-compare -Wno-char-subscripts -Wno-unused-function \
                -Wno-unused-const-variable -Wno-missing-field-initializers \
                -Wno-unused-variable -Wno-multichar -Wno-return-type -Wno-unknown-pragmas \
                -Wno-incompatible-pointer-types -Wno-macro-redefined -Wno-missing-braces \
                -Wno-logical-op-parentheses -Wno-extra-tokens -Wno-braced-scalar-init \
                -Wno-tautological-compare -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
                -Wno-format -Wno-self-assign -Wno-compare-distinct-pointer-types \
                -Wno-return-stack-address -Wno-int-conversion \
                -w -include strings.h \
                -include $(CURDIR)/upstream/mm/include/libc/math.h

EXTRA_TARGET_CFLAGS += $(OOPS_POSIX_INCLUDE) $(SOH_INCLUDE) $(SOH_GEN_INCLUDE)

OOPS_RENDERER := gl2
OOPS_FEATURES := display input keyboard mouse audio suspend thread draw dialog zip

include $(OOPS_APPS_ROOT)/common/app.mk

TARGET_CFLAGS += $(SOH_WARNINGS)

$(OOPS_PAYLOAD_OBJS): | $(OOPS_SDL_PREFIX_STAMP)
$(OOPS_CXX_OBJS): | $(OOPS_SDL_PREFIX_STAMP)

# ---------------------------------------------------------------------------
# `make package`: the title with the port's own archive beside it
#
# `2ship.o2r` is the port's assets - its fonts, its UI textures, libultraship's shaders -
# and libultraship refuses to start without it. It is NOT game data and needs NO ROM.
# Only `mm.o2r`, which the player converts from their own cartridge, needs one, and that
# stays a runtime question this build never asks at compile time.
# ---------------------------------------------------------------------------
SOH_O2R := $(CURDIR)/prebuilt/2ship.o2r

# ---------------------------------------------------------------------------
# `assets.zip`: what ZAPD reads while it converts the player's ROM
#
# ZAPD reads XML saying what is at which offset, one set per ROM version.
# `tools/make-assets-zip.py` packs the XML definitions so the device only has to unpack.
# ---------------------------------------------------------------------------
SOH_ASSETS_SRC := $(CURDIR)/upstream/mm/assets
SOH_ASSETS_ZIP := $(CURDIR)/build/assets.zip
SOH_ASSETS_TOOL := $(CURDIR)/tools/make-assets-zip.py
SOH_EXTRACT_CPP := $(CURDIR)/upstream/mm/2s2h/Extractor/Extract.cpp

SOH_PYTHON = $(shell for p in python3 python; do \
                 if $$p -c 'import zipfile' >/dev/null 2>&1; then echo $$p; break; fi; \
             done)

$(SOH_ASSETS_ZIP): $(SOH_ASSETS_TOOL) $(UPSTREAM_STAMP)
	@test -n "$(SOH_PYTHON)" || { \
	    echo "2-ship-2-harkinian: no python3 with zipfile, so no $(notdir $@)" >&2; \
	    echo "  it holds the XML ZAPD reads to convert a ROM; the build image has one" >&2; \
	    exit 1; }
	@mkdir -p $(dir $@)
	@$(SOH_PYTHON) $(SOH_ASSETS_TOOL) $(SOH_ASSETS_SRC) $(SOH_EXTRACT_CPP) $@

.PHONY: assets-zip
assets-zip: $(SOH_ASSETS_ZIP)

.PHONY: package
package: title $(SOH_ASSETS_ZIP)
	@test -f $(SOH_O2R) || { \
	    echo "2-ship-2-harkinian: no $(SOH_O2R)" >&2; \
	    echo "  it is the port's own assets and needs no ROM - see docs/PORTING.md" >&2; \
	    exit 1; }
	@# It is tracked rather than rebuilt, being the one thing here that needs a host ZAPD,
	@# so a lock bump would otherwise leave yesterday's archive shipping beside today's code.
	@test -n "$(SOH_PYTHON)" && $(SOH_PYTHON) $(CURDIR)/tools/check-o2r-version.py \
	    $(SOH_O2R) $(UPSTREAM_REF)
	@cp $(SOH_O2R) build/title/$(TITLE_ID)/2ship.o2r
	@cp $(SOH_ASSETS_ZIP) build/title/$(TITLE_ID)/assets.zip
	@# Both are zips; a truncated copy fails here rather than on the console, where it
	@# arrives as libultraship reporting a file missing.
	@for f in 2ship.o2r assets.zip; do \
	    head -c2 build/title/$(TITLE_ID)/$$f | grep -q PK || { \
	        echo "2-ship-2-harkinian: the staged $$f is not an archive" >&2; exit 1; }; \
	done
	@echo "2-ship-2-harkinian: package is build/title/$(TITLE_ID) ($$(du -sh build/title/$(TITLE_ID) | cut -f1), $$(find build/title/$(TITLE_ID) -type f | wc -l) files)"

# The survey is the default when there is no payload to build.
ifneq ($(SOH_ARMED),1)
.DEFAULT_GOAL := survey

ifeq ($(MAKELEVEL),0)
ifneq ($(filter title package,$(MAKECMDGOALS)),)
$(error the payload is behind SOH_ARMED: build it with `make SOH_ARMED=1 $(firstword $(filter title package,$(MAKECMDGOALS)))`)
endif
endif
endif
