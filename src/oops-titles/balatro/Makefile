# Balatro, through oops-sdk.
#
#   make title     the payload, packaged
#   make survey    the pin, the dependencies and what the player supplies
#   make census    compile every component's sources for the target, set by set
#   make census-one S=<set> F=<path>   every error in one source
#
# LÖVE 11.5 (`upstream/`), running the game from the player's own `Balatro.exe`. Nothing of
# Balatro is fetched or built: the engine is the port, and every other LÖVE game runs on the same
# payload given a different archive.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# LÖVE 11 needs OpenGL 2.1 with GLSL. It loads its entry points through its bundled `glad`, by
# name, so a clean link says nothing about which of them answer - `../q3rally`'s
# `scripts/gl-surface.sh` is the check for that.
OOPS_RENDERER = gl2

include $(OOPS_APPS_ROOT)/common/upstream.mk

LOVE := $(CURDIR)/upstream/src

# PhysFS, Box2D, lua-enet, luasocket, glslang and the image decoders are bundled under
# `src/libraries/` and build from upstream. LuaJIT is the interpreter only (`oops-luajit.mk`).
# Armed by default: the payload links with nothing unresolved.
BLTR_ARMED ?= 1
SCAFFOLD_ARM    := BLTR_ARMED
SCAFFOLD_DEPS   := sdl2 libcxx openal-soft freetype libogg libvorbis libtheora libmodplug zlib \
                   luajit
SCAFFOLD_SUPPLY := Balatro.exe from the player's own copy of Balatro, as \
                   /data/homebrew/BLTR00001/Balatro.exe

OOPS_SDL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2)
include $(OOPS_SDL)/oops-sdl.mk
OOPS_FT ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/freetype)
include $(OOPS_FT)/oops-freetype.mk
OOPS_VORBIS ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libvorbis)
include $(OOPS_VORBIS)/oops-libvorbis.mk
OOPS_THEORA ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libtheora)
include $(OOPS_THEORA)/oops-libtheora.mk
OOPS_ZLIB ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/zlib)
include $(OOPS_ZLIB)/oops-zlib.mk
include $(OOPS_APPS_ROOT)/common/posix.mk

OOPS_CXX_EXCEPTIONS = 1
OOPS_CXX_STD        = c++11
OOPS_LIBCXX ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libcxx)
include $(OOPS_LIBCXX)/oops-libcxx.mk
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk
OOPS_MODPLUG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libmodplug)
include $(OOPS_MODPLUG)/oops-libmodplug.mk
OOPS_LUAJIT ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/luajit)
include $(OOPS_LUAJIT)/oops-luajit.mk
# After SDL2, libc++ and the POSIX shim, whose include variables it reads.
OOPS_OPENAL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/openal-soft)
include $(OOPS_OPENAL)/oops-openal-soft.mk

