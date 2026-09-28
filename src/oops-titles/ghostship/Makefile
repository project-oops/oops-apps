# Ghostship, through oops-sdk.
#
#   make           the survey - what stands between this title and a link
#   make census    compile every component's sources for the target, set by set
#   make census-one S=<set> F=<path>   every error in one source
#   make title     the payload
#   make package   the payload with the port's archive and Torch's descriptions, which runs
#
# Harbour Masters' Super Mario 64 on libultraship. The runtime, its shader dialect and its
# dependency stack are the ones `../spaghetti-kart` and `../ship-of-harkinian` already carry.
# Torch - the ROM converter - is linked in rather than left out: Ghostship's own first-run flow
# (`src/port/Engine.cpp:691` onward) finds a ROM, converts it to `sm64.o2r` with a progress bar,
# and starts, so the port's part is to build it.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# libultraship makes the GL calls, through its own Fast3D and prism-generated shaders.
OOPS_RENDERER = gl2

include $(OOPS_APPS_ROOT)/common/upstream.mk

GHST_UP := $(CURDIR)/upstream
GHST_LUS := $(GHST_UP)/libultraship
GHST_TORCH := $(GHST_UP)/Torch

# libultraship's stack, as `../spaghetti-kart` names it, and `cimgui`, which this port's C code
# reaches ImGui through. Upstream turns networking on by default (`USE_SATELLA`, its mod-signing
# and relay service, CMakeLists.txt:173, pulls `USE_NETWORKING` on at :193); this build sets
# `USE_SATELLA` off, so neither IXWebSocket nor mbedTLS is needed. yaml-cpp is Torch's.
# Armed by default: the payload links with nothing unresolved.
GHST_ARMED ?= 1
SCAFFOLD_ARM    := GHST_ARMED
SCAFFOLD_DEPS   := sdl2 zlib libzip stb nlohmann-json thread-pool tinyxml2 spdlog imgui cimgui \
                   prism-processor libgfxd tomlplusplus semver libogg libvorbis dr-libs yaml-cpp \
                   monocypher
SCAFFOLD_SUPPLY := a Super Mario 64 ROM (US or JP, big-endian .z64, any name) in \
                   /data/homebrew/GHST00001/, converted to sm64.o2r on the console at first start

include $(OOPS_APPS_ROOT)/common/posix.mk
OOPS_TOMLPP ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/tomlplusplus)
include $(OOPS_TOMLPP)/oops-tomlplusplus.mk
OOPS_SEMVER ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/semver)
include $(OOPS_SEMVER)/oops-semver.mk
OOPS_JSON ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/nlohmann-json)
include $(OOPS_JSON)/oops-nlohmann-json.mk
OOPS_ZLIB ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/zlib)
include $(OOPS_ZLIB)/oops-zlib.mk
OOPS_LIBZIP ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libzip)
include $(OOPS_LIBZIP)/oops-libzip.mk
OOPS_THREADPOOL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/thread-pool)
include $(OOPS_THREADPOOL)/oops-thread-pool.mk
OOPS_SDL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2)
include $(OOPS_SDL)/oops-sdl.mk
OOPS_SPDLOG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/spdlog)
include $(OOPS_SPDLOG)/oops-spdlog.mk
OOPS_TINYXML2 ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/tinyxml2)
include $(OOPS_TINYXML2)/oops-tinyxml2.mk
# Ghostship's libultraship wants a newer prism than the shared pin (`upstream-prism.lock` says
# which), so the dependency builds from this title's own copy, into this title's build tree.
OOPS_PRISM_UPSTREAM := $(CURDIR)/upstream-prism
OOPS_PRISM_BUILD := $(CURDIR)/build/prism
OOPS_PRISM ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/prism-processor)
include $(OOPS_PRISM)/oops-prism.mk
OOPS_STB ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/stb)
include $(OOPS_STB)/oops-stb.mk
OOPS_IMGUI ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/imgui)
include $(OOPS_IMGUI)/oops-imgui.mk
OOPS_CIMGUI ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/cimgui)
include $(OOPS_CIMGUI)/oops-cimgui.mk
OOPS_DR_LIBS ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/dr-libs)
include $(OOPS_DR_LIBS)/oops-dr-libs.mk
OOPS_VORBIS ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libvorbis)
include $(OOPS_VORBIS)/oops-libvorbis.mk
OOPS_GFXD ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libgfxd)
include $(OOPS_GFXD)/oops-libgfxd.mk
OOPS_MONOCYPHER ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/monocypher)
include $(OOPS_MONOCYPHER)/oops-monocypher.mk

