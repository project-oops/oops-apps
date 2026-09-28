# Starship, through oops-sdk.
#
#   make           the survey - what stands between this title and a link
#   make census    compile every component's sources for the target, set by set
#   make census-one S=<set> F=<path>   every error in one source
#   make title     the payload, packaged - once STRF_ARMED=1 links
#
# Harbour Masters' Star Fox 64 on libultraship. The runtime, its shader dialect and its
# dependency stack are the ones `../spaghetti-kart` already carries. Torch, the ROM converter
# (`tools/Torch`, driven from `src/port/extractor`), is linked in: the port converts the player's
# ROM itself at first start.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# libultraship makes the GL calls, through its own Fast3D and prism-generated shaders.
OOPS_RENDERER = gl2

include $(OOPS_APPS_ROOT)/common/upstream.mk

STRF_UP := $(CURDIR)/upstream
STRF_LUS := $(STRF_UP)/libultraship
STRF_TORCH := $(STRF_UP)/tools/Torch

# libultraship's stack, as `../spaghetti-kart` names it, and yaml-cpp for Torch. SDL2_net is found
# only under `USE_NETWORKING` (CMakeLists.txt:378), which upstream sets off (:107). Upstream defines
# `CIMGUI_DEFINE_ENUMS_AND_STRUCTS` (:118) but no C source calls cimgui, so it is not linked.
SCAFFOLD_ARM    := STRF_ARMED
SCAFFOLD_DEPS   := sdl2 zlib libzip stb nlohmann-json thread-pool tinyxml2 spdlog imgui \
                   prism-processor libgfxd tomlplusplus semver libogg libvorbis dr-libs yaml-cpp
SCAFFOLD_SUPPLY := a Star Fox 64 ROM (US 1.0 or 1.1, .z64) as /data/homebrew/STRF00001/sf64.z64, \
                   converted to sf64.o2r on the console at first start

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
# Starship's libultraship pins its own prism (`upstream-prism.lock` says which), so the dependency
# builds from this title's copy, into this title's build tree.
OOPS_PRISM_UPSTREAM := $(CURDIR)/upstream-prism
OOPS_PRISM_BUILD := $(CURDIR)/build/prism
OOPS_PRISM ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/prism-processor)
include $(OOPS_PRISM)/oops-prism.mk
OOPS_STB ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/stb)
include $(OOPS_STB)/oops-stb.mk
OOPS_IMGUI ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/imgui)
include $(OOPS_IMGUI)/oops-imgui.mk
OOPS_DR_LIBS ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/dr-libs)
include $(OOPS_DR_LIBS)/oops-dr-libs.mk
OOPS_VORBIS ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libvorbis)
include $(OOPS_VORBIS)/oops-libvorbis.mk
OOPS_GFXD ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libgfxd)
include $(OOPS_GFXD)/oops-libgfxd.mk

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
# CMakeLists.txt:139's `GLOB_RECURSE`, less its filters (:166-175): the `.inc.c` fragments, and
# seven sources upstream leaves out by name.
STRF_FIND = $(shell cd $(STRF_UP) && find $(1) -name '$(2)' 2>/dev/null | sort)
STRF_EXCL := %.inc.c %/src/engine/fox_colheaders.c %/src/engine/fox_edata_info.c \
             %/src/engine/fox_rcp_setup.c %/src/engine/fox_load_inits.c \
             %/src/overlays/ovl_ending/fox_end2_data.c %/src/sys/sys_timer.c \
             %/src/sys/sys_fault.c %/src/mods/object_ram.c
# The single files are entries in the same `file(GLOB)` list as the directories, so one that is
# not in the tree (`src/1FA60.c` at this pin) matches nothing rather than failing: `wildcard`.
STRF_C_SRCS := $(filter-out $(STRF_EXCL), \
    $(addprefix $(STRF_UP)/,$(call STRF_FIND,src/audio src/engine src/overlays src/port src/sys,*.c)) \
    $(wildcard $(addprefix $(STRF_UP)/, \
        src/libultra/io/aisetfreq.c src/libultra/gu/sqrtf.c src/libultra/gu/mtxutil.c \
        src/libultra/gu/ortho.c src/1FA60.c src/buffers.c src/dmatable.c src/libc_math64.c \
        src/libc_sprintf.c)))
