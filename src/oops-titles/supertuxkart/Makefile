# SuperTuxKart, through oops-sdk.
#
#   make           the survey - what stands between this title and a link
#   make title     the payload, packaged - once STKT_ARMED=1 links
#
# The game (`upstream/`, stk-code 1.5) with its data (`upstream-assets/`, the matching release
# download). Both are free, so the package is complete and the player supplies nothing. A third
# racing title beside `../extreme-tux-racer`, not a replacement for it.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Its renderer is OpenGL 3.3 (or GLES 3.0), with no GL 2 path left in 1.x: that is Mesa's here.
# The Vulkan renderer is off (`NO_SHADERC`).
OOPS_RENDERER = mesa

include $(OOPS_APPS_ROOT)/common/upstream.mk

# AngelScript, Bullet, ENet, the Irrlicht fork, libsquish, mcpp, SheenBidi and tinygettext are
# bundled under `lib/` and build from upstream. HarfBuzz shapes the text FreeType draws. cURL is
# required by the build (CMakeLists.txt:554) for add-ons and online play, and the crypto behind
# them is MbedTLS or OpenSSL; neither is used offline, but both must link. SQLite is optional
# (`USE_SQLITE3`, server statistics) and is off.
#
# The assets are 4,279 files (273 MB unpacked): they ship as one archive and unpack at first
# run, since `pros restore` costs per file.
STKT_ARMED ?= 1
SCAFFOLD_ARM    := STKT_ARMED
SCAFFOLD_DEPS   := sdl2 libcxx openal-soft libogg libvorbis freetype harfbuzz libpng \
                   libjpeg-turbo zlib curl mbedtls
SCAFFOLD_SUPPLY := nothing - the game and its data are free and ship in the package

OOPS_FEATURES := display input keyboard mouse audio suspend thread draw dialog net

# Every dependency builds hosted, against the Mesa sysroot, because the title is
# (`common/dep-sys.mk`); libc++ too, which `OOPS_LIBCXX_HOSTED` selects.
include $(OOPS_APPS_ROOT)/common/posix.mk
OOPS_ZLIB ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/zlib)
include $(OOPS_ZLIB)/oops-zlib.mk
OOPS_MBEDTLS ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/mbedtls)
include $(OOPS_MBEDTLS)/oops-mbedtls.mk
OOPS_CURL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/curl)
include $(OOPS_CURL)/oops-curl.mk
OOPS_PNG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libpng)
include $(OOPS_PNG)/oops-libpng.mk
OOPS_JPEG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libjpeg-turbo)
include $(OOPS_JPEG)/oops-libjpeg.mk
OOPS_VORBIS ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libvorbis)
include $(OOPS_VORBIS)/oops-libvorbis.mk
OOPS_FT ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/freetype)
include $(OOPS_FT)/oops-freetype.mk
OOPS_SDL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2)
include $(OOPS_SDL)/oops-sdl.mk
OOPS_CXX_EXCEPTIONS = 1
OOPS_CXX_STD        = c++17
OOPS_LIBCXX_HOSTED  = 1
OOPS_LIBCXX ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libcxx)
include $(OOPS_LIBCXX)/oops-libcxx.mk
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk
# After SDL2 and libc++, whose include variables they read.
OOPS_HARFBUZZ ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/harfbuzz)
include $(OOPS_HARFBUZZ)/oops-harfbuzz.mk
OOPS_OPENAL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/openal-soft)
include $(OOPS_OPENAL)/oops-openal-soft.mk

