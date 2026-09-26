# SuperTux, through oops-sdk - on oops-gl's GL 2.0 / GLSL path.
#
#   make              the payload (`FORMATS` in app.env)
#   make package      the title package with SuperTux's data, which is what runs
#   make check        every GL entry point the ES 2.0 renderer calls, against oops-gl
#   make shadercheck  upstream's `shader100.*`, through oops-gl's compiler and code generator

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Included first because the wildcards below read `upstream/` while this file is parsed.
# `common/app.mk` includes it again at the bottom; the include is guarded.
include $(OOPS_APPS_ROOT)/common/upstream.mk

# ---------------------------------------------------------------------------
# The renderer: upstream's ES 2.0 build, which is the shader path
# ---------------------------------------------------------------------------
#
# SuperTux chooses its renderer at compile time:
#
#   -DUSE_OPENGLES2   GL33CoreContext, with `data/shader/shader100.*` as its one program
#   -DUSE_OPENGLES1   GL20Context, fixed-function
#   neither           GL33CoreContext with `shader330.*`, falling back to GL20Context by exception
#
# This port uses `USE_OPENGLES2`. `shader100.*` is `#version 100`, which oops-gl accepts as
# GLSL 1.10 (`make shadercheck`); its three samplers fit oops-gl's four texture units and
# carry the water and heat-haze effects. oops-gl refuses GLSL 3.30, so the default would
# reach GL20Context only through an exception.
#
# The define also empties `gl20_context.cpp` and `gl_pixel_request.cpp`, makes the three
# vertex-array-object calls inline no-ops, and makes `video/gl.hpp` include
# `<SDL_opengles2.h>`, which `shim/include/` maps to oops-gl's headers.
#
# The fixed-function alternative is neither define with `video` set to `opengl20` in the
# config, which needs a `GL/glew.h` in the shim. `USE_OPENGLES1` maps `glOrtho` to ES 1's
# `glOrthof`.
OOPS_RENDERER = gl2
OOPS_RENDERER_DEFINE := -DUSE_OPENGLES2