STRF_CXX_SRCS := $(filter-out %/android/%, \
    $(addprefix $(STRF_UP)/,$(call STRF_FIND,src/port,*.cpp)))

# libultraship whole, as `../spaghetti-kart` builds it: each backend guards its own contents.
STRF_LUS_C_SRCS := $(sort $(shell find $(STRF_LUS)/src -name '*.c' 2>/dev/null))
STRF_LUS_CXX_SRCS := $(sort $(shell find $(STRF_LUS)/src -name '*.cpp' 2>/dev/null))

# Torch as Starship configures it (CMakeLists.txt:389-395): a library, with Star Fox 64's factories
# and the N64 audio ones its `ast_audio` assets are written in. `main.cpp` is the standalone tool's.
STRF_TORCH_FACTORIES_OFF := mk64 sm64 fzerox
STRF_TORCH_CXX_SRCS := $(filter-out $(STRF_TORCH)/src/main.cpp $(STRF_TORCH)/src/ui/% \
        $(foreach f,$(STRF_TORCH_FACTORIES_OFF),$(STRF_TORCH)/src/factories/$(f)/%), \
    $(sort $(shell find $(STRF_TORCH)/src -name '*.cpp' 2>/dev/null)) \
    $(wildcard $(STRF_TORCH)/lib/strhash64/*.cpp $(STRF_TORCH)/lib/bk_zip/*.cpp))
STRF_TORCH_C_SRCS := $(filter-out $(STRF_TORCH)/src/ui/%, \
    $(sort $(shell find $(STRF_TORCH)/src -name '*.c' 2>/dev/null)) \
    $(wildcard $(STRF_TORCH)/lib/libmio0/*.c $(STRF_TORCH)/lib/libyay0/*.c \
               $(STRF_TORCH)/lib/libyaz0/*.c))

# --- definitions and includes ---------------------------------------------------------------
#
# CMakeLists.txt:111's game switches, libultraship's `ENABLE_OPENGL` and `EXCLUDE_MPQ_SUPPORT`, the
# four `../spaghetti-kart` adds for this target, and Torch's `SF64_SUPPORT` and `NAUDIO_SUPPORT`.
STRF_DEFS := -DVERSION_US=1 -DENABLE_RUMBLE=1 -DF3DEX_GBI=1 -D_LANGUAGE_C -D_USE_MATH_DEFINES \
             -DCIMGUI_DEFINE_ENUMS_AND_STRUCTS -DNON_MATCHING=1 -DNON_EQUIVALENT=1 -DAVOID_UB=1 \
             -DGBI_FLOATS=1 -DENABLE_OPENGL=1 -DEXCLUDE_MPQ_SUPPORT -DSF64_SUPPORT \
             -DNAUDIO_SUPPORT -DYAML_CPP_STATIC_DEFINE \
             -DSPDLOG_NO_THREAD_ID -DSPDLOG_NO_TLS -DSTBI_NO_THREAD_LOCALS -DMA_USE_STDINT

# The CVAR names: Starship overrides none of libultraship's.
STRF_GEN_DIR := $(CURDIR)/build/gen
STRF_CVARS_H := $(STRF_GEN_DIR)/ship/cvars_generated.h
$(shell sh $(OOPS_APPS_ROOT)/common/gen-lus-cvars.sh $(STRF_CVARS_H) OOPS_STRF_CVARS_GENERATED_H \
            $(STRF_LUS)/cmake/cvars.cmake >&2)

STRF_INCLUDE = $(OOPS_POSIX_INCLUDE) \
    $(OOPS_TOMLPP_INCLUDE) $(OOPS_SEMVER_INCLUDE) $(OOPS_JSON_INCLUDE) $(OOPS_LIBZIP_INCLUDE) \
    $(OOPS_THREADPOOL_INCLUDE) $(OOPS_SDL_INCLUDE) $(OOPS_SDL_PREFIX_INCLUDE) \
    $(OOPS_SPDLOG_INCLUDE) $(OOPS_TINYXML2_INCLUDE) $(OOPS_PRISM_INCLUDE) $(OOPS_STB_INCLUDE) \
    $(OOPS_IMGUI_INCLUDE) $(OOPS_ZLIB_INCLUDE) $(OOPS_DR_LIBS_INCLUDE) \
    $(OOPS_VORBIS_INCLUDE) $(OOPS_GFXD_INCLUDE) $(OOPS_YAML_INCLUDE) \
    -I$(STRF_GEN_DIR) -include ship/cvars_generated.h $(STRF_DEFS) \
    -I$(CURDIR)/shim/include \
    -I$(STRF_UP) -I$(STRF_UP)/include -I$(STRF_UP)/src \
    -I$(STRF_LUS)/include -I$(STRF_LUS)/include/libultraship -I$(STRF_LUS)/src \
    -I$(STRF_TORCH) -I$(STRF_TORCH)/src -I$(STRF_TORCH)/lib -I$(STRF_TORCH)/lib/binarytools \
    -I$(STRF_TORCH)/lib/strhash64 -I$(STRF_TORCH)/lib/n64graphics \
    -DOOPS_POSIX_HOME=\"/app0\"

STRF_BASE := -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib -nostdlibinc \
             -fPIC -O2 -w
STRF_SYS   = $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)
# The decompiled C passes pointers as integers and back, which this clang rejects by default;
# `../spaghetti-kart` turns the same three off.
STRF_C     = $(STRF_BASE) -std=gnu11 -Wno-int-conversion -Wno-incompatible-pointer-types \
             -Wno-incompatible-function-pointer-types $(STRF_INCLUDE) -include assert.h $(STRF_SYS)
STRF_CXX   = -x c++ $(STRF_BASE) -std=c++20 -fexceptions -frtti -Wno-c++11-narrowing \
             $(OOPS_LIBCXX_INCLUDE) $(STRF_INCLUDE) -include cctype $(STRF_SYS)

CENSUS_SETS := game_c game_cxx lus_c lus_cxx torch_c torch_cxx
CENSUS_game_c_SRCS := $(STRF_C_SRCS)
CENSUS_game_c_FLAGS = $(STRF_C)
CENSUS_game_cxx_SRCS := $(STRF_CXX_SRCS)
CENSUS_game_cxx_FLAGS = $(STRF_CXX)
CENSUS_lus_c_SRCS := $(STRF_LUS_C_SRCS)
CENSUS_lus_c_FLAGS = $(STRF_C)
CENSUS_lus_cxx_SRCS := $(STRF_LUS_CXX_SRCS)
CENSUS_lus_cxx_FLAGS = $(STRF_CXX)
CENSUS_torch_c_SRCS := $(STRF_TORCH_C_SRCS)
CENSUS_torch_c_FLAGS = $(STRF_C)
CENSUS_torch_cxx_SRCS := $(STRF_TORCH_CXX_SRCS)
CENSUS_torch_cxx_FLAGS = $(STRF_CXX)

OOPS_FEATURES := display input keyboard mouse audio suspend thread draw dialog

ifeq ($(STRF_ARMED),1)
$(error starship: the payload link is not written yet - `make census` says how close it is)
endif

include $(OOPS_APPS_ROOT)/common/app.mk
include $(OOPS_APPS_ROOT)/common/scaffold.mk
include $(OOPS_APPS_ROOT)/common/census.mk

# The `SDL2/` header view the sources include through, which a census compiles against too.
census: $(OOPS_SDL_PREFIX_STAMP)