# --- the sources ------------------------------------------------------------------------
#
# The game is `sources.cmake`'s glob, `src/*.cpp` recursively. The bundled libraries are their
# own CMake lists: Bullet, Irrlicht, AngelScript, ENet, the graphics engine and utilities,
# libsquish, mcpp, SheenBidi (its unity `SheenBidi.c`), tinygettext and DNS-C. WiiUse is left
# out (no Wii remotes). DNS-C (`USE_DNS_C`) is off upstream wherever the C library has a
# resolver; this platform's exports no `res_query` and none of the `ns_*` parsers the other path
# calls, so the bundled resolver is the one that works.
STKT := $(CURDIR)/upstream
STKT_LIB := $(STKT)/lib
stkt_find = $(sort $(shell find $(1) -name '$(2)' 2>/dev/null))
STKT_GAME_SRCS := $(call stkt_find,$(STKT)/src,*.cpp)
STKT_BULLET_SRCS := $(call stkt_find,$(STKT_LIB)/bullet/src,*.cpp)
# Less the Wayland protocol glue, generated C for the Wayland device, which is off.
STKT_IRR_SRCS := $(filter-out %_protocol.c, \
                   $(wildcard $(STKT_LIB)/irrlicht/source/Irrlicht/*.cpp \
                              $(STKT_LIB)/irrlicht/source/Irrlicht/*.c))
STKT_AS_SRCS := $(wildcard $(STKT_LIB)/angelscript/source/*.cpp)
STKT_GE_SRCS := $(wildcard $(STKT_LIB)/graphics_engine/src/*.cpp)
STKT_GE_C_SRCS := $(wildcard $(STKT_LIB)/graphics_engine/src/*.c)
STKT_LIBS_CXX_SRCS := $(wildcard $(STKT_LIB)/libsquish/*.cpp $(STKT_LIB)/tinygettext/src/*.cpp)
STKT_LIBS_C_SRCS := $(wildcard $(STKT_LIB)/graphics_utils/mipmap/*.c) \
                    $(STKT_LIB)/sheenbidi/Source/SheenBidi.c
STKT_ENET_SRCS := $(wildcard $(STKT_LIB)/enet/*.c)
# mcpp's own list (`lib/mcpp/CMakeLists.txt`), with the two definitions it builds under, less
# `main_libmcpp.c`: a sample `main` that upstream's plain archive link never pulls in, and that
# the whole-archive link below would.
STKT_MCPP_SRCS := $(addprefix $(STKT_LIB)/mcpp/,directive.c eval.c expand.c main.c \
                    mbchar.c support.c system.c)

# --- flags ------------------------------------------------------------------------------
#
# CMakeLists.txt's definitions for this configuration: the Irrlicht fork static, with its SDL2
# device and without X11, Wayland or Direct3D; sound through OpenAL; the Vulkan renderer's
# shader compiler off (`DISABLE_SHADERC`); crypto through Mbed TLS. `OOPS_MESA_INCLUDE` is the
# sysroot and Mesa's GL headers, which `common/app.mk` reads in.
STKT_DEFS := '-DSUPERTUXKART_VERSION="1.5"' -D_IRR_STATIC_LIB_ -DNO_IRR_COMPILE_WITH_X11_ \
             -DNO_IRR_COMPILE_WITH_WAYLAND_DEVICE_ -DNO_IRR_COMPILE_WITH_DIRECT3D_9_ \
             -DDISABLE_SHADERC -DENABLE_SOUND -DAL_LIBTYPE_STATIC -DENABLE_CRYPTO_MBEDTLS -DNDEBUG \
             -DENABLE_IPV6 -DDNS_C
# `USE_IPV6` is on by default (CMakeLists.txt:29, :237), which is also what selects the bundled
# ENet. ENet's own CMake probes the C library (lib/enet/CMakeLists.txt:7-65); against FreeBSD's
# every probe succeeds, so every switch it would set is set.
STKT_ENET_DEFS := -DHAS_FCNTL=1 -DHAS_POLL=1 -DHAS_GETNAMEINFO=1 -DHAS_GETADDRINFO=1 \
                  -DHAS_GETHOSTBYNAME_R=1 -DHAS_GETHOSTBYADDR_R=1 -DHAS_INET_PTON=1 \
                  -DHAS_INET_NTOP=1 -DHAS_MSGHDR_FLAGS=1 -DHAS_SOCKLEN_T=1
STKT_INC := -I$(STKT)/src -I$(STKT_LIB)/irrlicht/include -I$(STKT_LIB)/bullet/src \
            -I$(STKT_LIB)/enet/include -I$(STKT_LIB)/angelscript/include \
            -I$(STKT_LIB)/graphics_engine/include -I$(STKT_LIB)/graphics_utils \
            -I$(STKT_LIB)/libsquish -I$(STKT_LIB)/mcpp -I$(STKT_LIB)/sheenbidi/Headers \
            -I$(STKT_LIB)/tinygettext/include -I$(STKT_LIB)/simd_wrapper -I$(STKT_LIB)/dnsc
STKT_DEP_INC = $(OOPS_SDL_INCLUDE) $(OOPS_FT_INCLUDE) $(OOPS_HB_INCLUDE) $(OOPS_PNG_INCLUDE) \
               $(OOPS_JPEG_INCLUDE) $(OOPS_VORBIS_INCLUDE) $(OOPS_OPENAL_INCLUDE) \
               $(OOPS_CURL_INCLUDE) $(OOPS_MBEDTLS_INCLUDE) $(OOPS_ZLIB_INCLUDE)
# `-ftls-model=initial-exec`: the game keeps per-thread state in `thread_local` (`stk_process.cpp`,
# `log.cpp`), and the general-dynamic model's `__tls_get_addr` faults in libkernel on this
# platform. oops-mesa builds Mesa with the same flag (`../../oops-mesa/gl-cts` records it).
STKT_BASE := -target x86_64-unknown-freebsd -nostdlib -fPIC -O2 -w -ftls-model=initial-exec
STKT_CXX = -x c++ -std=c++17 $(STKT_BASE) -nostdinc++ -fexceptions -frtti $(OOPS_LIBCXX_INCLUDE) \
           $(STKT_DEFS) $(STKT_INC) $(STKT_DEP_INC) $(OOPS_MESA_INCLUDE) $(OOPS_SDK_INCLUDE)
STKT_C = -x c -std=gnu11 $(STKT_BASE) $(STKT_DEFS) $(STKT_INC) $(STKT_DEP_INC) \
         $(OOPS_MESA_INCLUDE) $(OOPS_SDK_INCLUDE)

CENSUS_SETS := game bullet irrlicht angelscript ge ge_c libs_cxx libs_c mcpp enet dnsc
CENSUS_enet_SRCS := $(STKT_ENET_SRCS)
CENSUS_enet_FLAGS = $(STKT_C) $(STKT_ENET_DEFS)
CENSUS_mcpp_SRCS := $(STKT_MCPP_SRCS)
CENSUS_mcpp_FLAGS = $(STKT_C) -DMCPP_LIB -DHAVE_CONFIG_H
CENSUS_game_SRCS := $(STKT_GAME_SRCS)
CENSUS_game_FLAGS = $(STKT_CXX)
CENSUS_bullet_SRCS := $(STKT_BULLET_SRCS)
CENSUS_bullet_FLAGS = $(STKT_CXX)
CENSUS_irrlicht_SRCS := $(STKT_IRR_SRCS)
CENSUS_irrlicht_FLAGS = $(STKT_CXX) -DIRRLICHT_EXPORTS=1 -I$(STKT_LIB)/irrlicht/source/Irrlicht
CENSUS_angelscript_SRCS := $(STKT_AS_SRCS)
CENSUS_angelscript_FLAGS = $(STKT_CXX)
CENSUS_ge_SRCS := $(STKT_GE_SRCS)
CENSUS_ge_FLAGS = $(STKT_CXX)
CENSUS_ge_c_SRCS := $(STKT_GE_C_SRCS)
CENSUS_ge_c_FLAGS = $(STKT_C)
CENSUS_libs_cxx_SRCS := $(STKT_LIBS_CXX_SRCS)
# Each library's own definitions: tinygettext without iconv, as STK builds it
# (lib/tinygettext/CMakeLists.txt:51, :81), and SheenBidi's unity file, which is empty without
# `SB_CONFIG_UNITY` (lib/sheenbidi/CMakeLists.txt:1).
CENSUS_libs_cxx_FLAGS = $(STKT_CXX) -DDISABLE_ICONV
CENSUS_libs_c_SRCS := $(STKT_LIBS_C_SRCS)
CENSUS_libs_c_FLAGS = $(STKT_C) -DSB_CONFIG_UNITY
# DNS-C as lib/dnsc/CMakeLists.txt builds it: strict C99.
CENSUS_dnsc_SRCS := $(STKT_LIB)/dnsc/dns.c
CENSUS_dnsc_FLAGS = $(STKT_C) -std=c99

ifeq ($(STKT_ARMED),1)
# --- the payload ------------------------------------------------------------------------
#
# Every census set in one archive, linked whole: the engine registers its GUI widgets, scripting
# bindings and resource loaders from static constructors, which a plain archive link would drop
# with the object. Each object is named for its source path, so the same file name in two trees
# is two objects.
STKT_BUILD := build/stkt

define stkt_objs
STKT_$(1)_OBJS := $$(call oops_objs,$$(STKT_BUILD)/$(1),$$(CENSUS_$(1)_SRCS))
$$(call oops_obj_rules,$$(STKT_BUILD)/$(1),TARGET_CC,CENSUS_$(1)_FLAGS,$$(CENSUS_$(1)_SRCS))
-include $$(STKT_$(1)_OBJS:.o=.d)
endef
$(foreach s,$(CENSUS_SETS),$(eval $(call stkt_objs,$(s))))

STKT_LIB := $(STKT_BUILD)/libsupertuxkart.a
STKT_OBJS := $(foreach s,$(CENSUS_SETS),$(STKT_$(s)_OBJS))
$(STKT_LIB): $(STKT_OBJS)
	@rm -f $@
	@a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $(STKT_OBJS)
	@echo "supertuxkart: $@ ($(words $(STKT_OBJS)) objects)"
# The generated header views, before anything includes them.
$(STKT_OBJS): $(OOPS_SDL_PREFIX_STAMP) $(OOPS_HB_PREFIX_STAMP)

# The entry shim is C++ (upstream's `main` is), and brings the C++ runtime with it.
OOPS_CXX_SRCS := $(CURDIR)/shim/stkt_start.cpp
include $(OOPS_APPS_ROOT)/common/cxx.mk
OOPS_CXX_INCLUDE += $(OOPS_LIBCXX_INCLUDE) -ftls-model=initial-exec
OOPS_CXX_FLAGS += -I$(OOPS_APPS_ROOT)/common -DOOPS_POSIX_HOME=\"/app0\"

# Hosted: the C library is the sysroot's, so `common/posix` contributes only `hosted.c`, the
# calls it declares and the platform does not export; and libc++ owns new and delete.
EXTRA_TARGET_CFLAGS += -DOOPS_CXX_EXTERNAL_NEW_DELETE -I$(OOPS_APPS_ROOT)/common \
                       -DOOPS_POSIX_HOME=\"/app0\"
PAYLOAD_SRCS := $(OOPS_APPS_ROOT)/common/crashtrace.c $(OOPS_APPS_ROOT)/common/tar_unpack.c \
                $(OOPS_POSIX_HOSTED_SRCS)
# Each archive after everything that calls it.
EXTRA_TARGET_LDFLAGS += -Wl,--whole-archive $(STKT_LIB) -Wl,--no-whole-archive \
                        $(OOPS_CXX_LDFLAGS) $(OOPS_HB_LDFLAGS) $(OOPS_FT_LDFLAGS) \
                        $(OOPS_OPENAL_LDFLAGS) $(OOPS_CURL_LDFLAGS) $(OOPS_MBEDTLS_LDFLAGS) \
                        $(OOPS_PNG_LDFLAGS) $(OOPS_JPEG_LDFLAGS) $(OOPS_VORBIS_LDFLAGS) \
                        $(OOPS_ZLIB_LDFLAGS) $(OOPS_SDL_LDFLAGS) \
                        $(OOPS_LIBCXX_LDFLAGS) $(OOPS_LIBCXXABI_LDFLAGS) $(OOPS_LIBUNWIND_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(STKT_LIB) $(OOPS_CXX_LIB) $(OOPS_HB_LIB) $(OOPS_FT_LIB) \
                        $(OOPS_OPENAL_LIBS) $(OOPS_CURL_LIB) $(OOPS_MBEDTLS_LIB) \
                        $(OOPS_PNG_LIB) $(OOPS_JPEG_LIB) $(OOPS_VORBIS_LIB) $(OOPS_OGG_LIB) \
                        $(OOPS_ZLIB_LIB) $(OOPS_SDL_LIB) \
                        $(OOPS_LIBCXX_LIB) $(OOPS_LIBCXXABI_LIB) $(OOPS_LIBUNWIND_LIB)

# Mesa's PT_TLS segment and `.init_array` bounds, keeping `.eh_frame` for the unwinder.
TARGET_LD_SCRIPT := $(abspath $(OOPS_APPS_ROOT)/common/mesa_title_eh.ld)
endif

# A Mesa title imports more of the C library than `common/symbols.txt` names, so its manifest is
# generated from the linked payload, as `../craft` does: `make imports` after a link, then
# `make title`. It is also the undefined-symbol check a hosted link has - a name no platform
# library exports stops the manifest.
OOPS_MESA ?= $(abspath $(OOPS_APPS_ROOT)/../oops-mesa)
SYMBOLS_FILE := $(abspath build/imports.txt)

include $(OOPS_APPS_ROOT)/common/app.mk

# oops-mesa's `liboopsmesa_cxx` stand-ins duplicate libc++ and libc++abi, which this title links
# (as `../../oops-mesa/gl-cts` filters them). `TARGET_LDFLAGS` reads this at link time.
EXTRA_TARGET_LDFLAGS := $(filter-out -loopsmesa_cxx,$(EXTRA_TARGET_LDFLAGS))

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
include $(OOPS_APPS_ROOT)/common/scaffold.mk
include $(OOPS_APPS_ROOT)/common/census.mk

# The `SDL2/` and `harfbuzz/` header views, which a census compiles against too.
census: $(OOPS_SDL_PREFIX_STAMP) $(OOPS_HB_PREFIX_STAMP)

# --- the data ---------------------------------------------------------------------------
#
# stk-code's own `data/` (1,307 files: GUI, shaders, fonts, translations) and the release's
# assets (4,279 files: karts, tracks, textures, music), as one tar the entry shim unpacks on
# first start into `/app0/data` and `/app0/assets` - `pros restore` costs per file.
STKT_PKG = build/title/$(TITLE_ID)
.PHONY: package
package: title
	@tar --format=gnu -cf $(STKT_PKG)/stk-data.tar --transform 's,^upstream-assets,assets,' \
	    -C upstream data -C .. upstream-assets
	@echo "supertuxkart: package is $(STKT_PKG) ($$(du -sh $(STKT_PKG) | cut -f1))"