OOPS_CXX_EXCEPTIONS = 1
OOPS_CXX_STD        = c++20
OOPS_LIBCXX ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libcxx)
include $(OOPS_LIBCXX)/oops-libcxx.mk
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk
OOPS_YAML ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/yaml-cpp)
include $(OOPS_YAML)/oops-yaml-cpp.mk

# --- the sources, transcribed from upstream's globs --------------------------------------
#
# CMakeLists.txt:235's `GLOB_RECURSE`, less its four filters (:294-297): the `.inc.c` fragments
# other sources include, the EU build's `bin/eu`, the decompilation's own `main.c`, and
# `dlmalloc.c`. `bin/`, `text/` and `textures/` do not exist in this tree: they would hold assets
# converted from a ROM, and this port reads those from `sm64.o2r` instead.
GHST_FIND = $(shell cd $(GHST_UP) && find $(1) -name '$(2)' 2>/dev/null | sort)
GHST_C_SRCS := $(filter-out %.inc.c bin/eu/% %/src/game/main.c %/src/pc/dlmalloc.c, \
    $(addprefix $(GHST_UP)/, \
        $(call GHST_FIND,actors data levels sound src/audio src/buffers src/engine src/game \
                         src/goddard src/menu src/pc src/port,*.c) \
        $(addprefix lib/src/, alBnkfNew.c guLookAtRef.c guMtxF2L.c guNormalize.c guOrthoF.c \
                    guPerspectiveF.c guRotateF.c guScaleF.c guLookAtF.c guTranslateF.c \
                    osAiSetFrequency.c)))
# The port's C++, less the Android bridge and the networking trees `USE_NETWORKING` would add.
GHST_CXX_SRCS := $(filter-out %/android/% %/port/net/% %/port/web/%, \
    $(addprefix $(GHST_UP)/,$(call GHST_FIND,src/pc src/port,*.cpp)))

# libultraship whole, as `../spaghetti-kart` builds it: each backend guards its own contents.
# `ENABLE_SCRIPTING` is off, as upstream sets it for iOS, Android and the web (CMakeLists.txt:463):
# its C mods compile with TCC and load as shared libraries, neither of which this platform has.
# That removes `ship/scripting/`, `ship/security/` and the scripting bridge
# (`src/ship/CMakeLists.txt:87-99`, `src/libultraship/CMakeLists.txt:14`).
GHST_LUS_C_SRCS := $(sort $(shell find $(GHST_LUS)/src -name '*.c' 2>/dev/null))
GHST_LUS_CXX_SRCS := $(filter-out $(GHST_LUS)/src/ship/scripting/% $(GHST_LUS)/src/ship/security/% \
                         %/bridge/scriptingbridge.cpp, \
    $(sort $(shell find $(GHST_LUS)/src -name '*.cpp' 2>/dev/null)))

