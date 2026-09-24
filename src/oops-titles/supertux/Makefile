# SuperTux, through oops-sdk.
#
#   make check    every GL entry point SuperTux's renderer calls, against oops-gl, and report
#   make elf      not yet - see below
#
# **This does not build a payload, and the reasons are counted rather than guessed.** README.md
# and docs/PORTING.md have them; the short version is that the *graphics* are far smaller than
# they looked and the C++ stack is the whole job.
#
# The Makefile is written now rather than when that lands, because it is what says what the title
# is made of. `craft/Makefile` and `extreme-tux-racer/Makefile` already use this shape for the
# same reason.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# **First, because this file names files under `upstream/` while it is being read.** The fetch is
# normally done by `common/app.mk` at the bottom, which is too late for the wildcards below.
# `common/upstream.mk` says the rest; it is guarded, so `app.mk`'s own include costs nothing.
include $(OOPS_APPS_ROOT)/common/upstream.mk

# **`check-only` until the C++ stack lands.** The source list below is real and is named because
# that is what says what this title is made of - but `make` builds the entry-point check and
# stops, rather than failing on a link everybody already knows will not happen. A title whose
# default target fails is one CI has to special-case and a reader learns to ignore.
FORMATS = check-only

# ---------------------------------------------------------------------------
# The renderer this port targets, which is **gl1 and not gl2**
# ---------------------------------------------------------------------------
#
# SuperTux ships two OpenGL backends and picks between them at run time, in
# `src/video/gl/gl_video_system.cpp`, by reading the major number out of
# `glGetString(GL_VERSION)`: 3 or more takes `GL33CoreContext`, exactly 2 takes `GL20Context`,
# and anything else throws so the SDL software renderer is used instead.
#
# **`GL20Context` uses no shaders at all.** It is fixed-function GL 1.x - `glMatrixMode`,
# `glOrtho`, `glTranslatef`, `glEnableClientState`, `glVertexPointer`, `glTexCoordPointer`,
# `glColorPointer`, `glColor4f`, `glDrawArrays` - and it binds **one** texture, ignoring the
# `displacement_texture` argument its own interface hands it. `set_blur` is an empty function.
#
# So the four files in `data/shader/` belong to the GLES2 and GL33Core paths and this port never
# loads them. That is worth stating plainly because it inverts the obvious guess: SuperTux looks
# like the collection's second GL 2.0 title and on this path it is a GL 1.x one, closer to
# Neverball than to Craft. docs/PORTING.md, *The renderer*, has the whole reading.
#
# `OOPS_RENDERER = gl1` is therefore what this will want - commented out with the payload it
# belongs to, because `common/app.mk` adds oops-gl's sources to `PAYLOAD_SRCS` when it is set,
# and there is no payload yet. The check below lists oops-gl in `HOST_TEST_SRCS` itself.
#
#   OOPS_RENDERER = gl1
#
# One thing the shim must do for any of this to happen: call `glContextSetVersion(2, 0)`.
# oops-gl's default badge is `"1.1 oops-gl fixed-function subset"`, and SuperTux's `sscanf` of
# that reads 1, which is the arm that **throws** and drops to the software renderer. At 2.0 the
# badge reads `"2.0 oops-gl programmable subset"`, the parse yields 2, and GL20Context is chosen -
# a context whose version is a claim about what the program expects, which is exactly what
# `gl_internal.h` says the setting is for.

# --- the POSIX shim --------------------------------------------------------
#
# `stat`, `opendir`, `getcwd` - the layer above oops-sdk's libc. Shared with Neverball, Craft and
# Extreme Tux Racer, which is why it lives in `common/`. PhysFS is what reaches it here.
include $(OOPS_APPS_ROOT)/common/posix.mk

# --- what upstream brings --------------------------------------------------
#
# **Named, not globbed, and not yet complete.** `src/` at this revision is 476 files and the
# selection is a real piece of work that has not been done: SuperTux's CMake chooses between
# renderers, between scripting backends and between optional features, and a glob sweeps in the
# arms this port does not take. This wildcard is here to be *measured* by `make check`, which
# reads the renderer directory directly, and is deliberately not fed to a link.
#
# The wildcard needs `upstream/` to exist, and the fetch that creates it is `common/upstream.mk`
# at the top of this file - `common/app.mk` at the bottom would be too late, and on a fresh
# checkout this would match nothing.
STX_GL_SRCS := $(wildcard upstream/src/video/gl/*.cpp)

# ---------------------------------------------------------------------------
# `make check`: every GL entry point SuperTux's renderer calls, against oops-gl
# ---------------------------------------------------------------------------
#
# **This is the analogue of Craft's `shadercheck` and it answers the same question**: can this
# title render at all. Craft's answer was in its shaders, because oops-gl compiles a fragment
# shader or refuses it by name. SuperTux's GL 2.0 path has no shaders, so the question moves to
# the entry points - and a GL function oops-gl does not have is a **link error**, not a slow
# path, which is the whole reason GLEW is shimmed away rather than used.
#
# It reads `upstream/src/video/gl/*.cpp` and oops-sdk's own `GL/gl.h` at run time, so it measures
# the two trees as they actually are and re-runs whenever either moves.
#
# **It measures surface, not behaviour.** A declared entry point can still be wrong, and that is
# gl1-probe's job rather than this one's; what this catches is the class of gap that stops a link
# dead. The tool says so in its own output.
HOST_TEST_SRCS = tools/glcheck.c

include $(OOPS_APPS_ROOT)/common/app.mk

# **After the include, and it has to be.** `common/app.mk` sets `CFLAGS` with `?=`, so assigning
# to it any earlier would count as "already set" and silently drop the whole warning set this
# repository builds host code with - `-Werror -Wconversion` and the rest. Recipes expand their
# variables when they run, so appending here still reaches the compile above.
#
# The SDK's header is passed in because its path is the SDK's to decide, not a relative guess
# from this directory. The renderer directory is not passed: `tools/glcheck.c` defaults to
# `upstream/src/video/gl` and `make check` runs from here, which is what `craft/tools/shadercheck.c`
# does with `upstream/shaders`.
CFLAGS += -DSTX_GL_HEADER='"$(OOPS_SDK_DIR)/include/GL/gl.h"'
