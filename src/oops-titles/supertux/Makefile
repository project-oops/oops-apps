# SuperTux, through oops-sdk - on oops-gl's GL 2.0 / GLSL path.
#
#   make              the payload (`FORMATS` in app.env)
#   make package      the title package **with SuperTux's 244 MB of data**, which is what runs
#   make check        every GL entry point the ES 2.0 renderer calls, against oops-gl
#   make shadercheck  upstream's `shader100.*`, through oops-gl's compiler and code generator

OOPS_APPS_ROOT ?= $(abspath ../../..)

# **First, because this file names files under `upstream/` while it is being read.** The fetch is
# normally done by `common/app.mk` at the bottom, which is too late for the wildcards below.
# `common/upstream.mk` says the rest; it is guarded, so `app.mk`'s own include costs nothing.
include $(OOPS_APPS_ROOT)/common/upstream.mk

# ---------------------------------------------------------------------------
# The renderer: upstream's ES 2.0 build, which is the shader path
# ---------------------------------------------------------------------------
#
# SuperTux has three renderers and chooses between them at **compile** time first:
#
#   -DUSE_OPENGLES2   GL33CoreContext, with `data/shader/shader100.*` as its one program
#   -DUSE_OPENGLES1   GL20Context, fixed-function
#   neither           GL33CoreContext with `shader330.*`, falling back to GL20Context by exception
#
# **This port takes `USE_OPENGLES2`.** `shader100.*` is `#version 100`, which oops-gl accepts as
# GLSL 1.10, and its fragment shader generates for gfx1030 - `make shadercheck`. It samples three
# textures, which oops-gl's four texture image units cover; the displacement and framebuffer
# samplers are what upstream's water and heat-haze effects are made of, and the fixed-function
# backend has neither.
#
# The default arm is not available: `shader330.*` is GLSL 3.30, which oops-gl refuses by version
# number, so it would throw on the first frame and land on the fixed-function backend anyway -
# the right renderer reached by the wrong route, through an exception, every launch.
#
# What the define also does, all of it upstream's and all of it checked by `make check`: it
# empties `gl20_context.cpp` and `gl_pixel_request.cpp`, replaces the three vertex-array-object
# calls with inline no-ops, and makes `video/gl.hpp` include `<SDL_opengles2.h>` - which
# `shim/include/` answers with oops-gl's own headers.
#
# **The fallback, if the shader path has to be given up:** build with neither define and set
# `video` to `opengl20` in the config, and GL20Context is the renderer - fixed-function, a subset
# of what Neverball already uses. It costs a `GL/glew.h` in the shim, which the ES 2.0 build does
# not include. `USE_OPENGLES1` is not the fallback: it maps `glOrtho` to ES 1's `glOrthof`.
OOPS_RENDERER = gl2
OOPS_RENDERER_DEFINE := -DUSE_OPENGLES2

