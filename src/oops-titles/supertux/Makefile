# SuperTux, through oops-sdk - on oops-gl's GL 2.0 / GLSL path.
#
#   make check        every GL entry point the ES 2.0 renderer calls, against oops-gl
#   make shadercheck  upstream's `shader100.*`, through oops-gl's compiler and code generator
#   make elf          not yet - the stack below is not all pinned
#
# **This does not build a payload yet, and what is missing is named below rather than guessed.**
# Both rendering questions are answered: `make check` finds every entry point present and
# `make shadercheck` finds the fragment shader generating for the console. What stands between
# this and a link is the dependency stack - see the table under *The stack*, and
# docs/PORTING.md for the order.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# **First, because this file names files under `upstream/` while it is being read.** The fetch is
# normally done by `common/app.mk` at the bottom, which is too late for the wildcards below.
# `common/upstream.mk` says the rest; it is guarded, so `app.mk`'s own include costs nothing.
include $(OOPS_APPS_ROOT)/common/upstream.mk

# **`check-only` until the stack lands.** The source list below is real and complete - it is
# upstream's own CMake glob - but `make` builds the checks and stops, rather than failing on a
# link everybody already knows will not happen. A title whose default target fails is one CI
# has to special-case and a reader learns to ignore.
FORMATS = check-only

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
OOPS_RENDERER_DEFINE := -DUSE_OPENGLES2

# ---------------------------------------------------------------------------
# What upstream brings
# ---------------------------------------------------------------------------
#
# **Upstream's own CMake selection, transcribed.** `CMakeLists.txt` globs `src/*/*.cpp`,
# `src/supertux/menu/*.cpp`, `src/video/{sdl,null,gl}/*.cpp`, and builds `src/main.cpp` into the
# executable separately. Every optional feature in that tree is behind a `config.h` switch rather
# than a file list - Discord, touchscreen, binreloc - so the glob is the selection and
# `shim/include/config.h` is where each feature is turned off. 467 files.
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
# other ten archivers read game formats from the 1990s (`grp`, `hog`, `wad`...) and are parsers of
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
# `version.h` and `SDL_opengles2.h` are ours.
STX_CFLAGS := $(OOPS_RENDERER_DEFINE) -Ishim/include -Iupstream/src \
              -Iupstream/external/obstack -Iupstream/external/findlocale \
              -Iupstream/external/SDL_SavePNG -Iupstream/external/partio_zip \
              -Iupstream/external/physfs/src -Iupstream/external/squirrel/include \
              -Iupstream/external/sexp-cpp/include -Iupstream/external/tinygettext/include

# ---------------------------------------------------------------------------
# The stack
# ---------------------------------------------------------------------------
#
# **C++ with exceptions and RTTI, and there is no avoiding it.** 779 `throw`, 832 `catch`,
# 226 `dynamic_cast` - measured. `OOPS_CXX_EXCEPTIONS = 1` with libc++abi and libunwind is the
# route `src/oops-utilities/cxx-throw` proved on hardware (5/5), and this is the first title to
# take it. `-fno-exceptions` would refuse the first `throw` at compile time, which is the point
# of that flag, and here it would refuse 779 of them.
#
#   pinned in oops-deps                        in the fetched tree (above)
#   ----------------------------------------   ------------------------------------------------
#   libc++, libc++abi, libunwind               physfs, squirrel, sexp-cpp, tinygettext
#   sdl2, sdl2-image, libpng, zlib
#   sdl2-ttf, freetype                         to add
#   libogg, libvorbis (vorbisfile)             ------------------------------------------------
#   common/posix.mk                            oops-deps/glm          header-only
#                                              oops-deps/openal-soft  shared with SuperTuxKart
#                                              shim: boost subset, curl stub
#
# docs/PORTING.md has why each is where it is. The oops-deps `.mk` files are included with the
# link they feed, which is the step that lands with glm and OpenAL Soft - until then there is
# nothing for their archives to be linked into.
OOPS_CXX_EXCEPTIONS = 1

# The POSIX shim - `stat`, `opendir`, `getcwd` and the rest. PhysFS's POSIX platform layer is
# what reaches it here. Shared with Neverball, Craft and Extreme Tux Racer.
include $(OOPS_APPS_ROOT)/common/posix.mk

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
