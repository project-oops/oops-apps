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
# Armed by default: the payload links with nothing unresolved.
MNCL_ARMED ?= 1
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
# Plus `builtin_files.cpp`, which upstream's CMake generates: the builtin Lua's digests.
MNCL_GEN := $(CURDIR)/build/gen
$(shell sh $(CURDIR)/shim/gen-builtin-files.sh $(LUANTI) $(MNCL_GEN)/builtin_files.cpp >&2)
MNCL_SRCS += $(MNCL_GEN)/builtin_files.cpp
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
# `gnu++17`, not `c++17`: the Irrlicht fork refuses `__STRICT_ANSI__` (`CFileSystem.cpp`), which
# the strict mode defines, and upstream's CMake builds with the GNU extensions on.
MNCL_CXX = -x c++ $(MNCL_BASE) -std=gnu++17 $(OOPS_LIBCXX_INCLUDE) $(MNCL_SYS)
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
OOPS_FEATURES := display input keyboard mouse audio suspend thread draw png dialog net

# `__tls_get_addr` is libkernel's, which `common/symbols.txt` records, so the loader resolves it;
# the engine's `thread_local`s reach it, as `../balatro`'s do.
EXTRA_UNDEF_ALLOW := ^__tls_get_addr$$

include $(OOPS_APPS_ROOT)/common/deps.mk

ifeq ($(MNCL_ARMED),1)
# --- the payload ------------------------------------------------------------------------
#
# The engine, its Irrlicht fork, Lua and the bundled libraries in one archive, linked whole:
# `main` is in there, and Irrlicht registers its loaders and Luanti its script APIs through
# static constructors, which a plain archive link would drop.
MNCL_BUILD := build/luanti

define mncl_objs
MNCL_$(1)_OBJS := $$(call oops_objs,$$(MNCL_BUILD)/$(1),$$(CENSUS_$(1)_SRCS))
$$(call oops_obj_rules,$$(MNCL_BUILD)/$(1),TARGET_CC,CENSUS_$(1)_FLAGS,$$(CENSUS_$(1)_SRCS))
-include $$(MNCL_$(1)_OBJS:.o=.d)
endef
$(foreach s,$(CENSUS_SETS),$(eval $(call mncl_objs,$(s))))

MNCL_LIB := $(MNCL_BUILD)/libluanti.a
MNCL_OBJS := $(foreach s,$(CENSUS_SETS),$(MNCL_$(s)_OBJS))
$(MNCL_LIB): $(MNCL_OBJS)
	@rm -f $@
	@a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $(MNCL_OBJS)
	@echo "mineclonia: $@ ($(words $(MNCL_OBJS)) objects)"

# The entry shim is C++ (Luanti's `main` is), and brings the C++ runtime with it.
OOPS_CXX_SRCS := $(CURDIR)/shim/mncl_start.cpp
include $(OOPS_APPS_ROOT)/common/cxx.mk
OOPS_CXX_INCLUDE += $(OOPS_LIBCXX_INCLUDE)
OOPS_CXX_FLAGS += -I$(OOPS_APPS_ROOT)/common $(OOPS_POSIX_INCLUDE) -DOOPS_POSIX_HOME=\"/app0\"

EXTRA_TARGET_CFLAGS += -DOOPS_CXX_EXTERNAL_NEW_DELETE $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE) \
                       -I$(OOPS_APPS_ROOT)/common -DOOPS_POSIX_HOME=\"/app0\"
PAYLOAD_SRCS := $(OOPS_APPS_ROOT)/common/tar_unpack.c $(OOPS_APPS_ROOT)/common/crashtrace.c \
                $(OOPS_APPS_ROOT)/common/loading_screen.c \
                $(OOPS_POSIX_SRCS)
EXTRA_TARGET_LDFLAGS += -Wl,--whole-archive $(MNCL_LIB) -Wl,--no-whole-archive \
                        $(OOPS_CXX_LDFLAGS) $(OOPS_SQLITE_LDFLAGS) $(OOPS_ZSTD_LDFLAGS) \
                        $(OOPS_FT_LDFLAGS) $(OOPS_PNG_LDFLAGS) $(OOPS_JPEG_LDFLAGS) \
                        $(OOPS_VORBIS_LDFLAGS) $(OOPS_OPENAL_LDFLAGS) $(OOPS_SDL_LDFLAGS) \
                        $(OOPS_LIBCXX_LDFLAGS) $(OOPS_LIBCXXABI_LDFLAGS) $(OOPS_LIBUNWIND_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(MNCL_LIB) $(OOPS_CXX_LIB) $(OOPS_SQLITE_LIB) $(OOPS_ZSTD_LIB) \
                        $(OOPS_FT_LIB) $(OOPS_PNG_LIB) $(OOPS_ZLIB_LIB) $(OOPS_JPEG_LIB) \
                        $(OOPS_VORBIS_LIB) $(OOPS_OGG_LIB) $(OOPS_OPENAL_LIBS) $(OOPS_SDL_LIB) \
                        $(OOPS_LIBCXX_LIB) $(OOPS_LIBCXXABI_LIB) $(OOPS_LIBUNWIND_LIB)
endif

include $(OOPS_APPS_ROOT)/common/app.mk
include $(OOPS_APPS_ROOT)/common/scaffold.mk
include $(OOPS_APPS_ROOT)/common/census.mk

# --- the data ---------------------------------------------------------------------------
#
# Luanti's run-in-place tree - `builtin/` (the engine's own Lua), `client/` (shaders), `fonts/`,
# `textures/` - with the game under `games/mineclonia`: some 10,000 files, which `pros restore`
# would copy one at a time for over an hour. So `make package` puts them in one tar beside
# `eboot.bin`, and `shim/mncl_start.cpp` unpacks it on first start (`common/tar_unpack.c`).
.PHONY: package
package: title
	@rm -rf build/data && mkdir -p build/data/games
	@cp -r upstream/builtin upstream/client upstream/fonts upstream/textures build/data/
	@cp -r upstream-game build/data/games/mineclonia
	@rm -rf build/data/games/mineclonia/.git build/data/games/mineclonia/.oops-upstream-stamp
	@tar --format=gnu -cf build/title/$(TITLE_ID)/luanti-data.tar -C build/data .
	@echo "mineclonia: data is $$(find build/data -type f | wc -l) files in one tar, $$(du -sh build/title/$(TITLE_ID)/luanti-data.tar | cut -f1)"
