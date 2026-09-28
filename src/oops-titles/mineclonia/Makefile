# Mineclonia, through oops-sdk.
#
#   make title     the payload, packaged - once MNCL_ARMED=1 links
#   make survey    the pins and the dependencies
#   make census    compile every component's sources for the target, set by set
#   make census-one S=<set> F=<path>   every error in one source
#
# The Luanti engine (`upstream/`) with the Mineclonia game (`upstream-game/`) packaged under
# `games/mineclonia`. Both are free, so the package is complete and the player supplies nothing.
# A separate title from `../craft`, which is its own small engine.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Luanti's Irrlicht fork (`irr/`) draws through its legacy OpenGL driver - GL 2.x with GLSL - in a
# window SDL2 opens. The fork's OpenGL 3 and GLES 2 drivers are not built.
OOPS_RENDERER = gl2

include $(OOPS_APPS_ROOT)/common/upstream.mk

LUANTI := $(CURDIR)/upstream
IRR := $(LUANTI)/irr

# Lua 5.1 (with `bitop`), jsoncpp, mini-gmp, lstrpack, sha256 and the glTF reader are bundled under
# `lib/` and build from upstream, so no LuaJIT. SQLite is the world store and zstd compresses map
# blocks; both are required. cURL, gettext, LevelDB, PostgreSQL, OpenSSL and ncurses are options,
# all off (`shim/include/cmake_config.h`): nothing here downloads content, and a single-player
# world lives in SQLite.
SCAFFOLD_ARM    := MNCL_ARMED
SCAFFOLD_DEPS   := sdl2 libcxx openal-soft libogg libvorbis freetype libpng libjpeg-turbo zlib \
                   zstd sqlite
SCAFFOLD_SUPPLY := nothing - the engine and the game are both free and ship in the package

OOPS_SDL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2)
include $(OOPS_SDL)/oops-sdl.mk
OOPS_FT ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/freetype)
include $(OOPS_FT)/oops-freetype.mk
OOPS_PNG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libpng)
include $(OOPS_PNG)/oops-libpng.mk
OOPS_JPEG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libjpeg-turbo)
include $(OOPS_JPEG)/oops-libjpeg.mk
OOPS_VORBIS ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libvorbis)
include $(OOPS_VORBIS)/oops-libvorbis.mk
OOPS_ZSTD ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/zstd)
include $(OOPS_ZSTD)/oops-zstd.mk
OOPS_SQLITE ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sqlite)
include $(OOPS_SQLITE)/oops-sqlite.mk
include $(OOPS_APPS_ROOT)/common/posix.mk

OOPS_CXX_EXCEPTIONS = 1
OOPS_CXX_STD        = c++17
OOPS_LIBCXX ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libcxx)
include $(OOPS_LIBCXX)/oops-libcxx.mk
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk
# After SDL2, libc++ and the POSIX shim, whose include variables it reads.
OOPS_OPENAL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/openal-soft)
include $(OOPS_OPENAL)/oops-openal-soft.mk

# --- the components -------------------------------------------------------------------
#
# Luanti's `src/` is every source its subdirectories' CMake lists add for a client build, less
# the unit tests, benchmarks and test helpers, the database backends that are switched off, and
# the Android port. The Irrlicht fork's sources are `irr/src/CMakeLists.txt`'s for the SDL device
# and the legacy OpenGL driver: its top level, less the EGL manager that only the GLES driver
# uses.
# `catch.cpp` is added only for unit tests (src/CMakeLists.txt:531), and
# `settings_translation_file.cpp` is not compiled at all: it is a list of fake `gettext` calls for
# `xgettext` to read.
MNCL_SRCS := $(filter-out %/porting_android.cpp %/database-leveldb.cpp %/database-postgresql.cpp \
                          %/database-redis.cpp %/src/catch.cpp %/settings_translation_file.cpp, \
    $(shell find $(LUANTI)/src -name '*.cpp' -not -path '*/unittest/*' -not -path '*/benchmark/*' \
            -not -path '*/src/test/*' 2>/dev/null | sort))