# --- the components ---------------------------------------------------------------------
#
# Every module with every backend LÖVE builds on Linux, which is the branch `common/config.h`
# takes for FreeBSD - SDL for window, input, events and threads, OpenAL and the null device for
# audio, OpenGL for graphics - and every bundled library, less the Windows halves of the socket
# libraries. `love.cpp` holds `main`.
BLTR_LOVE_SRCS := $(LOVE)/love.cpp $(filter-out %/android.cpp, $(wildcard $(LOVE)/common/*.cpp)) \
    $(shell find $(LOVE)/modules -name '*.cpp' 2>/dev/null | sort)
# luasocket's Unix-domain and serial modules want `AF_UNIX` and `<sys/un.h>`, which a payload
# has no use for, and `luasocket.cpp` does not register them. Its `compat.c` supplies Lua 5.2's
# `luaL_setfuncs` to a Lua 5.1, and LuaJIT - which reports itself as 5.1 - defines it already; a
# desktop's dynamic link lets one quietly win, a static one refuses the pair.
BLTR_LIB_EXCL := %/wsocket.c %/win32.c $(LOVE)/libraries/glslang/glslang/OSDependent/Windows/% \
                 %/libluasocket/unix.c %/libluasocket/unixtcp.c %/libluasocket/unixudp.c \
                 %/libluasocket/unixstream.c %/libluasocket/unixdgram.c %/libluasocket/serial.c \
                 %/libluasocket/compat.c
BLTR_LIB_CXX_SRCS := $(filter-out $(BLTR_LIB_EXCL), \
    $(shell find $(LOVE)/libraries \( -name '*.cpp' -o -name '*.cc' \) 2>/dev/null | sort))
BLTR_LIB_C_SRCS := $(filter-out $(BLTR_LIB_EXCL), \
    $(shell find $(LOVE)/libraries -name '*.c' 2>/dev/null | sort))

# --- flags --------------------------------------------------------------------------------
#
# `LOVE_NOMPG123`: LÖVE 11 decodes MP3 only through mpg123, which is not built, so MP3 is
# unsupported; Ogg Vorbis, WAV, FLAC and tracker modules are. `HAS_SOCKLEN_T` is
# CMakeLists.txt:198's, for luasocket.
#
# `PHYSFS_NO_CDROM_SUPPORT` is PhysFS's own switch (`physfs_platform_unix.c:26`): its CD-ROM
# discovery walks the mounted filesystems through `<sys/mount.h>`, and there are no discs to find.
BLTR_BASE := -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib -nostdlibinc \
             -fPIC -ftls-model=initial-exec -O2 -w -DLOVE_NOMPG123 -DHAS_SOCKLEN_T -DPHYSFS_NO_CDROM_SUPPORT=1
BLTR_SYS   = $(OOPS_POSIX_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)
BLTR_INC   = -I$(LOVE) -I$(LOVE)/libraries -I$(LOVE)/modules \
             -I$(LOVE)/libraries/enet/libenet/include $(OOPS_LUAJIT_INCLUDE) $(OOPS_FT_INCLUDE) \
             $(OOPS_VORBIS_INCLUDE) $(OOPS_THEORA_INCLUDE) $(OOPS_ZLIB_INCLUDE) \
             $(OOPS_MODPLUG_INCLUDE) $(OOPS_OPENAL_INCLUDE) $(OOPS_SDL_INCLUDE)

CENSUS_SETS := love libs_cxx libs_c
CENSUS_love_SRCS := $(BLTR_LOVE_SRCS)
CENSUS_love_FLAGS = -x c++ $(BLTR_BASE) -std=c++11 $(OOPS_LIBCXX_INCLUDE) $(BLTR_SYS) $(BLTR_INC)
CENSUS_libs_cxx_SRCS := $(BLTR_LIB_CXX_SRCS)
CENSUS_libs_cxx_FLAGS = -x c++ $(BLTR_BASE) -std=c++11 $(OOPS_LIBCXX_INCLUDE) $(BLTR_SYS) \
                        $(BLTR_INC) -include sys/select.h
CENSUS_libs_c_SRCS := $(BLTR_LIB_C_SRCS)
# ENet's `unix.h` takes `fd_set` from `<sys/types.h>`, which FreeBSD's includes `<sys/select.h>`
# to provide; here the two are separate headers, so the C libraries see it first.
CENSUS_libs_c_FLAGS = $(BLTR_BASE) -std=gnu11 $(BLTR_SYS) $(BLTR_INC) -include sys/select.h

OOPS_FEATURES := display input keyboard mouse audio suspend thread draw dialog

# `__tls_get_addr` is libkernel's, which `common/symbols.txt` records, so the loader resolves it;
# the C++ runtime's thread-locals reach it, as `../spaghetti-kart`'s do.
EXTRA_UNDEF_ALLOW := ^__tls_get_addr$$

include $(OOPS_APPS_ROOT)/common/deps.mk

ifeq ($(BLTR_ARMED),1)
# --- the payload ------------------------------------------------------------------------
#
# LÖVE and its bundled libraries in one archive, linked whole: `main` is in there, and the Lua
# modules register themselves by table, which a plain archive link would drop.
BLTR_BUILD := build/love

define bltr_objs
BLTR_$(1)_OBJS := $$(call oops_objs,$$(BLTR_BUILD)/$(1),$$(CENSUS_$(1)_SRCS))
$$(call oops_obj_rules,$$(BLTR_BUILD)/$(1),TARGET_CC,CENSUS_$(1)_FLAGS,$$(CENSUS_$(1)_SRCS))
-include $$(BLTR_$(1)_OBJS:.o=.d)
endef
$(foreach s,$(CENSUS_SETS),$(eval $(call bltr_objs,$(s))))

BLTR_LIB := $(BLTR_BUILD)/liblove.a
BLTR_OBJS := $(BLTR_love_OBJS) $(BLTR_libs_cxx_OBJS) $(BLTR_libs_c_OBJS)
$(BLTR_OBJS): | $(OOPS_MODPLUG_PREFIX_STAMP)
$(BLTR_LIB): $(BLTR_OBJS)
	@rm -f $@
	@a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $(BLTR_OBJS)
	@echo "balatro: $@ ($(words $(BLTR_OBJS)) objects)"

# The entry shim is C++ (LÖVE's `main` is), and brings the C++ runtime with it.
OOPS_CXX_SRCS := $(CURDIR)/shim/bltr_start.cpp
include $(OOPS_APPS_ROOT)/common/cxx.mk
OOPS_CXX_INCLUDE += $(OOPS_LIBCXX_INCLUDE)
OOPS_CXX_FLAGS += -I$(OOPS_APPS_ROOT)/common $(OOPS_POSIX_INCLUDE) -DOOPS_POSIX_HOME=\"/app0\"

# The POSIX shim's headers before SDL's: `sdl2/include` carries a `unistd.h` of its own.
EXTRA_TARGET_CFLAGS += -DOOPS_CXX_EXTERNAL_NEW_DELETE $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE) \
                       -I$(OOPS_APPS_ROOT)/common -DOOPS_POSIX_HOME=\"/app0\"
PAYLOAD_SRCS := $(OOPS_APPS_ROOT)/common/player_data.c $(OOPS_APPS_ROOT)/common/crashtrace.c \
                $(OOPS_POSIX_SRCS)
EXTRA_TARGET_LDFLAGS += -Wl,--whole-archive $(BLTR_LIB) -Wl,--no-whole-archive \
                        $(OOPS_CXX_LDFLAGS) $(OOPS_LUAJIT_LDFLAGS) $(OOPS_MODPLUG_LDFLAGS) \
                        $(OOPS_THEORA_LDFLAGS) $(OOPS_VORBIS_LDFLAGS) $(OOPS_OPENAL_LDFLAGS) \
                        $(OOPS_FT_LDFLAGS) $(OOPS_ZLIB_LDFLAGS) $(OOPS_SDL_LDFLAGS) \
                        $(OOPS_LIBCXX_LDFLAGS) $(OOPS_LIBCXXABI_LDFLAGS) $(OOPS_LIBUNWIND_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(BLTR_LIB) $(OOPS_CXX_LIB) $(OOPS_LUAJIT_LIB) $(OOPS_MODPLUG_LIB) \
                        $(OOPS_THEORA_LIB) $(OOPS_VORBIS_LIB) $(OOPS_OGG_LIB) $(OOPS_OPENAL_LIBS) \
                        $(OOPS_FT_LIB) $(OOPS_ZLIB_LIB) $(OOPS_SDL_LIB) $(OOPS_LIBCXX_LIB) \
                        $(OOPS_LIBCXXABI_LIB) $(OOPS_LIBUNWIND_LIB)
endif

include $(OOPS_APPS_ROOT)/common/app.mk
include $(OOPS_APPS_ROOT)/common/scaffold.mk
include $(OOPS_APPS_ROOT)/common/census.mk

# The header views the sources include through, which a census compiles against too.
census: $(OOPS_MODPLUG_PREFIX_STAMP)
