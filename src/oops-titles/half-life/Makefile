# Half-Life, through oops-sdk.
#
#   make title     the payload, packaged
#   make survey    the pins, the dependencies and what the player supplies
#   make census    compile every component's sources for the target, set by set
#   make census-one S=<set> F=<path>   every error in one source
#
# Xash3D FWGS (`upstream/`) running hlsdk-portable's game code (`upstream-hlsdk/`), both linked
# into one payload under upstream's own `XASH_STATIC_LIBS` mode. The player's `valve/` directory is
# the data, read at run time.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# `ref/gl`, the engine's OpenGL renderer, is fixed-function GL 1.x.
OOPS_RENDERER = gl1

include $(OOPS_APPS_ROOT)/common/upstream.mk

XASH := $(CURDIR)/upstream
HLSDK := $(CURDIR)/upstream-hlsdk

# SDL2 for the window, input and audio. The codecs are the ones already vendored here rather
# than the copies the engine carries as submodules under `3rdparty/`.
# Armed by default: the payload links with nothing unresolved. `make HALF_ARMED=0` is the
# survey and census alone.
HALF_ARMED ?= 1
SCAFFOLD_ARM    := HALF_ARMED
SCAFFOLD_DEPS   := sdl2 libcxx zlib libogg libvorbis opus opusfile freetype
SCAFFOLD_SUPPLY := the valve/ folder from the player's own Half-Life install, as \
                   /data/homebrew/HALF00001/valve/

OOPS_SDL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2)
include $(OOPS_SDL)/oops-sdl.mk
OOPS_FT ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/freetype)
include $(OOPS_FT)/oops-freetype.mk
OOPS_VORBIS ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libvorbis)
include $(OOPS_VORBIS)/oops-libvorbis.mk
OOPS_OPUSFILE ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/opusfile)
include $(OOPS_OPUSFILE)/oops-opusfile.mk
include $(OOPS_APPS_ROOT)/common/posix.mk

OOPS_CXX_EXCEPTIONS = 1
OOPS_CXX_STD        = c++17
OOPS_LIBCXX ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libcxx)
include $(OOPS_LIBCXX)/oops-libcxx.mk
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk

# --- the components, transcribed from their wscripts ---------------------------
#
# Each is what upstream builds as one library. In a static build the engine is the program and
# the others are relocatable objects whose only global symbol is their export table
# (`scripts/waifulib/xshlib.py`), which is what lets the client and the server each carry their
# own `pm_shared` without colliding.