# ---------------------------------------------------------------------------
# What upstream brings
# ---------------------------------------------------------------------------
#
# **Upstream's own CMake selection, transcribed.** `CMakeLists.txt` globs `src/*/*.cpp`,
# `src/supertux/menu/*.cpp`, `src/video/{sdl,null,gl}/*.cpp`, and builds `src/main.cpp` into the
# executable separately. Every optional feature in that tree is behind a `config.h` switch rather
# than a file list - Discord, touchscreen, binreloc - so the glob is the selection and
# `shim/include/config.h` is where each feature is turned off. 467 files, and `main.cpp`.
STX_CXX_SRCS := $(sort $(wildcard upstream/src/*/*.cpp) \
                       $(wildcard upstream/src/supertux/menu/*.cpp) \
                       $(wildcard upstream/src/video/sdl/*.cpp) \
                       $(wildcard upstream/src/video/null/*.cpp) \
                       $(wildcard upstream/src/video/gl/*.cpp)) \
                upstream/src/main.cpp

# Vendored in upstream's own `external/`, not submodules - they arrive with the fetch. CMake
# builds each as its own small library; here they are simply more sources.
STX_VENDORED_C   := $(wildcard upstream/external/obstack/*.c) \
                    upstream/external/findlocale/findlocale.c \
                    upstream/external/SDL_SavePNG/savepng.c
STX_VENDORED_CXX := upstream/external/partio_zip/zip_manager.cpp

STX_SHIM_CXX     := $(wildcard shim/*.cpp)

# **The four submodules the game is made of**, fetched with the tree by `UPSTREAM_SUBMODULES=1` -
# `upstream.lock` says why here and not `src/oops-deps/`. Each list is what upstream's own build
# of that library compiles, and no more.
#
# PhysFS is the library core, the `dir` and `zip` archivers and the POSIX platform layer. Its
# other archivers read game formats from the 1990s (`grp`, `hog`, `wad`...) and are parsers of
# input nothing here produces; `PHYSFS_SUPPORTS_*=0` turns each off. clang defines `__FreeBSD__`
# for this target, which selects `physfs_platform_unix.c` - and that file's CD-ROM probing calls
# `getmntinfo`, which is what `PHYSFS_NO_CDROM_SUPPORT` is for.
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

# **`-Iupstream/src`, because upstream includes by path from there** - `#include
# "supertux/main.hpp"`, never a relative path. `shim/include` goes first so that `config.h`,
# `version.h`, `SDL_opengles2.h`, `boost/` and `curl/` are ours.
#
# Three defines are each one library's, passed to every file because all of them are read from
# headers the game includes too - so one value everywhere is also the only ODR-safe one:
# `_SQ64` sizes Squirrel's integers (upstream's CMake adds it on 64-bit), `HAVE_SDL` makes
# tinygettext convert through `SDL_iconv` (upstream's CMake passes `-DHAVE_SDL=ON`), and
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
# **C++ with exceptions and RTTI, and there is no avoiding it.** 779 `throw`, 832 `catch`,
# 226 `dynamic_cast` - measured. `OOPS_CXX_EXCEPTIONS = 1` with libc++abi and libunwind is the
# route `src/oops-utilities/cxx-throw` proved on hardware (5/5), and this is the first title to
# take it.
#
# **`OOPS_CXX_LINK_OBJECTS = 1`** because thirty of the file names collide -
# `object/camera.cpp`, `scripting/camera.cpp`, `worldmap/camera.cpp` - and one archive keeps one
# of each. `common/cxx.mk` has the reasoning.
#
# **C++17**, one past upstream's 14: the shim's `boost::optional` is `std::optional` underneath.
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
# goes after all three. The 24 JPEGs are the level backgrounds.
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

# The POSIX shim - `stat`, `opendir`, `getcwd` and the rest. PhysFS's POSIX platform layer is
# what reaches it here. Shared with Neverball, Craft and Extreme Tux Racer.
include $(OOPS_APPS_ROOT)/common/posix.mk

# After SDL2, libc++ and the POSIX shim, whose include variables it reads.
OOPS_OPENAL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/openal-soft)
include $(OOPS_OPENAL)/oops-openal-soft.mk

STX_DEP_INCLUDE := $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE) $(OOPS_IMG_INCLUDE) \
                   $(OOPS_TTF_INCLUDE) $(OOPS_FT_INCLUDE) $(OOPS_PNG_INCLUDE) \
                   $(OOPS_VORBIS_INCLUDE) $(OOPS_GLM_INCLUDE) $(OOPS_OPENAL_INCLUDE)

# **Four standard headers upstream uses without including**, forced in for C++ only. 105 files
# name `std::function` through `editor/object_option.hpp`, which never includes `<functional>`;
# `util/string_util.cpp` calls `isdigit` with no `<cctype>`; nine files build an
# `std::ostringstream` with no `<sstream>`; two write to `std::cout`/`std::cerr` with no
# `<iostream>`. libstdc++ reaches all four transitively through `<string>` and `<algorithm>`;
# libc++ is stricter about what its headers drag in. `-include` answers it without editing
# upstream, as Extreme Tux Racer's `-include GL/glext.h` does.
OOPS_CXX_INCLUDE     += $(OOPS_LIBCXX_INCLUDE) -include functional -include cctype \
                        -include sstream -include iostream
EXTRA_TARGET_CFLAGS  += $(STX_INCLUDE) $(STX_DEFINES) $(STX_DEP_INCLUDE)

# ---------------------------------------------------------------------------
# The C sources: PhysFS and three vendored helpers, under upstream's flags
# ---------------------------------------------------------------------------
#
# **Objects on the link line, not `PAYLOAD_SRCS`**, for the reason Neverball's Makefile gives:
# `app.mk` holds payload C to this repository's `-Werror -Wconversion`, and somebody else's C
# does not survive that - nor should our shim's C be held to less to make room for it.
include $(OOPS_APPS_ROOT)/common/deps.mk

STX_C_SRCS   := $(STX_PHYSFS_C) $(STX_VENDORED_C)
STX_C_BUILD  := build/c
# The header order is `app.mk`'s - oops-sdk's libc, then the POSIX shim - and `shim/include`
# ahead of both, for its `<pthread.h>`. `stx_physfs_fcntl.h` is forced in for the two `fcntl`
# names PhysFS's flush uses, which oops-sdk's `<fcntl.h>` (found first) does not declare; the
# header says why that is not solved by reordering the path.
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
# A GL function oops-gl does not have is a **link error**, not a slow path, and this finds it
# before a link does. It reads `upstream/src/video/gl/*.cpp` and oops-sdk's own `GL/gl.h` at run
# time, so it measures the two trees as they actually are. It measures surface, not behaviour -
# the tool says so in its own output.
HOST_TEST_SRCS = tools/glcheck.c

include $(OOPS_APPS_ROOT)/common/app.mk

# **After the include, and it has to be.** `common/app.mk` sets `CFLAGS` with `?=`, so assigning
# to it any earlier would count as "already set" and silently drop the whole warning set this
# repository builds host code with. Recipes expand their variables when they run, so appending
# here still reaches the compile above.
CFLAGS += -DSTX_GL_HEADER='"$(OOPS_SDK_DIR)/include/GL/gl.h"'

# ---------------------------------------------------------------------------
# `make shadercheck`: the other half of "can this render"
# ---------------------------------------------------------------------------
#
# **The shader the ES 2.0 path loads, through oops-gl's own compiler and generator** - by way of
# oops-sdk's `tools/shader-survey.sh`, which is the SDK's to maintain. Two properties asserted:
# the fragment shader *generates* (it can become gfx1030 instructions, which is what decides
# whether it runs on the console rather than only on the software reference), and the vertex
# shader compiles.
#
# `shader330.*` are surveyed too and are expected to fail by version number. They are not this
# port's path, and asserting nothing about them keeps an oops-gl that one day accepts 3.30 from
# failing this.
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
# **`make package` is the one to run**, as it is for Extreme Tux Racer and Neverball: a title
# package from `app.mk` holds the payload and `sce_sys/`, and SuperTux reaches none of its menus
# without its 244 MB of content. It lands at `/app0/data`, which `shim/stx_start.cpp` passes as
# `--datadir` and `config.h` names as `BUILD_DATA_DIR`.
#
# `data/locale/*.po` is kept: tinygettext reads the `.po` files directly, with no compile step.
#
# This is the **third** title to copy its content into the package, which is the count
# Extreme Tux Racer's Makefile set for making it shared - a `TITLE_DATA_DIR` in `app.mk`. That is
# its own change, to all three at once.
.PHONY: package
package: title
	@test -d upstream/data || { echo "supertux: no upstream/data - run make first" >&2; exit 1; }
	@echo "supertux: copying $$(du -sh upstream/data | cut -f1) of data into the package"
	@rm -rf build/title/$(TITLE_ID)/data
	@cp -r upstream/data build/title/$(TITLE_ID)/data
	@echo "supertux: package is build/title/$(TITLE_ID) ($$(du -sh build/title/$(TITLE_ID) | cut -f1))"
