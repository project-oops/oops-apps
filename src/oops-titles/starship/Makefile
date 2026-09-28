# Starship, through oops-sdk.
#
#   make title     the payload
#   make package   the payload with the port's archive and Torch's descriptions, which runs
#   make census    compile every component's sources for the target, set by set
#   make census-one S=<set> F=<path>   every error in one source
#   make survey    the pins and what the player supplies
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
# Armed by default: the payload links with nothing unresolved.
STRF_ARMED ?= 1
SCAFFOLD_ARM    := STRF_ARMED
SCAFFOLD_DEPS   := sdl2 zlib libzip stb nlohmann-json thread-pool tinyxml2 spdlog imgui \
                   prism-processor libgfxd tomlplusplus semver libogg libvorbis dr-libs yaml-cpp
SCAFFOLD_SUPPLY := a Star Fox 64 ROM (a version upstream lists, big-endian .z64) as \
                   /data/homebrew/STRF00001/baserom.us.rev1.z64, converted to sf64.o2r on the \
                   console at first start

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
# and the N64 audio ones its `ast_audio` assets are written in, plus the `BinaryTools` and
# `N64Graphics` libraries its CMakeLists.txt:171-176 builds beside it. `main.cpp` stays, as the
# library build keeps it: its `main` is behind `STANDALONE`, and it defines `Companion::Instance`.
STRF_TORCH_FACTORIES_OFF := mk64 sm64 fzerox
# Less Torch's copies of `StringHelper` and `StrHash64`, which libultraship carries with the same
# functions: linked whole beside it they collide. The newer Torch in `../ghostship` drops its own
# copies in exactly this case (its CMakeLists.txt:150-155); this one predates that rule.
STRF_TORCH_CXX_SRCS := $(filter-out $(STRF_TORCH)/src/ui/% \
        $(STRF_TORCH)/src/utils/StringHelper.cpp $(STRF_TORCH)/lib/strhash64/StrHash64.cpp \
        $(foreach f,$(STRF_TORCH_FACTORIES_OFF),$(STRF_TORCH)/src/factories/$(f)/%), \
    $(sort $(shell find $(STRF_TORCH)/src -name '*.cpp' 2>/dev/null)) \
    $(wildcard $(STRF_TORCH)/lib/strhash64/*.cpp $(STRF_TORCH)/lib/bk_zip/*.cpp \
               $(STRF_TORCH)/lib/binarytools/*.cpp))
STRF_TORCH_C_SRCS := $(filter-out $(STRF_TORCH)/src/ui/%, \
    $(sort $(shell find $(STRF_TORCH)/src -name '*.c' 2>/dev/null)) \
    $(wildcard $(STRF_TORCH)/lib/libmio0/*.c $(STRF_TORCH)/lib/libyay0/*.c \
               $(STRF_TORCH)/lib/libyaz0/*.c $(STRF_TORCH)/lib/n64graphics/*.c))

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

# This libultraship predates the `ship/` and `fast/` layout: its headers include each other by bare
# name, and Starship's CMakeLists.txt:247-280 puts each of its subdirectories on the path. The same
# list, less the `extern/` and `Lib/` trees that are not in this checkout.
STRF_LUS_DIRS := $(addprefix -I$(STRF_LUS)/src/,log debug menu utils utils/binarytools config \
                   resource resource/type resource/factory audio window window/gui public \
                   public/libultra public/bridge graphic/Fast3D/U64/PR graphic)

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
    $(STRF_LUS_DIRS) \
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
# `-Wno-parentheses`: Torch's `naudio/v1/AudioConverter.cpp:96-100` writes `tmin <= 0.5f <= tmax`,
# which this clang rejects by default and the compilers upstream builds with warn about. It is
# compiled as upstream compiles it, meaning included.
STRF_CXX   = -x c++ $(STRF_BASE) -std=c++20 -fexceptions -frtti -Wno-c++11-narrowing -Wno-parentheses \
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

include $(OOPS_APPS_ROOT)/common/deps.mk

ifeq ($(STRF_ARMED),1)
# --- the payload ------------------------------------------------------------------------
#
# As `../ghostship` links: the game, libultraship and Torch in one archive linked whole, for
# libultraship's static constructors; each object named for its source path.
STRF_BUILD := build/strf

define strf_objs
STRF_$(1)_OBJS := $$(call oops_objs,$$(STRF_BUILD)/$(1),$$(CENSUS_$(1)_SRCS))
$$(call oops_obj_rules,$$(STRF_BUILD)/$(1),TARGET_CC,CENSUS_$(1)_FLAGS,$$(CENSUS_$(1)_SRCS))
-include $$(STRF_$(1)_OBJS:.o=.d)
endef
$(foreach s,$(CENSUS_SETS),$(eval $(call strf_objs,$(s))))

STRF_LIB := $(STRF_BUILD)/libstarship.a
STRF_OBJS := $(foreach s,$(CENSUS_SETS),$(STRF_$(s)_OBJS))
$(STRF_LIB): $(STRF_OBJS)
	@rm -f $@
	@a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $(STRF_OBJS)
	@echo "starship: $@ ($(words $(STRF_OBJS)) objects)"
$(STRF_OBJS): $(OOPS_SDL_PREFIX_STAMP)

OOPS_CXX_SRCS := $(CURDIR)/shim/strf_start.cpp
include $(OOPS_APPS_ROOT)/common/cxx.mk
OOPS_CXX_INCLUDE += $(OOPS_LIBCXX_INCLUDE)
OOPS_CXX_FLAGS += -I$(OOPS_APPS_ROOT)/common $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE) \
                  -DOOPS_POSIX_HOME=\"/app0\"

EXTRA_TARGET_CFLAGS += -DOOPS_CXX_EXTERNAL_NEW_DELETE $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE) \
                       -I$(OOPS_APPS_ROOT)/common -DOOPS_POSIX_HOME=\"/app0\"
PAYLOAD_SRCS := $(OOPS_APPS_ROOT)/common/crashtrace.c $(OOPS_APPS_ROOT)/common/player_data.c \
                $(OOPS_POSIX_SRCS)
EXTRA_TARGET_LDFLAGS += -Wl,--whole-archive $(STRF_LIB) -Wl,--no-whole-archive \
                        $(OOPS_CXX_LDFLAGS) $(OOPS_IMGUI_LDFLAGS) $(OOPS_YAML_LDFLAGS) \
                        $(OOPS_LIBZIP_LDFLAGS) $(OOPS_TINYXML2_LDFLAGS) $(OOPS_PRISM_LDFLAGS) \
                        $(OOPS_GFXD_LDFLAGS) $(OOPS_STB_LDFLAGS) $(OOPS_VORBIS_LDFLAGS) \
                        $(OOPS_ZLIB_LDFLAGS) $(OOPS_SDL_LDFLAGS) \
                        $(OOPS_LIBCXX_LDFLAGS) $(OOPS_LIBCXXABI_LDFLAGS) $(OOPS_LIBUNWIND_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(STRF_LIB) $(OOPS_CXX_LIB) $(OOPS_IMGUI_LIB) $(OOPS_YAML_LIB) \
                        $(OOPS_LIBZIP_LIB) $(OOPS_TINYXML2_LIB) $(OOPS_PRISM_LIB) \
                        $(OOPS_GFXD_LIB) $(OOPS_STB_LIB) $(OOPS_VORBIS_LIB) $(OOPS_OGG_LIB) \
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
# The port's own archive, `starship.o2r` - `port/` packed as `GeneratePortO2R` packs it
# (CMakeLists.txt:630-636, no version), by `common/pack_o2r.py` - and what Torch reads to convert
# the player's ROM on first start: `config.yml`, which names the ROMs it accepts, and the 103
# `assets/` descriptions it points into. Upstream's conversion (`GameExtractor::GenerateOTR`)
# names both relative to the working directory, which the package is.
STRF_PKG = build/title/$(TITLE_ID)
.PHONY: package
package: title
	@python3 $(OOPS_APPS_ROOT)/common/pack_o2r.py $(STRF_UP)/port $(STRF_PKG)/starship.o2r
	@cp $(STRF_UP)/config.yml $(STRF_PKG)/config.yml
	@rm -rf $(STRF_PKG)/assets && cp -r $(STRF_UP)/assets $(STRF_PKG)/assets
	@echo "starship: package is $(STRF_PKG) ($$(du -sh $(STRF_PKG) | cut -f1))"