# `public/`, the utility library everything links. `build_vcs.c` takes the pin as its commit.
HL_PUBLIC_SRCS := $(wildcard $(XASH)/public/*.c)

# `engine/wscript`: the dedicated core, the POSIX and SDL2 platform layers and, for a client
# build, the whole `client/` tree. Upstream's `platform/misc/lib_static.c` is replaced by
# `shim/lib_static_oops.c` and `platform/posix/id_posix.c` by `shim/id_oops.c` - see their
# headers. `crash_libbacktrace.c` needs libbacktrace, which is not built; `crash_posix.c` is the
# one `common/crashtrace.c` would fight, and is out too.
HL_ENGINE_SRCS := $(filter-out %/crash_libbacktrace.c %/crash_posix.c %/id_posix.c, \
    $(wildcard $(XASH)/engine/common/*.c $(XASH)/engine/common/imagelib/*.c \
               $(XASH)/engine/common/soundlib/*.c $(XASH)/engine/server/*.c \
               $(XASH)/engine/common/http/*.c $(XASH)/engine/platform/posix/*.c \
               $(XASH)/engine/platform/sdl2/*.c) \
    $(shell find $(XASH)/engine/client -name '*.c' 2>/dev/null | sort))

# The engine's three small bundled libraries: bzip2, the Steam-ticket emulator and the library
# suffix table.
HL_3RD_SRCS := $(addprefix $(XASH)/3rdparty/bzip2/bzip2/, \
                   blocksort.c huffman.c crctable.c randtable.c compress.c decompress.c bzlib.c) \
               $(wildcard $(XASH)/3rdparty/MultiEmulator/src/*.c) \
               $(XASH)/3rdparty/library_suffix/src/library_suffix.c

HL_FS_SRCS := $(filter-out %/android.c,$(wildcard $(XASH)/filesystem/*.c))
HL_FS_CXX_SRCS := $(wildcard $(XASH)/filesystem/*.cpp)
# `ref/common` is the `ref_common` static library `ref/gl/wscript` links in.
HL_REF_SRCS := $(wildcard $(XASH)/ref/gl/*.c $(XASH)/ref/gl/gl2_shim/*.c $(XASH)/ref/common/*.c)
HL_MENU_SRCS := $(wildcard $(addprefix $(XASH)/3rdparty/mainui/, \
    *.cpp miniutl/*.cpp font/*.cpp menus/*.cpp menus/dynamic/*.cpp model/*.cpp controls/*.cpp))

# `cl_dll/wscript` and `dlls/wscript`. The client carries the weapon code the server also has,
# for prediction, and freevgui, statically (`INTERNAL_VGUI_SUPPORT`). Both take hlsdk's own
# `strlcpy` and `strlcat`, which its wscripts add when the libc has none, and
# `game_shared/vcs_info.c`, the `vcs_info` library both link.
#
# The client's exclusions are upstream's, and they are paths: `game_shared/voice_status.cpp`
# is out, `cl_dll/voice_status.cpp` - which defines `CVoiceStatus` - is in.
HL_OPENBSD := $(HLSDK)/external/openbsd/strlcpy.c $(HLSDK)/external/openbsd/strlcat.c \
              $(HLSDK)/game_shared/vcs_info.c
HL_CLIENT_EXCL := %/GameStudioModelRenderer_Sample.cpp $(HLSDK)/game_shared/voice_vgui_tweakdlg.cpp \
                  $(HLSDK)/game_shared/voice_gamemgr.cpp $(HLSDK)/game_shared/voice_status.cpp
HL_CLIENT_SRCS := $(filter-out $(HL_CLIENT_EXCL), \
    $(shell find $(HLSDK)/cl_dll -name '*.cpp' 2>/dev/null | sort) \
    $(wildcard $(HLSDK)/game_shared/*.cpp) $(HLSDK)/public/safe_snprintf.c \
    $(wildcard $(HLSDK)/pm_shared/*.c) $(HL_OPENBSD) \
    $(addprefix $(HLSDK)/dlls/, crossbow.cpp crowbar.cpp egon.cpp gauss.cpp glock.cpp \
        handgrenade.cpp hornetgun.cpp mp5.cpp python.cpp rpg.cpp satchel.cpp shotgun.cpp \
        squeakgrenade.cpp tripmine.cpp))
# `freevgui/wscript` for a static, engine-supporting build: the toolkit, the POSIX platform
# layer, and `platform/xash3d-fwgs`, whose entry point `INTERNAL_VGUI_SUPPORT` renames to the
# `InitVGUISupportAPI` the engine looks up in the client.
HL_VGUI_SRCS := $(wildcard $(addprefix $(HLSDK)/freevgui/, *.cpp controls/*.cpp \
                    platform/common/*.cpp platform/posix/*.cpp platform/xash3d-fwgs/*.cpp))
HL_SERVER_SRCS := $(filter-out %/mpstubb.cpp %/stats.cpp %/Wxdebug.cpp, \
    $(shell find $(HLSDK)/dlls -name '*.cpp' 2>/dev/null | sort) \
    $(wildcard $(HLSDK)/pm_shared/*.c) $(HLSDK)/public/safe_snprintf.c \
    $(HLSDK)/game_shared/voice_gamemgr.cpp $(HL_OPENBSD))

# --- flags -----------------------------------------------------------------------
#
# Upstream's warnings are its own business: `-w`, as every vendored tree here builds. The SDK's
# libc comes after libc++ for the C++ sets, since libc++ ships its own <math.h> and <stdlib.h>.
#
# `HAVE_DIRENT_D_TYPE` is left undefined: the shared `dirent` has only `d_name`, and the
# filesystem falls back to `stat`. `MINIZ_NO_TIME` keeps miniz from setting file times through
# `utime`, which it only does when extracting to disk.
#
# The `HAVE_*` set is `public/wscript`'s libc checks answered for the SDK: it has
# `strcasecmp`, `strncasecmp` and `strnlen`, and not `strcasestr`, `strchrnul`, `strlcpy` or
# `strlcat`, which `public/crtlib.c` then supplies itself. `alloca` is the builtin
# `<stdlib.h>` defines.
HL_BASE := -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib -nostdlibinc \
           -fPIC -ftls-model=initial-exec -O2 -w -fcommon -DXASH_STATIC_LIBS=1 -DXASH_SDL=2 -DXASH_GAMEDIR=\"valve\" \
           -DXASH_REF_GL_ENABLED=1 -DMINIZ_NO_TIME=1 -DOOPS -D_GNU_SOURCE=1 \
           -DHAVE_STRCASECMP=1 -DHAVE_STRNCASECMP=1 -DHAVE_STRNLEN=1 '-DALLOCA_H=<stdlib.h>'
HL_SYS  = $(OOPS_POSIX_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)
HL_PUB  := -I$(XASH)/public -I$(XASH)/common -I$(XASH)/pm_shared -I$(XASH)/filesystem \
           -I$(XASH)/engine -I$(XASH)/3rdparty/library_suffix/include
HL_C    = $(HL_BASE) -std=gnu11 $(HL_PUB) $(HL_SYS)
HL_CXX  = -x c++ $(HL_BASE) -std=c++17 $(OOPS_LIBCXX_INCLUDE) $(HL_SYS)

CENSUS_SETS := public engine thirdparty filesystem filesystem_cxx ref_gl menu client client_c vgui \
               server server_c

# `hlsdk/wscript:258` supplies the Windows names for these on every other platform.
HL_GAME_DEFS := -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -D_snprintf=snprintf \
                -D_vsnprintf=vsnprintf

CENSUS_public_SRCS := $(HL_PUBLIC_SRCS)
CENSUS_public_FLAGS = $(HL_C) -DXASH_BUILD_COMMIT=\"$(UPSTREAM_REV)\" \
                      -DXASH_BUILD_BRANCH=\"master\" -DXASH_BUILD_COMMIT_DATE=\"2026-09-28\"

CENSUS_engine_SRCS := $(HL_ENGINE_SRCS) $(CURDIR)/shim/lib_static_oops.c \
                      $(CURDIR)/shim/id_oops.c
CENSUS_engine_FLAGS = $(HL_C) -DENGINE_DLL=1 \
    -I$(XASH)/engine/common -I$(XASH)/engine/common/imagelib -I$(XASH)/engine/common/soundlib \
    -I$(XASH)/engine/server -I$(XASH)/engine/client -I$(XASH)/engine/client/vgui \
    -I$(XASH)/engine/platform -I$(XASH)/3rdparty/MultiEmulator/include \
    -I$(XASH)/3rdparty/bzip2/bzip2 -I$(CURDIR)/shim/include \
    $(OOPS_SDL_INCLUDE) $(OOPS_OPUSFILE_INCLUDE) $(OOPS_OPUS_INCLUDE) $(OOPS_VORBIS_INCLUDE) \
    $(OOPS_OGG_INCLUDE)

CENSUS_thirdparty_SRCS := $(HL_3RD_SRCS)
CENSUS_thirdparty_FLAGS = $(HL_C) -I$(XASH)/3rdparty/MultiEmulator/include \
    -I$(XASH)/3rdparty/MultiEmulator/src -I$(XASH)/3rdparty/bzip2/bzip2 -I$(CURDIR)/shim/include

CENSUS_filesystem_SRCS := $(HL_FS_SRCS)
CENSUS_filesystem_FLAGS = $(HL_C)
CENSUS_filesystem_cxx_SRCS := $(HL_FS_CXX_SRCS)
CENSUS_filesystem_cxx_FLAGS = $(HL_CXX) '-DSTDINT_H=<cstdint>' $(HL_PUB)

CENSUS_ref_gl_SRCS := $(HL_REF_SRCS)
CENSUS_ref_gl_FLAGS = $(HL_C) -DREF_DLL=1 -I$(XASH)/ref/gl -I$(XASH)/ref/common \
    -I$(XASH)/engine/common -I$(XASH)/engine/common/imagelib -I$(XASH)/engine/client \
    -I$(XASH)/engine/server $(OOPS_SDL_INCLUDE)

HL_MENU_INC := . miniutl font controls menus model sdk_includes/common sdk_includes/engine \
               sdk_includes/public sdk_includes/pm_shared
CENSUS_menu_SRCS := $(HL_MENU_SRCS)
CENSUS_menu_FLAGS = $(HL_CXX) -fno-rtti -DMAINUI_USE_FREETYPE=1 -DMAINUI_USE_CUSTOM_FONT_RENDER=1 \
    '-DSTDINT_H=<cstdint>' \
    $(addprefix -I$(XASH)/3rdparty/mainui/,$(HL_MENU_INC)) $(OOPS_FT_INCLUDE)

HL_GAME_INC := -I$(HLSDK)/dlls -I$(HLSDK)/common -I$(HLSDK)/engine -I$(HLSDK)/pm_shared \
               -I$(HLSDK)/game_shared -I$(HLSDK)/public
# The game libraries' `.c` sources - `pm_shared`, `safe_snprintf`, the OpenBSD string
# functions - are C and compile as C, in sets of their own: compiled as C++ they would mangle,
# and `pm_math.c`'s `Length` would collide with `cl_dll/util.cpp`'s, which upstream's build
# keeps apart by language.
CENSUS_client_SRCS := $(filter %.cpp,$(HL_CLIENT_SRCS))
# `shim/include` for `memory.h`, which `cl_dll/entity.cpp` includes (see the header).
CENSUS_client_FLAGS = $(HL_CXX) $(HL_GAME_DEFS) -DCLIENT_DLL=1 -DINTERNAL_VGUI_SUPPORT=1 \
    -I$(HLSDK)/cl_dll $(HL_GAME_INC) -I$(HLSDK)/utils/vgui/include -I$(CURDIR)/shim/include
CENSUS_client_c_SRCS := $(filter %.c,$(HL_CLIENT_SRCS))
CENSUS_client_c_FLAGS = $(HL_BASE) -std=gnu11 $(HL_SYS) $(HL_GAME_DEFS) -DCLIENT_DLL=1 \
    -I$(HLSDK)/cl_dll $(HL_GAME_INC)

CENSUS_vgui_SRCS := $(HL_VGUI_SRCS)
CENSUS_vgui_FLAGS = $(HL_CXX) $(HL_GAME_DEFS) -DINTERNAL_VGUI_SUPPORT=1 -I$(HLSDK)/freevgui \
    -I$(HLSDK)/utils/vgui/include -I$(HLSDK)/engine -I$(HLSDK)/common

CENSUS_server_SRCS := $(filter %.cpp,$(HL_SERVER_SRCS))
CENSUS_server_FLAGS = $(HL_CXX) $(HL_GAME_DEFS) -I$(HLSDK)/dlls $(HL_GAME_INC)
CENSUS_server_c_SRCS := $(filter %.c,$(HL_SERVER_SRCS))
CENSUS_server_c_FLAGS = $(HL_BASE) -std=gnu11 $(HL_SYS) $(HL_GAME_DEFS) -I$(HLSDK)/dlls \
    $(HL_GAME_INC)

# `net` for the engine's loopback server, which a single-player game runs through.
OOPS_FEATURES := display input keyboard mouse audio suspend thread draw dialog net

include $(OOPS_APPS_ROOT)/common/deps.mk

ifeq ($(HALF_ARMED),1)
# --- the payload -------------------------------------------------------------------
#
# The engine, `public/` and the three bundled libraries are one archive, linked whole: the
# engine is the program, and `Host_Main` is in there. Each census set compiles under its own
# flags into its own object directory.
HL_BUILD := build/hl

define hl_objs
HL_$(1)_OBJS := $$(call oops_objs,$$(HL_BUILD)/$(1),$$(CENSUS_$(1)_SRCS))
$$(call oops_obj_rules,$$(HL_BUILD)/$(1),TARGET_CC,CENSUS_$(1)_FLAGS,$$(CENSUS_$(1)_SRCS))
-include $$(HL_$(1)_OBJS:.o=.d)
endef
$(foreach s,$(CENSUS_SETS),$(eval $(call hl_objs,$(s))))

HL_ENGINE_LIB := $(HL_BUILD)/libxash.a
HL_ENGINE_OBJS := $(HL_public_OBJS) $(HL_engine_OBJS) $(HL_thirdparty_OBJS)
$(HL_ENGINE_LIB): $(HL_ENGINE_OBJS)
	@rm -f $@
	@# A response file (`common/deps.mk`): the object list exceeds Windows' command line.
	@:$(call oops_rsp,$@.rsp,$(HL_ENGINE_OBJS))
	@a=$$(command -v $(AR) 2>/dev/null || command -v llvm-ar || command -v ar); \
	 "$$a" rcs $@ @$@.rsp
	@echo "half-life: $@ ($(words $(HL_ENGINE_OBJS)) objects)"

# The five libraries the engine loads, each as upstream's static build makes them
# (`scripts/waifulib/xshlib.py`): its objects linked into one relocatable object with a table of
# its exports, `lib_<name>_exports`, and every other symbol made local. That is what lets the
# client and the server each carry `pm_shared` and the weapon code without colliding, and what
# `shim/lib_static_oops.c` looks names up in.
#
# The filesystem, renderer and menu name their exports in upstream's `exports.txt`. hlsdk has no
# such list - on a desktop the engine finds entity spawn functions by `dlsym` - so the client's
# and the server's are every function their objects define, read back with `llvm-nm`. That
# includes C++ member functions under their mangled names, which is what save games record a
# think or touch callback as (see `COM_NameForFunction` in the shim).
HL_RELOC_CC := $(HL_BASE) -std=gnu11
HL_EXPORTS_filesystem_stdio := $(XASH)/filesystem/exports.txt
HL_EXPORTS_ref_gl := $(XASH)/ref/gl/exports.txt
HL_EXPORTS_menu := $(XASH)/3rdparty/mainui/exports.txt

# $(call hl_reloc_lib,<library name>,<objects>)
define hl_reloc_lib
$$(HL_BUILD)/$(1).o: $(2) $$(oops_makefiles)
	@ld.lld -r -o $$(HL_BUILD)/$(1).pre.o $(2)
	@$(if $(HL_EXPORTS_$(1)),grep -E '^[A-Za-z_][A-Za-z0-9_]*$$$$' $(HL_EXPORTS_$(1)),\
	    llvm-nm -g --defined-only $$(HL_BUILD)/$(1).pre.o \
	    | awk '$$$$2 == "T" || $$$$2 == "W" { print $$$$3 }' \
	    | grep -E '^[A-Za-z_][A-Za-z0-9_]*$$$$' | sort -u) > $$(HL_BUILD)/$(1).exports
	@{ echo 'typedef struct { const char *name; void *func; } table_t;'; \
	   sed 's/.*/extern void &(void);/' $$(HL_BUILD)/$(1).exports; \
	   echo 'table_t lib_$(1)_exports[] = {'; \
	   sed 's/.*/{ "&", (void *)\&& },/' $$(HL_BUILD)/$(1).exports; \
	   echo '{ 0, 0 } };'; } > $$(HL_BUILD)/$(1).table.c
	@$$(TARGET_CC) $$(HL_RELOC_CC) -c -o $$(HL_BUILD)/$(1).table.o $$(HL_BUILD)/$(1).table.c
	@ld.lld -r -o $$(HL_BUILD)/$(1).unstripped.o $$(HL_BUILD)/$(1).pre.o $$(HL_BUILD)/$(1).table.o
	@llvm-objcopy -G lib_$(1)_exports $$(HL_BUILD)/$(1).unstripped.o $$@
	@echo "half-life: $$@ ($$$$(wc -l < $$(HL_BUILD)/$(1).exports) exports)"
endef
$(eval $(call hl_reloc_lib,filesystem_stdio,$(HL_filesystem_OBJS) $(HL_filesystem_cxx_OBJS)))
$(eval $(call hl_reloc_lib,ref_gl,$(HL_ref_gl_OBJS)))
$(eval $(call hl_reloc_lib,menu,$(HL_menu_OBJS)))
$(eval $(call hl_reloc_lib,client,$(HL_client_OBJS) $(HL_client_c_OBJS) $(HL_vgui_OBJS)))
$(eval $(call hl_reloc_lib,hl,$(HL_server_OBJS) $(HL_server_c_OBJS)))

HL_RELOC := $(addprefix $(HL_BUILD)/,filesystem_stdio.o ref_gl.o menu.o client.o hl.o)

# The C++ runtime (`common/cxxrt.cpp`: `__cxa_atexit`, the static-init guards) comes with
# `cxx.mk`, which builds it beside the title's own C++ - here one small shim.
OOPS_CXX_SRCS := $(CURDIR)/shim/hl_platform.cpp
include $(OOPS_APPS_ROOT)/common/cxx.mk
OOPS_CXX_INCLUDE += $(OOPS_LIBCXX_INCLUDE)

# libc++ owns new/delete for a title linking the full archive (`../bugdom/docs/PORTING.md`).
# The POSIX shim's headers before SDL's: `sdl2/include` carries a `unistd.h` of its own, and
# `posix.c` needs the full one.
EXTRA_TARGET_CFLAGS += -DOOPS_CXX_EXTERNAL_NEW_DELETE $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE) \
                       -I$(OOPS_APPS_ROOT)/common -DOOPS_POSIX_HOME=\"/app0\"