# Torch as Ghostship configures it (CMakeLists.txt:744-751): a library, not the standalone tool,
# with Super Mario 64's factories and the N64 audio ones its sound banks are written in
# (`assets/ymls/*/sound`) - the others read other games' ROMs. Its `ui/` viewer is `BUILD_UI`,
# off. `main.cpp` stays, as upstream's library build keeps it: its `main` is behind `STANDALONE`,
# and it defines `Companion::Instance`.
GHST_TORCH_FACTORIES_OFF := mk64 sf64 pm64 fzerox bk64 mario_artist oot
# Less Torch's copies of `StringHelper` and `StrHash64`, which libultraship carries too: Torch's
# own CMakeLists.txt:150-155 drops them whenever libultraship is linked whole beside it, as it is
# here, since libultraship's are a superset. Plus the `BinaryTools` library (CMakeLists.txt:307).
GHST_TORCH_CXX_SRCS := $(filter-out $(GHST_TORCH)/src/ui/% \
        $(GHST_TORCH)/src/utils/StringHelper.cpp $(GHST_TORCH)/lib/strhash64/StrHash64.cpp \
        $(foreach f,$(GHST_TORCH_FACTORIES_OFF),$(GHST_TORCH)/src/factories/$(f)/%), \
    $(sort $(shell find $(GHST_TORCH)/src -name '*.cpp' 2>/dev/null)) \
    $(wildcard $(GHST_TORCH)/lib/strhash64/*.cpp $(GHST_TORCH)/lib/bk_zip/*.cpp \
               $(GHST_TORCH)/lib/binarytools/*.cpp))
# And the `N64Graphics` library (CMakeLists.txt:312), less its `stb_image.c`: `../../oops-deps/stb`
# already defines stb_image, and linked whole the two would collide. Its `stb_image_write.c` stays,
# because Torch's copy adds the palette PNG writer it calls.
GHST_TORCH_C_SRCS := $(filter-out $(GHST_TORCH)/src/ui/%, \
    $(sort $(shell find $(GHST_TORCH)/src -name '*.c' 2>/dev/null)) \
    $(wildcard $(GHST_TORCH)/lib/libmio0/*.c $(GHST_TORCH)/lib/libyay0/*.c \
               $(GHST_TORCH)/lib/libyaz0/*.c $(GHST_TORCH)/lib/n64graphics/n64graphics.c \
               $(GHST_TORCH)/lib/n64graphics/stb_image_write.c))

# --- definitions and includes ---------------------------------------------------------------
#
# CMakeLists.txt:202's game switches, libultraship's `ENABLE_OPENGL` and `EXCLUDE_MPQ_SUPPORT`
# (:448), and the four `../spaghetti-kart` adds for this target (thread-local state the loader does
# not relocate, and miniaudio's `uint64_t`). Torch runs as a library (`STANDALONE` undefined) with
# `SM64_SUPPORT` and `NAUDIO_SUPPORT`.
GHST_DEFS := -DVERSION_US=1 -DENABLE_RUMBLE=1 -DF3D_OLD=1 -DF3D_GBI=1 -DGBI_FLOATS=1 \
             -D_LANGUAGE_C -D_USE_MATH_DEFINES -DAVOID_UB=1 -DENABLE_OPENGL=1 \
             -DEXCLUDE_MPQ_SUPPORT -DSM64_SUPPORT -DNAUDIO_SUPPORT -DYAML_CPP_STATIC_DEFINE \
             -DSPDLOG_NO_THREAD_ID -DSPDLOG_NO_TLS -DSTBI_NO_THREAD_LOCALS -DMA_USE_STDINT

# The CVAR names, from upstream's own cmake files in the order CMake reads them: Ghostship's
# overrides first (CMakeLists.txt:7-8), then libultraship's defaults.
GHST_GEN_DIR := $(CURDIR)/build/gen
GHST_CVARS_H := $(GHST_GEN_DIR)/ship/cvars_generated.h
$(shell sh $(OOPS_APPS_ROOT)/common/gen-lus-cvars.sh $(GHST_CVARS_H) OOPS_GHST_CVARS_GENERATED_H \
            $(GHST_UP)/cmake/ghostship-cvars.cmake $(GHST_UP)/cmake/lus-cvars.cmake \
            $(GHST_LUS)/cmake/cvars.cmake >&2)

# libultraship's default context, embedded as its `src/CMakeLists.txt:22-24` embeds it: the
# JSON read whole into the raw string `ship/default_context_json.h.in` wraps it in. Rewritten
# only when the content changes, so nothing that includes it rebuilds for nothing.
GHST_CTX_JSON_H := $(GHST_GEN_DIR)/ship/default_context_json.h
$(shell mkdir -p $(GHST_GEN_DIR)/ship && \
    { printf '#pragma once\n// Generated from default_context.json by the Makefile.\n'; \
      printf 'namespace Ship {\ninline constexpr const char* kDefaultContextJson = R"JSON('; \
      cat $(GHST_LUS)/src/ship/default_context.json; \
      printf ')JSON";\n} // namespace Ship\n'; } > $(GHST_CTX_JSON_H).tmp && \
    { cmp -s $(GHST_CTX_JSON_H).tmp $(GHST_CTX_JSON_H) && rm -f $(GHST_CTX_JSON_H).tmp || \
      mv $(GHST_CTX_JSON_H).tmp $(GHST_CTX_JSON_H); })

# `src/port/build.c`, which CMakeLists.txt:130 configures from `build.c.in`: the version the
# settings menu and the log print. The values are the ones CMake computes - "Nautilus" and the
# NATO word for the patch number (:10-23), the project version, the team (:24) - with the branch,
# commit and tag taken from the pin rather than from a git checkout.
GHST_BUILD_C := $(GHST_GEN_DIR)/build.c
$(shell mkdir -p $(GHST_GEN_DIR) && \
    { printf '#include <libultraship/libultra.h>\n'; \
      printf 'const char gBuildVersion[] = "Nautilus Alfa (3.0.0)";\n'; \
      printf 'const u16 gBuildVersionMajor = 3;\nconst u16 gBuildVersionMinor = 0;\n'; \
      printf 'const u16 gBuildVersionPatch = 0;\n'; \
      printf 'const char gGitBranch[] = "%s";\n' "$(UPSTREAM_REF)"; \
      printf 'const char gGitCommitHash[] = "%.7s";\n' "$(UPSTREAM_REV)"; \
      printf 'const char gGitCommitTag[] = "%s";\n' "$(UPSTREAM_REF)"; \
      printf 'const char gBuildTeam[] = "Lywx";\n'; \
      printf 'const char gBuildDate[] = __DATE__ " " __TIME__;\n'; \
      printf 'const char gBuildMakeOption[] = "";\n'; } > $(GHST_BUILD_C).tmp && \
    { cmp -s $(GHST_BUILD_C).tmp $(GHST_BUILD_C) && rm -f $(GHST_BUILD_C).tmp || \
      mv $(GHST_BUILD_C).tmp $(GHST_BUILD_C); })
GHST_C_SRCS += $(GHST_BUILD_C)

GHST_INCLUDE = $(OOPS_POSIX_INCLUDE) \
    $(OOPS_TOMLPP_INCLUDE) $(OOPS_SEMVER_INCLUDE) $(OOPS_JSON_INCLUDE) $(OOPS_LIBZIP_INCLUDE) \
    $(OOPS_THREADPOOL_INCLUDE) $(OOPS_SDL_INCLUDE) $(OOPS_SDL_PREFIX_INCLUDE) \
    $(OOPS_SPDLOG_INCLUDE) $(OOPS_TINYXML2_INCLUDE) $(OOPS_PRISM_INCLUDE) $(OOPS_STB_INCLUDE) \
    $(OOPS_IMGUI_INCLUDE) $(OOPS_CIMGUI_INCLUDE) $(OOPS_ZLIB_INCLUDE) $(OOPS_DR_LIBS_INCLUDE) \
    $(OOPS_VORBIS_INCLUDE) $(OOPS_GFXD_INCLUDE) $(OOPS_YAML_INCLUDE) $(OOPS_MONOCYPHER_INCLUDE) \
    -I$(GHST_GEN_DIR) -include ship/cvars_generated.h $(GHST_DEFS) \
    -I$(CURDIR)/shim/include \
    -I$(GHST_UP) -I$(GHST_UP)/include -I$(GHST_UP)/src \
    -I$(GHST_LUS)/include -I$(GHST_LUS)/include/libultraship -I$(GHST_LUS)/src \
    -I$(GHST_TORCH) -I$(GHST_TORCH)/src -I$(GHST_TORCH)/lib -I$(GHST_TORCH)/lib/binarytools \
    -I$(GHST_TORCH)/lib/strhash64 -I$(GHST_TORCH)/lib/n64graphics \
    -DOOPS_POSIX_HOME=\"/app0\"

# The C is built as upstream builds it on the desktop, `-O1 -fno-strict-aliasing`
# (CMakeLists.txt:167): its comment says strict aliasing breaks the camera through undefined
# behaviour in the decompiled code. The decompiled C also passes pointers as integers and back,
# which this clang rejects by default and the desktop compilers upstream targets warn about;
# `../spaghetti-kart` turns the same three off.
GHST_BASE := -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib -nostdlibinc \
             -fPIC -w
GHST_SYS   = $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)
GHST_C     = $(GHST_BASE) -O1 -fno-strict-aliasing -std=gnu11 -Wno-int-conversion \
             -Wno-incompatible-pointer-types -Wno-incompatible-function-pointer-types \
             $(GHST_INCLUDE) -include assert.h $(GHST_SYS)
GHST_CXX   = -x c++ $(GHST_BASE) -O2 -std=c++20 -fexceptions -frtti -Wno-c++11-narrowing \
             $(OOPS_LIBCXX_INCLUDE) $(GHST_INCLUDE) -include cctype $(GHST_SYS)

CENSUS_SETS := game_c game_cxx lus_c lus_cxx torch_c torch_cxx
CENSUS_game_c_SRCS := $(GHST_C_SRCS)
CENSUS_game_c_FLAGS = $(GHST_C)
CENSUS_game_cxx_SRCS := $(GHST_CXX_SRCS)
CENSUS_game_cxx_FLAGS = $(GHST_CXX)
CENSUS_lus_c_SRCS := $(GHST_LUS_C_SRCS)
CENSUS_lus_c_FLAGS = $(GHST_C)
CENSUS_lus_cxx_SRCS := $(GHST_LUS_CXX_SRCS)
CENSUS_lus_cxx_FLAGS = $(GHST_CXX)
CENSUS_torch_c_SRCS := $(GHST_TORCH_C_SRCS)
CENSUS_torch_c_FLAGS = $(GHST_C)
CENSUS_torch_cxx_SRCS := $(GHST_TORCH_CXX_SRCS)
CENSUS_torch_cxx_FLAGS = $(GHST_CXX)

OOPS_FEATURES := display input keyboard mouse audio suspend thread draw dialog

include $(OOPS_APPS_ROOT)/common/deps.mk

ifeq ($(GHST_ARMED),1)
# --- the payload ------------------------------------------------------------------------
#
# The game, libultraship and Torch in one archive, linked whole: libultraship registers its
# resource factories and console variables from static constructors, which a plain archive link
# would drop with the object. Each object is named for its source path, so the same file name
# in two trees (`Utils.cpp`, `Light.cpp`) is two objects.
GHST_BUILD := build/ghst

define ghst_objs
GHST_$(1)_OBJS := $$(call oops_objs,$$(GHST_BUILD)/$(1),$$(CENSUS_$(1)_SRCS))
$$(call oops_obj_rules,$$(GHST_BUILD)/$(1),TARGET_CC,CENSUS_$(1)_FLAGS,$$(CENSUS_$(1)_SRCS))
-include $$(GHST_$(1)_OBJS:.o=.d)
endef
$(foreach s,$(CENSUS_SETS),$(eval $(call ghst_objs,$(s))))

GHST_LIB := $(GHST_BUILD)/libghostship.a
GHST_OBJS := $(foreach s,$(CENSUS_SETS),$(GHST_$(s)_OBJS))
$(GHST_LIB): $(GHST_OBJS)
	@rm -f $@
	@a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $(GHST_OBJS)
	@echo "ghostship: $@ ($(words $(GHST_OBJS)) objects)"
# The generated headers, before anything includes them.
$(GHST_OBJS): $(OOPS_SDL_PREFIX_STAMP)

# The entry shim is C++ (upstream's `main` is), and brings the C++ runtime with it.
OOPS_CXX_SRCS := $(CURDIR)/shim/ghst_start.cpp
include $(OOPS_APPS_ROOT)/common/cxx.mk
OOPS_CXX_INCLUDE += $(OOPS_LIBCXX_INCLUDE)
OOPS_CXX_FLAGS += -I$(OOPS_APPS_ROOT)/common $(OOPS_POSIX_INCLUDE) -DOOPS_POSIX_HOME=\"/app0\"

# libc++ owns new/delete for a title linking the full archive (see bugdom/docs/PORTING.md).
EXTRA_TARGET_CFLAGS += -DOOPS_CXX_EXTERNAL_NEW_DELETE $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE) \
                       -I$(OOPS_APPS_ROOT)/common -DOOPS_POSIX_HOME=\"/app0\"
PAYLOAD_SRCS := $(OOPS_APPS_ROOT)/common/crashtrace.c $(OOPS_APPS_ROOT)/common/player_data.c \
                $(OOPS_APPS_ROOT)/common/tar_unpack.c $(OOPS_POSIX_SRCS)
# Each archive after everything that calls it, as `../spaghetti-kart` orders them.
EXTRA_TARGET_LDFLAGS += -Wl,--whole-archive $(GHST_LIB) -Wl,--no-whole-archive \
                        $(OOPS_CXX_LDFLAGS) $(OOPS_CIMGUI_LDFLAGS) $(OOPS_IMGUI_LDFLAGS) \
                        $(OOPS_YAML_LDFLAGS) $(OOPS_LIBZIP_LDFLAGS) $(OOPS_TINYXML2_LDFLAGS) \
                        $(OOPS_PRISM_LDFLAGS) $(OOPS_GFXD_LDFLAGS) $(OOPS_STB_LDFLAGS) \
                        $(OOPS_MONOCYPHER_LDFLAGS) $(OOPS_VORBIS_LDFLAGS) $(OOPS_ZLIB_LDFLAGS) \
                        $(OOPS_SDL_LDFLAGS) \
                        $(OOPS_LIBCXX_LDFLAGS) $(OOPS_LIBCXXABI_LDFLAGS) $(OOPS_LIBUNWIND_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(GHST_LIB) $(OOPS_CXX_LIB) $(OOPS_CIMGUI_LIB) $(OOPS_IMGUI_LIB) \
                        $(OOPS_YAML_LIB) $(OOPS_LIBZIP_LIB) $(OOPS_TINYXML2_LIB) \
                        $(OOPS_PRISM_LIB) $(OOPS_GFXD_LIB) $(OOPS_STB_LIB) \
                        $(OOPS_MONOCYPHER_LIB) $(OOPS_VORBIS_LIB) $(OOPS_OGG_LIB) \
                        $(OOPS_ZLIB_LIB) $(OOPS_SDL_LIB) \
                        $(OOPS_LIBCXX_LIB) $(OOPS_LIBCXXABI_LIB) $(OOPS_LIBUNWIND_LIB)
endif

# `__tls_get_addr` is libkernel's, which `common/symbols.txt` records, so the loader resolves it;
# `BS::thread_pool`'s `this_thread` reaches it, as in `../spaghetti-kart`.
EXTRA_UNDEF_ALLOW := ^__tls_get_addr$$

include $(OOPS_APPS_ROOT)/common/app.mk
include $(OOPS_APPS_ROOT)/common/scaffold.mk
include $(OOPS_APPS_ROOT)/common/census.mk

# The `SDL2/` header view the sources include through, which a census compiles against too.
census: $(OOPS_SDL_PREFIX_STAMP)

# --- the data ---------------------------------------------------------------------------
#
# What upstream's install puts beside the executable (CMakeLists.txt:1002-1004): the port's own
# archive, `ghostship.o2r`; `config.yml`, which names the ROMs Torch accepts; and `assets/`, the
# extraction descriptions Torch reads to convert the player's ROM. `ghostship.o2r` is `port/`
# with libultraship's shaders copied over it, packed as `GeneratePortO2R` packs it
# (`common/pack_o2r.py`). `assets/` is 842 files, so it ships as one tar the entry shim unpacks on
# first start - `pros restore` costs per file.
GHST_PKG = build/title/$(TITLE_ID)
.PHONY: package
package: title
	@rm -rf build/o2r-port && mkdir -p build/o2r-port
	@cp -r $(GHST_UP)/port/. build/o2r-port/
	@cp -r $(GHST_LUS)/src/fast/shaders/. build/o2r-port/shaders/
	@python3 $(OOPS_APPS_ROOT)/common/pack_o2r.py build/o2r-port $(GHST_PKG)/ghostship.o2r 3.0.0
	@cp $(GHST_UP)/config.yml $(GHST_PKG)/config.yml
	@tar --format=gnu -cf $(GHST_PKG)/ghostship-assets.tar -C $(GHST_UP) assets
	@echo "ghostship: package is $(GHST_PKG) ($$(du -sh $(GHST_PKG) | cut -f1))"
