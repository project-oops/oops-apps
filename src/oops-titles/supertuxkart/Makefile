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
# libsquish, mcpp, SheenBidi (its unity `SheenBidi.c`) and tinygettext. WiiUse is left out (no
# Wii remotes) and DNS-C with it (`USE_DNS_C` is off on a BSD).
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
# mcpp's own list (`lib/mcpp/CMakeLists.txt`), with the two definitions it builds under.
STKT_MCPP_SRCS := $(addprefix $(STKT_LIB)/mcpp/,directive.c eval.c expand.c main.c \
                    main_libmcpp.c mbchar.c support.c system.c)

# --- flags ------------------------------------------------------------------------------
#
# CMakeLists.txt's definitions for this configuration: the Irrlicht fork static, with its SDL2
# device and without X11, Wayland or Direct3D; sound through OpenAL; the Vulkan renderer's
# shader compiler off (`DISABLE_SHADERC`); crypto through Mbed TLS. `OOPS_MESA_INCLUDE` is the
# sysroot and Mesa's GL headers, which `common/app.mk` reads in.
STKT_DEFS := '-DSUPERTUXKART_VERSION="1.5"' -D_IRR_STATIC_LIB_ -DNO_IRR_COMPILE_WITH_X11_ \
             -DNO_IRR_COMPILE_WITH_WAYLAND_DEVICE_ -DNO_IRR_COMPILE_WITH_DIRECT3D_9_ \
             -DDISABLE_SHADERC -DENABLE_SOUND -DAL_LIBTYPE_STATIC -DENABLE_CRYPTO_MBEDTLS -DNDEBUG \
             -DENABLE_IPV6
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
            -I$(STKT_LIB)/tinygettext/include -I$(STKT_LIB)/simd_wrapper
STKT_DEP_INC = $(OOPS_SDL_INCLUDE) $(OOPS_FT_INCLUDE) $(OOPS_HB_INCLUDE) $(OOPS_PNG_INCLUDE) \
               $(OOPS_JPEG_INCLUDE) $(OOPS_VORBIS_INCLUDE) $(OOPS_OPENAL_INCLUDE) \
               $(OOPS_CURL_INCLUDE) $(OOPS_MBEDTLS_INCLUDE) $(OOPS_ZLIB_INCLUDE)
STKT_BASE := -target x86_64-unknown-freebsd -nostdlib -fPIC -O2 -w
STKT_CXX = -x c++ -std=c++17 $(STKT_BASE) -nostdinc++ -fexceptions -frtti $(OOPS_LIBCXX_INCLUDE) \
           $(STKT_DEFS) $(STKT_INC) $(STKT_DEP_INC) $(OOPS_MESA_INCLUDE) $(OOPS_SDK_INCLUDE)
STKT_C = -x c -std=gnu11 $(STKT_BASE) $(STKT_DEFS) $(STKT_INC) $(STKT_DEP_INC) \
         $(OOPS_MESA_INCLUDE) $(OOPS_SDK_INCLUDE)

CENSUS_SETS := game bullet irrlicht angelscript ge ge_c libs_cxx libs_c mcpp enet
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
CENSUS_libs_cxx_FLAGS = $(STKT_CXX)
CENSUS_libs_c_SRCS := $(STKT_LIBS_C_SRCS)
CENSUS_libs_c_FLAGS = $(STKT_C)

# Arming is where the game's source list (`sources.cmake`) and the bundled libraries' are
# transcribed, with `NO_SHADERC=ON`, `shim/stkt_start.cpp` and `common/crashtrace.c`, and a
# `package` target that stages `upstream/data` and the assets. Until then an armed build says so
# rather than linking a payload of two files.
ifeq ($(STKT_ARMED),1)
$(error supertuxkart: the source lists are not transcribed yet - `make survey` says what is next)
endif

include $(OOPS_APPS_ROOT)/common/app.mk
include $(OOPS_APPS_ROOT)/common/scaffold.mk
include $(OOPS_APPS_ROOT)/common/census.mk

# The `SDL2/` and `harfbuzz/` header views, which a census compiles against too.
census: $(OOPS_SDL_PREFIX_STAMP) $(OOPS_HB_PREFIX_STAMP)