PAYLOAD_SRCS := shim/half_life_start.c $(OOPS_APPS_ROOT)/common/player_data.c \
                $(OOPS_APPS_ROOT)/common/crashtrace.c $(OOPS_POSIX_SRCS)
EXTRA_TARGET_LDFLAGS += -Wl,--whole-archive $(HL_ENGINE_LIB) -Wl,--no-whole-archive $(HL_RELOC) \
                        $(OOPS_CXX_LDFLAGS) \
                        $(OOPS_FT_LDFLAGS) $(OOPS_OPUSFILE_LDFLAGS) $(OOPS_VORBIS_LDFLAGS) \
                        $(OOPS_SDL_LDFLAGS) $(OOPS_LIBCXX_LDFLAGS) $(OOPS_LIBCXXABI_LDFLAGS) \
                        $(OOPS_LIBUNWIND_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(HL_ENGINE_LIB) $(HL_RELOC) $(OOPS_CXX_LIB) $(OOPS_FT_LIB) $(OOPS_OPUSFILE_LIB) \
                        $(OOPS_OPUS_LIB) $(OOPS_VORBIS_LIB) $(OOPS_OGG_LIB) $(OOPS_SDL_LIB) \
                        $(OOPS_LIBCXX_LIB) $(OOPS_LIBCXXABI_LIB) $(OOPS_LIBUNWIND_LIB)
endif

include $(OOPS_APPS_ROOT)/common/app.mk
include $(OOPS_APPS_ROOT)/common/scaffold.mk
include $(OOPS_APPS_ROOT)/common/census.mk