# ---------------------------------------------------------------------------
# What upstream brings
# ---------------------------------------------------------------------------
#
# Upstream's CMake selection: `CMakeLists.txt` globs `src/*/*.cpp`, `src/supertux/menu/*.cpp`
# and `src/video/{sdl,null,gl}/*.cpp`, and builds `src/main.cpp` separately. Optional features
# (Discord, touchscreen, binreloc) are `config.h` switches, set in `shim/include/config.h`.
STX_CXX_SRCS := $(sort $(wildcard upstream/src/*/*.cpp) \
                       $(wildcard upstream/src/supertux/menu/*.cpp) \
                       $(wildcard upstream/src/video/sdl/*.cpp) \
                       $(wildcard upstream/src/video/null/*.cpp) \
                       $(wildcard upstream/src/video/gl/*.cpp)) \
                upstream/src/main.cpp

# Vendored in upstream's `external/` rather than submodules. CMake builds each as a small
# library; here they are more sources.
STX_VENDORED_C   := $(wildcard upstream/external/obstack/*.c) \
                    upstream/external/findlocale/findlocale.c \
                    upstream/external/SDL_SavePNG/savepng.c
STX_VENDORED_CXX := upstream/external/partio_zip/zip_manager.cpp

STX_SHIM_CXX     := $(wildcard shim/*.cpp)

# The four game submodules, fetched by `UPSTREAM_SUBMODULES=1` (see `upstream.lock`). Each
# list is what upstream's build of that library compiles.
#
# PhysFS is the core, the `dir` and `zip` archivers and the POSIX platform layer; the other
# archivers read formats the game does not use and `PHYSFS_SUPPORTS_*=0` turns them off.
# clang defines `__FreeBSD__` here, selecting `physfs_platform_unix.c`, whose CD-ROM probing
# calls `getmntinfo`; `PHYSFS_NO_CDROM_SUPPORT` removes it.
STX_PHYSFS_C := $(addprefix upstream/external/physfs/src/, \
                  physfs.c physfs_byteorder.c physfs_unicode.c \
                  physfs_archiver_dir.c physfs_archiver_unpacked.c physfs_archiver_zip.c \
                  physfs_platform_posix.c physfs_platform_unix.c)
STX_PHYSFS_CFLAGS := -DPHYSFS_NO_CDROM_SUPPORT=1 \
                     $(foreach a,7Z GRP HOG ISO9660 MVL QPAK SLB VDF WAD,-DPHYSFS_SUPPORTS_$(a)=0)

# Squirrel and its standard library - `squirrel_static` and `sqstdlib_static` in upstream's CMake.
STX_SQUIRREL_CXX := $(wildcard upstream/external/squirrel/squirrel/*.cpp) \
                    $(wildcard upstream/external/squirrel/sqstdlib/*.cpp)

STX_SEXP_CXX        := $(wildcard upstream/external/sexp-cpp/src/*.cpp)
STX_TINYGETTEXT_CXX := $(wildcard upstream/external/tinygettext/src/*.cpp)

# Upstream includes by path from `src/` (`#include "supertux/main.hpp"`). `shim/include` goes
# first so that `config.h`, `version.h`, `SDL_opengles2.h`, `boost/` and `curl/` are ours.
#
# The library defines go to every file because the game includes those libraries' headers
# too, and one value everywhere keeps the ODR: `_SQ64` sizes Squirrel's integers (upstream
# adds it on 64-bit), `HAVE_SDL` makes tinygettext convert through `SDL_iconv`, and
# `SEXP_USE_LOCALE` makes sexp-cpp parse numbers in the classic locale.
STX_INCLUDE := -Ishim/include -Iupstream/src \
               -Iupstream/external/obstack -Iupstream/external/findlocale \
               -Iupstream/external/SDL_SavePNG -Iupstream/external/partio_zip \
               -Iupstream/external/physfs/src -Iupstream/external/squirrel/include \
               -Iupstream/external/sexp-cpp/include -Iupstream/external/tinygettext/include
STX_DEFINES := $(OOPS_RENDERER_DEFINE) -D_SQ64 -DHAVE_SDL -DSEXP_USE_LOCALE

# ---------------------------------------------------------------------------
# The stack
# ---------------------------------------------------------------------------
#
# The game uses C++ exceptions and RTTI throughout. `OOPS_CXX_EXCEPTIONS = 1` links libc++abi
# and libunwind, the configuration `src/oops-utilities/cxx-throw` exercises on hardware.
#
# `OOPS_CXX_LINK_OBJECTS = 1` because object file names collide (`object/camera.cpp`,
# `scripting/camera.cpp`) and an archive keeps one of each; see `common/cxx.mk`.
#
# C++17 rather than upstream's 14: the shim's `boost::optional` is `std::optional`.
OOPS_CXX_EXCEPTIONS   = 1
OOPS_CXX_LINK_OBJECTS = 1
OOPS_CXX_STD          = c++17
OOPS_CXX_SRCS := $(STX_CXX_SRCS) $(STX_VENDORED_CXX) $(STX_SQUIRREL_CXX) $(STX_SEXP_CXX) \
                 $(STX_TINYGETTEXT_CXX) $(STX_SHIM_CXX)

OOPS_LIBCXX ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libcxx)
include $(OOPS_LIBCXX)/oops-libcxx.mk
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk
include $(OOPS_APPS_ROOT)/common/cxx.mk

OOPS_SDL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2)
include $(OOPS_SDL)/oops-sdl.mk

# libpng brings zlib with it; SDL2_image reads libpng's, libjpeg's and SDL2's variables, so it
# goes after all three. The level backgrounds are JPEGs.
OOPS_PNG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libpng)
include $(OOPS_PNG)/oops-libpng.mk
OOPS_JPEG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libjpeg-turbo)
include $(OOPS_JPEG)/oops-libjpeg.mk
OOPS_IMG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2-image)
include $(OOPS_IMG)/oops-sdl2-image.mk

OOPS_FT ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/freetype)
include $(OOPS_FT)/oops-freetype.mk
OOPS_TTF ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2-ttf)
include $(OOPS_TTF)/oops-sdl2-ttf.mk

# vorbisfile for `ogg_sound_file.cpp`. `oops-libvorbis.mk` includes libogg itself, so naming it
# here too would define its rules twice.
OOPS_VORBIS ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libvorbis)
include $(OOPS_VORBIS)/oops-libvorbis.mk

OOPS_GLM ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/glm)
include $(OOPS_GLM)/oops-glm.mk

# The POSIX shim - `stat`, `opendir`, `getcwd` and the rest, which PhysFS's POSIX platform
# layer calls.
include $(OOPS_APPS_ROOT)/common/posix.mk

# After SDL2, libc++ and the POSIX shim, whose include variables it reads.
OOPS_OPENAL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/openal-soft)
include $(OOPS_OPENAL)/oops-openal-soft.mk

STX_DEP_INCLUDE := $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE) $(OOPS_IMG_INCLUDE) \
                   $(OOPS_TTF_INCLUDE) $(OOPS_FT_INCLUDE) $(OOPS_PNG_INCLUDE) \
                   $(OOPS_VORBIS_INCLUDE) $(OOPS_GLM_INCLUDE) $(OOPS_OPENAL_INCLUDE)

# Four standard headers upstream uses without including, forced in for C++ only:
# `editor/object_option.hpp` uses `std::function` without `<functional>`,
# `util/string_util.cpp` calls `isdigit` without `<cctype>`, and others use
# `std::ostringstream` and `std::cout` without `<sstream>` or `<iostream>`. libstdc++
# reaches these transitively; libc++ does not.
OOPS_CXX_INCLUDE     += $(OOPS_LIBCXX_INCLUDE) -include functional -include cctype \
                        -include sstream -include iostream
EXTRA_TARGET_CFLAGS  += $(STX_INCLUDE) $(STX_DEFINES) $(STX_DEP_INCLUDE)

# ---------------------------------------------------------------------------
# The C sources: PhysFS and three vendored helpers, under upstream's flags
# ---------------------------------------------------------------------------
#
# Objects on the link line rather than `PAYLOAD_SRCS`: `app.mk` builds payload C with this
# repository's `-Werror -Wconversion`, which upstream C does not meet.
include $(OOPS_APPS_ROOT)/common/deps.mk

STX_C_SRCS   := $(STX_PHYSFS_C) $(STX_VENDORED_C)
STX_C_BUILD  := build/c
# The header order is `app.mk`'s - oops-sdk's libc, then the POSIX shim - and `shim/include`
# ahead of both, for its `<pthread.h>`. `stx_physfs_fcntl.h` is forced in for the two `fcntl`
# names PhysFS's flush uses, which oops-sdk's `<fcntl.h>` (found first) does not declare; the
# header says why reordering the path does not solve it.
STX_C_CFLAGS  = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                -nostdlibinc -fPIC -O2 -w -std=gnu11 $(STX_PHYSFS_CFLAGS) \
                $(STX_INCLUDE) $(STX_DEFINES) -include stx_physfs_fcntl.h \
                $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE) $(OOPS_POSIX_INCLUDE) \
                $(OOPS_SDL_INCLUDE) $(OOPS_PNG_INCLUDE)
STX_C_OBJS   := $(call oops_objs,$(STX_C_BUILD)/obj,$(STX_C_SRCS))
-include $(STX_C_OBJS:.o=.d)
$(call oops_obj_rules,$(STX_C_BUILD)/obj,TARGET_CC,STX_C_CFLAGS,$(STX_C_SRCS))

# ---------------------------------------------------------------------------
# The link
# ---------------------------------------------------------------------------
#
# The title's own objects first, then each library after everything that calls it - `app.mk`
# puts LDFLAGS ahead of `PAYLOAD_SRCS`, and a plain archive seen before its callers contributes
# nothing. SDL2_image's flags carry libpng, zlib and libjpeg; libvorbis's carry libogg; FreeType
# follows SDL2_ttf; SDL2 is last of the C libraries because every one of them calls it.
EXTRA_TARGET_LDFLAGS += $(OOPS_CXX_LDFLAGS) $(STX_C_OBJS) $(OOPS_OPENAL_LDFLAGS) \
                        $(OOPS_IMG_LDFLAGS) $(OOPS_TTF_LDFLAGS) $(OOPS_FT_LDFLAGS) \
                        $(OOPS_VORBIS_LDFLAGS) $(OOPS_SDL_LDFLAGS) \
                        $(OOPS_LIBCXX_LDFLAGS) $(OOPS_LIBCXXABI_LDFLAGS) $(OOPS_LIBUNWIND_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(OOPS_CXX_LIB) $(STX_C_OBJS) $(OOPS_OPENAL_LIBS) \
                        $(OOPS_IMG_LIB) $(OOPS_PNG_LIB) $(OOPS_ZLIB_LIB) $(OOPS_JPEG_LIB) \
                        $(OOPS_TTF_LIB) $(OOPS_FT_LIB) $(OOPS_VORBIS_LIB) $(OOPS_OGG_LIB) \
                        $(OOPS_SDL_LIB) $(OOPS_LIBCXX_LIB) $(OOPS_LIBCXXABI_LIB) \
                        $(OOPS_LIBUNWIND_LIB)

OOPS_FEATURES := glu display input keyboard mouse audio suspend thread draw

PAYLOAD_SRCS = $(OOPS_POSIX_SRCS)

ENTRY_POINT := stx_start

# ---------------------------------------------------------------------------
# `make check`: every GL entry point the ES 2.0 renderer calls, against oops-gl
# ---------------------------------------------------------------------------
#
# A GL function oops-gl lacks is a link error; this reports it first. It reads
# `upstream/src/video/gl/*.cpp` and oops-sdk's `GL/gl.h` at run time and checks declarations,
# not behaviour.
HOST_TEST_SRCS = tools/glcheck.c

include $(OOPS_APPS_ROOT)/common/app.mk

# After the include: `common/app.mk` sets `CFLAGS` with `?=`, so an earlier assignment would
# replace its host warning set. Recipes expand variables when they run, so this still applies.
CFLAGS += -DSTX_GL_HEADER='"$(OOPS_SDK_DIR)/include/GL/gl.h"'

# ---------------------------------------------------------------------------
# `make shadercheck`: the ES 2.0 shaders through oops-gl
# ---------------------------------------------------------------------------
#
# Runs oops-sdk's `tools/shader-survey.sh` over `data/shader` and asserts that
# `shader100.frag` generates gfx1030 code (so it runs on the console, not only on the software
# reference) and `shader100.vert` compiles. `shader330.*` are listed but not asserted.
.PHONY: shadercheck
shadercheck:
	@out=$$(bash $(OOPS_SDK_DIR)/tools/shader-survey.sh --per-file upstream/data/shader 2>/dev/null); \
	echo "$$out" | grep -E '^(GENERATES|COMPILES|COMP-FAIL|GEN-FAIL)' | sed 's|  .*/data/shader/|  |'; \
	echo "$$out" | grep -qE '^GENERATES +.*/shader100\.frag' || \
	    { echo "supertux shadercheck: FAILED - shader100.frag does not generate for the console" >&2; exit 1; }; \
	echo "$$out" | grep -qE '^(GENERATES|COMPILES) +.*/shader100\.vert' || \
	    { echo "supertux shadercheck: FAILED - shader100.vert does not compile" >&2; exit 1; }; \
	echo "supertux shadercheck: the ES 2.0 program compiles, and its fragment shader generates"

# ---------------------------------------------------------------------------
# The data
# ---------------------------------------------------------------------------
#
# `make package` adds the game data to the title package from `app.mk`, which holds only the
# payload and `sce_sys/`; SuperTux reaches no menu without it. It lands at `/app0/data`, which
# `shim/stx_start.cpp` passes as `--datadir` and `config.h` names as `BUILD_DATA_DIR`.
#
# `data/locale/*.po` is kept: tinygettext reads the `.po` files directly.
.PHONY: package
package: title
	@test -d upstream/data || { echo "supertux: no upstream/data - run make first" >&2; exit 1; }
	@echo "supertux: copying $$(du -sh upstream/data | cut -f1) of data into the package"
	@rm -rf build/title/$(TITLE_ID)/data
	@cp -r upstream/data build/title/$(TITLE_ID)/data
	@echo "supertux: package is build/title/$(TITLE_ID) ($$(du -sh build/title/$(TITLE_ID) | cut -f1))"
