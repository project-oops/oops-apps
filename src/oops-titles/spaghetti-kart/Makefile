# Spaghetti Kart, through oops-sdk.
#
#   make               the survey - what this port still needs before it can link
#   make census        compile every source this title needs, for the target, and report
#   make census-one F=<path> [X=1]   every error in one source, C or C++
#   make title         the payload, packaged - once SPGK_ARMED=1 is possible
#
# The payload is behind SPGK_ARMED: two dependencies are unvendored and libultraship has no build
# here, which `make survey` reports. app.env carries FORMATS=title and app.mk is included either way,
# so `make title` is a real target and only the source lists are conditional.
#
# ../ship-of-harkinian/README.md covers the shared runtime, shader dialect and dependency stack.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Before the app.mk include, because this file names files under OOPS_SDK_DIR below.
OOPS_SDK ?= $(abspath $(OOPS_APPS_ROOT)/../oops-sdk)
OOPS_SDK_DIR := $(abspath $(OOPS_SDK))

# A libultraship port makes no direct GL calls: the graphics are inside the library, which carries its
# own backends and generates shaders through prism-processor. So this names libultraship's backend
# rather than anything in src/.
OOPS_RENDERER = gl2

# First, because this file names files under upstream/ while it is being read.
include $(OOPS_APPS_ROOT)/common/upstream.mk

SPGK_UP := $(CURDIR)/upstream
SPGK_LUS := $(SPGK_UP)/libultraship

# --- the source list, transcribed from upstream's globs ---------------------
#
# CMakeLists.txt:237-295 is an explicit list of directories rather than one recursive glob, so this is
# transcribed: wildcarding over src/ would pick up trees upstream leaves out.
#
# src/os/ is ten named files, not the directory: upstream lists the gu matrix helpers and nothing else
# there.
SPGK_OS_SRCS := $(addprefix $(SPGK_UP)/src/os/, \
    guLookAtF.c guPerspectiveF.c guMtxF2L.c guRotateF.c guOrthoF.c guScaleF.c \
    guTranslateF.c guMtxCatF.c guMtxCatL.c guNormalize.c)

# Upstream's second glob is `GLOB_RECURSE`, so each root below is the whole tree under it, not the
# directory alone. Naming the directories one level deep missed `src/engine/editor`,
# `src/engine/cameras`, `src/engine/items`, `src/engine/registry`, `src/engine/vehicles`,
# `src/port/interpolation`, `src/port/audio`, `src/port/resource` and `src/port/ui` - ten trees whose
# symbols the link then had nothing for. `src/*.c` alone is a plain `GLOB`, so that one is not.
SPGK_C_ROOTS  := src/buffers src/data src/debug src/ending src/racing src/audio src/port src/mods \
                 src/enhancements src/engine/tracks assets/code courses
SPGK_CXX_ROOTS := src/port src/mods src/engine src/enhancements/freecam

# Two exclusions upstream's (CMakeLists.txt:298-299): `*.inc.c` files are `#include`d
# into other translation units rather than compiled, and `crash_screen_enhancement.c` is filtered out
# by name.
#
# The third is this build's: `src/port/GameExtractor.cpp` converts a ROM through torch, a host tool
# with its own dependency set that runs once on a desktop. `shim/GameExtractor.cpp` replaces it and
# says where the archive belongs - see the header there and `shim/include/Companion.h`.
SPGK_EXCLUDE := %.inc.c $(SPGK_UP)/src/debug/crash_screen_enhancement.c \
                $(SPGK_UP)/src/port/GameExtractor.cpp