MNCL_IRR_SRCS := $(filter-out %/CEGLManager.cpp,$(wildcard $(IRR)/src/*.cpp))
# Lua is built as C++ (`lib/lua/CMakeLists.txt:1`), so its errors unwind as exceptions through
# the engine's C++. `lua.c` and `luac.c` are the interpreter and compiler programs.
MNCL_LUA_SRCS := $(filter-out %/lua.c %/luac.c,$(wildcard $(LUANTI)/lib/lua/src/*.c))
MNCL_LIB_CXX_SRCS := $(LUANTI)/lib/jsoncpp/jsoncpp.cpp $(LUANTI)/lib/bitop/bit.cpp
MNCL_LIB_C_SRCS := $(LUANTI)/lib/gmp/mini-gmp.c $(LUANTI)/lib/lstrpack/lstrpack.c

# --- flags ------------------------------------------------------------------------------
MNCL_BASE := -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib -nostdlibinc \
             -fPIC -O2 -w
MNCL_SYS   = $(OOPS_POSIX_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)
MNCL_DEPS_INC = $(OOPS_ZSTD_INCLUDE) $(OOPS_SQLITE_INCLUDE) $(OOPS_FT_INCLUDE) $(OOPS_PNG_INCLUDE) \
                $(OOPS_JPEG_INCLUDE) $(OOPS_VORBIS_INCLUDE) $(OOPS_OPENAL_INCLUDE) \
                $(OOPS_SDL_INCLUDE)
MNCL_LIB_INC := -I$(LUANTI)/lib/lua/src -I$(LUANTI)/lib/gmp -I$(LUANTI)/lib/jsoncpp \
                -I$(LUANTI)/lib/bitop -I$(LUANTI)/lib/lstrpack -I$(LUANTI)/lib/sha256 \
                -I$(LUANTI)/lib/tiniergltf
MNCL_CXX = -x c++ $(MNCL_BASE) -std=c++17 $(OOPS_LIBCXX_INCLUDE) $(MNCL_SYS)
# `irr/src/CMakeLists.txt:80-160` for SDL2 and the legacy driver.
MNCL_IRR_DEFS := -D_IRR_POSIX_API_ -D_IRR_COMPILE_WITH_SDL_DEVICE_ \
                 -D_IRR_COMPILE_WITH_JOYSTICK_EVENTS_ -D_IRR_COMPILE_WITH_OPENGL_ \
                 -DIRR_PREFER_SDL_GL_HEADER

CENSUS_SETS := luanti irr lua libs_cxx libs_c
CENSUS_luanti_SRCS := $(MNCL_SRCS)
CENSUS_luanti_FLAGS = $(MNCL_CXX) -DUSE_CMAKE_CONFIG_H -DMT_BUILDTARGET=1 \
    -I$(CURDIR)/shim/include -I$(LUANTI)/src -I$(LUANTI)/src/script $(MNCL_LIB_INC) \
    -I$(IRR)/include $(MNCL_DEPS_INC)
CENSUS_irr_SRCS := $(MNCL_IRR_SRCS)
# The fork reaches back into Luanti for a few headers (`util/base64.h`, `irrlichttypes.h`).
CENSUS_irr_FLAGS = $(MNCL_CXX) $(MNCL_IRR_DEFS) -I$(IRR)/include -I$(IRR)/src -I$(LUANTI)/src \
    -I$(LUANTI)/lib/tiniergltf -I$(LUANTI)/lib/jsoncpp $(MNCL_DEPS_INC)
CENSUS_lua_SRCS := $(MNCL_LUA_SRCS)
CENSUS_lua_FLAGS = $(MNCL_CXX) -DLUA_USE_POSIX -I$(LUANTI)/lib/lua/src
CENSUS_libs_cxx_SRCS := $(MNCL_LIB_CXX_SRCS)
CENSUS_libs_cxx_FLAGS = $(MNCL_CXX) $(MNCL_LIB_INC)
CENSUS_libs_c_SRCS := $(MNCL_LIB_C_SRCS) $(LUANTI)/lib/sha256/sha256.c
CENSUS_libs_c_FLAGS = $(MNCL_BASE) -std=gnu11 -I$(CURDIR)/shim/include/sha256 $(MNCL_LIB_INC) \
    $(MNCL_SYS)

# `net` for the engine's loopback server, which a single-player world runs through.
OOPS_FEATURES := display input keyboard mouse audio suspend thread draw dialog net

ifeq ($(MNCL_ARMED),1)
$(error mineclonia: the payload link is not written yet - `make census` says how close it is)
endif

include $(OOPS_APPS_ROOT)/common/app.mk
include $(OOPS_APPS_ROOT)/common/scaffold.mk
include $(OOPS_APPS_ROOT)/common/census.mk