SPGK_C_SRCS := $(filter-out $(SPGK_EXCLUDE), \
    $(sort $(SPGK_OS_SRCS) $(wildcard $(SPGK_UP)/src/*.c) \
           $(shell find $(addprefix $(SPGK_UP)/,$(SPGK_C_ROOTS)) -name '*.c' 2>/dev/null)))
SPGK_CXX_SRCS := $(filter-out $(SPGK_EXCLUDE), \
    $(sort $(shell find $(addprefix $(SPGK_UP)/,$(SPGK_CXX_ROOTS)) -name '*.cpp' 2>/dev/null)))

# libultraship is a submodule rather than an installed library, so its sources build with the title.
# `libultraship/src/CMakeLists.txt` adds its three trees whole, and each platform backend guards its
# own contents - `__APPLE__` for Metal, `ENABLE_DX11` for Direct3D, `_WIN32` for WASAPI, `__ANDROID__`
# for the mobile shim - so a recursive glob compiles exactly what this target wants and nothing needs
# excluding by name. `INCLUDE_MPQ_SUPPORT` stays undefined, which is what empties OtrArchive.cpp and
# is why StormLib is not among the dependencies.
SPGK_LUS_C_SRCS   := $(sort $(shell find $(SPGK_LUS)/src -name '*.c'))
SPGK_LUS_CXX_SRCS := $(sort $(shell find $(SPGK_LUS)/src -name '*.cpp'))

# --- what is still missing -------------------------------------------------
#
# `SPGK_DEPS` is checked by `make survey` against `src/oops-deps`, so the list stays honest rather
# than going stale in a comment. `prism` is vendored as `prism-processor`, which is why it is named
# that way here.
SPGK_DEPS := sdl2 sdl2-net zlib libzip stb nlohmann-json thread-pool tinyxml2 spdlog \
             imgui prism-processor libgfxd tomlplusplus semver libogg libvorbis dr-libs

include $(OOPS_APPS_ROOT)/common/posix.mk

# The two header-only libraries upstream fetches at configure time, vendored and pinned instead: a
# build here pins every dependency by commit, and toml++ came from a tag while semver came from a
# raw URL on a branch. sse2neon, the third, is only reached under __aarch64__; this target takes the
# <emmintrin.h> arm beside it.
OOPS_TOMLPP ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/tomlplusplus)
include $(OOPS_TOMLPP)/oops-tomlplusplus.mk
OOPS_SEMVER ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/semver)
include $(OOPS_SEMVER)/oops-semver.mk

# libultraship reads and writes its configuration as JSON, so its headers reach into this title.
OOPS_JSON ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/nlohmann-json)
include $(OOPS_JSON)/oops-nlohmann-json.mk
# libultraship reads its archives through libzip, which deflates through zlib, so zlib comes first -
# `oops-libzip.mk` reads `OOPS_ZLIB_INCLUDE` at include time.
OOPS_ZLIB ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/zlib)
include $(OOPS_ZLIB)/oops-zlib.mk
OOPS_LIBZIP ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libzip)
include $(OOPS_LIBZIP)/oops-libzip.mk
# libultraship dispatches resource loads across a pool, and its headers name BS_thread_pool.hpp.
OOPS_THREADPOOL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/thread-pool)
include $(OOPS_THREADPOOL)/oops-thread-pool.mk
# SDL2 for the window, the GL context and the audio device, as every other title here.
OOPS_SDL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2)
include $(OOPS_SDL)/oops-sdl.mk
# libultraship logs through spdlog, and its own headers include it.
OOPS_SPDLOG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/spdlog)
include $(OOPS_SPDLOG)/oops-spdlog.mk
# The resource system reads XML manifests, and prism generates its shaders.
OOPS_TINYXML2 ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/tinyxml2)
include $(OOPS_TINYXML2)/oops-tinyxml2.mk
OOPS_PRISM ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/prism-processor)
include $(OOPS_PRISM)/oops-prism.mk
OOPS_STB ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/stb)
include $(OOPS_STB)/oops-stb.mk
OOPS_IMGUI ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/imgui)
include $(OOPS_IMGUI)/oops-imgui.mk
# StormLib is absent on purpose: upstream sets EXCLUDE_MPQ_SUPPORT (CMakeLists.txt:396), which empties
# libultraship's `OtrArchive.cpp` and its header, so the game reads `.o2r` archives through libzip and
# nothing reaches an MPQ.
# dr_libs decodes the wav, mp3 and flac an archive can hold; header-only, fetched by upstream at
# `CMakeLists.txt:387-408`. Ogg and Vorbis are linked on every non-Windows target (line 494).
OOPS_DR_LIBS ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/dr-libs)
include $(OOPS_DR_LIBS)/oops-dr-libs.mk
OOPS_VORBIS ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libvorbis)
include $(OOPS_VORBIS)/oops-libvorbis.mk
OOPS_GFXD ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libgfxd)
include $(OOPS_GFXD)/oops-libgfxd.mk

# --- the definitions upstream's CMake supplies ------------------------------
#
# `CMakeLists.txt:192-203` for the first group: the game's own build switches, the ROM version it
# decompiles against and the F3DEX microcode revision. `EXCLUDE_MPQ_SUPPORT` (line 396) is what
# empties libultraship's MPQ archive reader. `ENABLE_OPENGL` (libultraship/src/CMakeLists.txt:171)
# selects the Fast3D GL backend, this target having no Direct3D or Metal.
#
# SPAGHETTI_VERSION is `git describe --tags` upstream; the lock's tag is the same answer without
# needing git in the container.
#
# The last four are this target's rather than upstream's: spdlog reads a thread id and keeps a
# thread-local registry, and stb keeps thread-local failure strings, none of which a single-threaded
# payload needs and each of which wants `__thread` support the loader does not relocate.
#
# `MA_USE_STDINT` makes miniaudio's `ma_uint64` the `uint64_t` from `<stdint.h>` rather than its own
# `unsigned long long`. Both are 64 bits, and in C++ they are still different types: `HMAS.cpp` passes
# a `uint64_t*` to `ma_sound_get_cursor_in_pcm_frames`, which without this takes the other one.
SPGK_DEFS := -DVERSION_US=1 -DENABLE_RUMBLE=1 -DF3DEX_GBI=1 -D_LANGUAGE_C \
             -D_USE_MATH_DEFINES -DCIMGUI_DEFINE_ENUMS_AND_STRUCTS \
             -DNON_MATCHING=1 -DNON_EQUIVALENT=1 -DAVOID_UB=1 \
             -DSPAGHETTI_VERSION=\"$(UPSTREAM_REF)\" \
             -DEXCLUDE_MPQ_SUPPORT -DENABLE_OPENGL \
             -DSPDLOG_NO_THREAD_ID -DSPDLOG_NO_TLS -DSTBI_NO_THREAD_LOCALS \
             -DMA_USE_STDINT

# --- generated headers ------------------------------------------------------
#
# libultraship takes its CVAR names as command-line macros that CMake supplies. `common/gen-lus-cvars.sh`
# reads them out of upstream's own cmake files and writes a header, force-included below, so an upstream
# bump carries the names with it. Spaghetti Kart overrides none of them, so libultraship's defaults are
# the whole list - unlike Ship of Harkinian, which prefixes them.
SPGK_GEN_DIR := $(CURDIR)/build/gen
SPGK_CVARS_H := $(SPGK_GEN_DIR)/ship/cvars_generated.h
SPGK_GEN_INCLUDE := -I$(SPGK_GEN_DIR) -include ship/cvars_generated.h

# Generated while this file is read, as `common/upstream.mk` runs its fetch, and for the same kind of
# reason: the header is force-included into every object, and app.mk's object rules take their
# prerequisites from `-MMD`, which cannot name a file that does not exist yet. The generator leaves
# the file alone when the content has not changed, so running it every invocation rebuilds nothing.
$(shell sh $(OOPS_APPS_ROOT)/common/gen-lus-cvars.sh $(SPGK_CVARS_H) \
            OOPS_SPGK_CVARS_GENERATED_H $(SPGK_LUS)/cmake/cvars.cmake >&2)

# `libultraship/include` rather than its `libultraship/` subdirectory: sources write
# `#include <libultraship/libultraship.h>` and `<libultraship/libultra/types.h>`.
# The port's own trees are on the path by name, not just their root, and the list below is upstream's
# own (`CMakeLists.txt:215-234` and `402-409`) rather than the subset the sources happened to need:
# `menu_items.c` includes `heap.h` and an `.inc.c` under `src/actors` includes `external.h`, both of
# which live in `src/audio`, and neither is reachable from a directory's own name.
# `$(OOPS_POSIX_INCLUDE)` first: both it and `oops-deps/sdl2/include` carry a `unistd.h`, and SDL's
# declares only `_exit`. spdlog's `os-inl.h` needs the shim's `getpid`, which 60 sources reached
# through it.
# `shim/include` before upstream's trees: it carries `ship/install_config.h`, which CMake generates,
# and a `Companion.h` that stands in for torch's - see the header there.
#
# `upstream/torch/src` is deliberately absent, so that a source needing more of torch than the `fs`
# alias fails here rather than pulling a host tool into the payload.
SPGK_INCLUDE := $(OOPS_POSIX_INCLUDE) \
                $(OOPS_TOMLPP_INCLUDE) $(OOPS_SEMVER_INCLUDE) $(OOPS_JSON_INCLUDE) \
                $(OOPS_LIBZIP_INCLUDE) $(OOPS_THREADPOOL_INCLUDE) \
                $(OOPS_SDL_INCLUDE) $(OOPS_SDL_PREFIX_INCLUDE) \
                $(OOPS_SPDLOG_INCLUDE) $(OOPS_TINYXML2_INCLUDE) $(OOPS_PRISM_INCLUDE) \
                $(OOPS_STB_INCLUDE) $(OOPS_IMGUI_INCLUDE) $(OOPS_ZLIB_INCLUDE) \
                $(OOPS_DR_LIBS_INCLUDE) $(OOPS_VORBIS_INCLUDE) \
                $(OOPS_GFXD_INCLUDE) $(SPGK_GEN_INCLUDE) $(SPGK_DEFS) \
                -I$(CURDIR)/shim/include \
                -I$(SPGK_UP) -I$(SPGK_UP)/include -I$(SPGK_UP)/include/assets \
                -I$(SPGK_UP)/src \
                -I$(SPGK_UP)/src/racing -I$(SPGK_UP)/src/ending -I$(SPGK_UP)/src/data \
                -I$(SPGK_UP)/src/buffers -I$(SPGK_UP)/src/audio -I$(SPGK_UP)/src/debug \
                -I$(SPGK_UP)/src/engine -I$(SPGK_UP)/src/engine/tracks \
                -I$(SPGK_UP)/src/engine/actors -I$(SPGK_UP)/src/engine/objects \
                -I$(SPGK_UP)/src/engine/sky -I$(SPGK_UP)/src/engine/particles \
                -I$(SPGK_UP)/src/enhancements -I$(SPGK_UP)/src/enhancements/freecam \
                -I$(SPGK_UP)/src/port \
                -I$(SPGK_LUS)/include -I$(SPGK_LUS)/include/libultraship \
                -I$(SPGK_LUS)/src \
                -DOOPS_POSIX_HOME=\"/data/spaghetti-kart\"

# `include/actor_types.h` writes `static_assert` in C without including <assert.h>, which is where
# C11 spells it. Upstream builds at C11 and gets the header some other way; forcing it in is
# cheaper than a patch, and it is where the macro belongs.
SPGK_INCLUDE += -include assert.h

SPGK_WARNINGS := -Wno-strict-prototypes -Wno-missing-prototypes -Wno-conversion \
                 -Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-comment \
                 -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-shadow \
                 -Wno-sign-compare -Wno-char-subscripts -Wno-unused-function \
                 -Wno-unused-const-variable -Wno-missing-field-initializers \
                 -Wno-unused-variable -Wno-multichar \
                 -Wno-macro-redefined -Wno-missing-braces -Wno-missing-declarations \
                 -Wno-unused-but-set-parameter -Wno-incompatible-pointer-types \
                 -Wno-return-type -Wno-unknown-pragmas \
                 -Wno-tautological-compare -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
                 -Wno-format -Wno-self-assign -Wno-compare-distinct-pointer-types \
                 -Wno-return-stack-address -Wno-int-conversion \
                 -Wno-deprecated-non-prototype -Wno-logical-op-parentheses \
                 -Wno-extra-tokens -Wno-braced-scalar-init \
                 -w

EXTRA_TARGET_CFLAGS += $(SPGK_INCLUDE) $(SPGK_WARNINGS)

# --- the link --------------------------------------------------------------
#
# The blocker is dependencies rather than this title's sources, which `make survey` names. Arming it
# before they land gives a link whose undefined list is the whole of libultraship.
#
# main is at src/port/Game.cpp:945, so the shim is C++: in a freestanding C++ translation unit main is
# an ordinary function and comes out as _Z4mainiPPc, which a C shim misses silently, the payload link
# ignoring unresolved symbols.
# Bugdom paid for that lesson.
# libc++ is included either way: the census compiles C++ and needs its headers whether or not there
# is a payload to link. OOPS_CXX_EXCEPTIONS is above the include, which reads it at include time to
# pick between the exceptions and no-exceptions archives.
OOPS_CXX_EXCEPTIONS = 1
OOPS_CXX_STD        = c++20
OOPS_LIBCXX ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libcxx)
include $(OOPS_LIBCXX)/oops-libcxx.mk
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk
# libc++ owns new/delete for any title linking the full archive - see bugdom/docs/PORTING.md.
EXTRA_TARGET_CFLAGS += -DOOPS_CXX_EXTERNAL_NEW_DELETE

ifeq ($(SPGK_ARMED),1)
OOPS_CXX_SRCS := $(SPGK_CXX_SRCS) $(SPGK_LUS_CXX_SRCS) \
                 $(CURDIR)/shim/GameExtractor.cpp $(CURDIR)/shim/spgk_start.cpp
# Object names collide across the trees - ControlDeck, Controller, Light, Matrix, Utils and
# ControllerDefaultMappings each appear in both libultraship and the game - so the objects go on the
# link line rather than into an archive that would keep one of each. Set before the include, which is
# where `oops_ar_check` runs.
OOPS_CXX_LINK_OBJECTS = 1
include $(OOPS_APPS_ROOT)/common/cxx.mk
OOPS_CXX_INCLUDE += $(OOPS_LIBCXX_INCLUDE)
# The two relaxations `-w` does not cover, the same pair Ship of Harkinian needs: narrowing in an
# initialiser list is an error rather than a warning, and libc++ does not reach `<cctype>`
# transitively where libstdc++ does.
OOPS_CXX_FLAGS += -Wno-c++11-narrowing -include cctype
PAYLOAD_SRCS := $(SPGK_C_SRCS) $(SPGK_LUS_C_SRCS) $(OOPS_POSIX_SRCS)
# Each archive after everything that calls it. libultraship sits on top of all of them, so the order
# runs outward: the title's own C++, then the libraries it names, then the standard library.
EXTRA_TARGET_LDFLAGS += $(OOPS_CXX_LDFLAGS) $(OOPS_IMGUI_LDFLAGS) \
                        $(OOPS_LIBZIP_LDFLAGS) $(OOPS_TINYXML2_LDFLAGS) $(OOPS_SPDLOG_LDFLAGS) \
                        $(OOPS_PRISM_LDFLAGS) $(OOPS_GFXD_LDFLAGS) $(OOPS_STB_LDFLAGS) \
                        $(OOPS_VORBIS_LDFLAGS) $(OOPS_ZLIB_LDFLAGS) $(OOPS_SDL_LDFLAGS) \
                        $(OOPS_LIBCXX_LDFLAGS) $(OOPS_LIBCXXABI_LDFLAGS) $(OOPS_LIBUNWIND_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(OOPS_CXX_LIB) $(OOPS_IMGUI_LIB) \
                        $(OOPS_LIBZIP_LIB) $(OOPS_TINYXML2_LIB) $(OOPS_SPDLOG_LIB) \
                        $(OOPS_PRISM_LIB) $(OOPS_GFXD_LIB) $(OOPS_STB_LIB) \
                        $(OOPS_VORBIS_LIB) $(OOPS_OGG_LIB) $(OOPS_ZLIB_LIB) $(OOPS_SDL_LIB) \
                        $(OOPS_LIBCXX_LIB) $(OOPS_LIBCXXABI_LIB) $(OOPS_LIBUNWIND_LIB)
endif

# `__tls_get_addr` is libkernel's, which `common/symbols.txt:391` records, so the loader resolves it.
# It arrives through `BS::thread_pool`'s `this_thread`, which libultraship's resource loader uses; it
# is the one import this title adds to the set `UNDEF_ALLOW` already covers.
EXTRA_UNDEF_ALLOW := ^__tls_get_addr$$

OOPS_FEATURES := display input keyboard mouse audio suspend thread draw dialog

include $(OOPS_APPS_ROOT)/common/app.mk

# Census flags. OOPS_LIBCXX_INCLUDE precedes the C headers: libc++ ships its own <math.h> and
# <stdlib.h>, and <cmath> stops with an explicit error if it reaches the C ones first. Recursive
# (`=`), because TARGET_CFLAGS is not known until app.mk has been read.
SPGK_CXXFLAGS = -x c++ -std=c++20 -Wno-c++11-narrowing $(OOPS_LIBCXX_INCLUDE) \
                $(filter-out -std=c11 -Wstrict-prototypes -Wmissing-prototypes,$(TARGET_CFLAGS)) \
                $(EXTRA_TARGET_CFLAGS)

# ---------------------------------------------------------------------------
# `make survey`: what stands between this title and a link
# ---------------------------------------------------------------------------
.PHONY: survey
survey:
	@echo "spaghetti-kart: upstream $(UPSTREAM_REF) @ $(UPSTREAM_REV)"
	@printf '\n-- submodules\n'
	@for m in libultraship torch; do \
	    if [ -n "$$(ls -A upstream/$$m 2>/dev/null)" ]; then s=present; else s="MISSING"; fi; \
	    printf '   %-14s %s\n' "$$m" "$$s"; \
	 done
	@printf '\n-- source volume\n'
	@printf '   %-18s %s\n' "this title C"   "$(words $(SPGK_C_SRCS))"
	@printf '   %-18s %s\n' "this title C++" "$(words $(SPGK_CXX_SRCS))"
	@printf '   %-18s c:%-5s cpp:%-5s\n' "libultraship" \
	    "$$(find upstream/libultraship/src -name '*.c' 2>/dev/null | wc -l)" \
	    "$$(find upstream/libultraship/src -name '*.cpp' 2>/dev/null | wc -l)"
	@printf '   %-18s cpp:%-5s\n' "torch" \
	    "$$(find upstream/torch/src -name '*.cpp' 2>/dev/null | wc -l)"
	@printf '\n-- dependencies\n'
	@have=0; need=0; \
	 for d in $(SPGK_DEPS); do \
	    if [ -d $(OOPS_APPS_ROOT)/src/oops-deps/$$d ]; then \
	        printf '   %-18s vendored\n' "$$d"; have=$$((have+1)); \
	    else \
	        printf '   %-18s TO VENDOR\n' "$$d"; need=$$((need+1)); \
	    fi; \
	 done; printf '   -> %s vendored, %s to go\n' "$$have" "$$need"
	@printf '\n-- the two headers upstream fetches at configure time\n'
	@printf '   semver.hpp   vendored at src/oops-deps/semver, pinned by commit\n'
	@printf '   sse2neon.h   not needed: it is reached only under __aarch64__\n'
	@printf '\n-- the payload\n'
	@printf '   %-18s %s\n' "C++ objects"  "$(words $(SPGK_CXX_SRCS)) + libultraship $(words $(SPGK_LUS_CXX_SRCS)) + shim 2"
	@printf '   %-18s %s\n' "C objects"    "$(words $(SPGK_C_SRCS)) + libultraship $(words $(SPGK_LUS_C_SRCS)) + posix"
	@printf '   %-18s %s\n' "build it with" "make SPGK_ARMED=1 title"

# ---------------------------------------------------------------------------
# `make census`: which of this title's own sources compile for the target
#
# This title's own sources only, not libultraship or torch, so a clean result here still leaves the
# runtime unbuilt.
# ---------------------------------------------------------------------------
.PHONY: census
census:
	@ok=0; bad=0; mkdir -p $(BUILD); : > $(BUILD)/census-errors.txt; \
	 for f in $(SPGK_C_SRCS); do \
	    if $(TARGET_CC) $(TARGET_CFLAGS) $(EXTRA_TARGET_CFLAGS) -fsyntax-only "$$f" \
	         2>$(BUILD)/census-e.txt; then ok=$$((ok+1)); \
	    else bad=$$((bad+1)); \
	        printf '%s\t%s\n' "$${f#$(SPGK_UP)/}" \
	          "$$(grep -a -m1 'error:' $(BUILD)/census-e.txt | sed 's|.*error: ||' | cut -c1-70)" \
	          >> $(BUILD)/census-errors.txt; \
	    fi; \
	 done; \
	 printf 'spaghetti-kart C census: %s compile, %s do not (of %s)\n' \
	    "$$ok" "$$bad" "$(words $(SPGK_C_SRCS))"; \
	 cok=0; cbad=0; \
	 for f in $(SPGK_CXX_SRCS); do \
	    if $(TARGET_CC) $(SPGK_CXXFLAGS) -fsyntax-only "$$f" 2>$(BUILD)/census-e.txt; \
	      then cok=$$((cok+1)); \
	    else cbad=$$((cbad+1)); \
	        printf '%s\t%s\n' "$${f#$(SPGK_UP)/}" \
	          "$$(grep -a -m1 'error:' $(BUILD)/census-e.txt | sed 's|.*error: ||' | cut -c1-70)" \
	          >> $(BUILD)/census-errors.txt; \
	    fi; \
	 done; \
	 printf 'spaghetti-kart C++ census: %s compile, %s do not (of %s)\n' \
	    "$$cok" "$$cbad" "$(words $(SPGK_CXX_SRCS))"; \
	 if [ "$$bad" -gt 0 ] || [ "$$cbad" -gt 0 ]; then \
	    printf '\n-- blockers, most common first --\n'; \
	    cut -f2 $(BUILD)/census-errors.txt | sort | uniq -c | sort -rn | head -20; \
	    printf '\n-- the files, in $(BUILD)/census-errors.txt --\n'; \
	    head -20 $(BUILD)/census-errors.txt; fi

# ---------------------------------------------------------------------------
# `make census-one F=<path>`: every error in one source. `X=1` for a C++ source.
# ---------------------------------------------------------------------------
.PHONY: census-one
census-one:
	@if [ -n "$(X)" ]; then \
	    $(TARGET_CC) $(SPGK_CXXFLAGS) -fsyntax-only $(SPGK_UP)/$(F) 2>&1; \
	 else \
	    $(TARGET_CC) $(TARGET_CFLAGS) $(EXTRA_TARGET_CFLAGS) -fsyntax-only $(SPGK_UP)/$(F) 2>&1; \
	 fi | grep 'error:' | sed 's|.*error: ||' | sort | uniq -c | sort -rn

# The survey is the default when there is no payload to build.
ifneq ($(SPGK_ARMED),1)
.DEFAULT_GOAL := survey
endif
