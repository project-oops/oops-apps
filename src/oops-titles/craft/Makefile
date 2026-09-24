# Craft, through oops-sdk.
#
#   make check    compile every shader Craft ships, for the console, and report
#   make elf      not yet - see below
#
# **This does not build a payload yet, and the reasons are counted rather than guessed.** What
# works, what does not, and what each gap costs is in README.md and docs/PORTING.md; the short
# version is that the graphics are done and the storage is not.
#
# The Makefile is written now rather than when that lands, because it is what says what the
# title is made of, and because most of the pieces it names are real and tested. That is the
# shape `extreme-tux-racer/Makefile` already uses for the same reason.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# **First, because this file names files under `upstream/` while it is being read.** The fetch
# is normally done by `common/app.mk` at the bottom, which is too late for the wildcard below.
# `common/upstream.mk` says the rest; it is guarded, so `app.mk`'s own include costs nothing.
include $(OOPS_APPS_ROOT)/common/upstream.mk

# **`check-only` until the two subsystems below land.** The payload's source list is real and is
# named here because that is what says what the title is made of - but `make` builds the shader
# check and stops, rather than failing on a link everybody already knows will not happen.
# docs/PORTING.md has the gap list, per file, with the header each one wants.
FORMATS = check-only

# --- the POSIX shim --------------------------------------------------------
#
# `fopen` and friends come from oops-sdk's libc; this is the layer above - `stat`, `opendir`,
# `getcwd` - which Craft reaches through SQLite rather than directly. Shared with Neverball and
# Extreme Tux Racer, which is why it lives in `common/`.
include $(OOPS_APPS_ROOT)/common/posix.mk

# --- what the port replaces ------------------------------------------------
#
# **GLFW and GLEW are not compiled.** `upstream/deps/glfw` and `upstream/deps/glew` are in the
# fetched tree and stay there; `shim/` answers the twenty-three GLFW entry points and the two
# GLEW ones Craft actually calls, over oops-sdk's display, input and clock. `shim/include` goes
# on the path *before* anything else so that `#include <GLFW/glfw3.h>` finds ours.
#
# The surface is measured, not chosen: it is every `glfw*` token in Craft's own sources.
CRAFT_SHIM_SRCS := shim/glfw_shim.c shim/craft_offline.c

# --- what upstream brings --------------------------------------------------
#
# Craft's own twelve sources, minus the one that is not wanted:
#
#   - **`auth.c` is excluded**, and it is the only file that includes <curl/curl.h>. It logs in
#     to a multiplayer server over HTTPS; this port is single-player, so the dependency goes
#     rather than being stubbed - a stub would be a login that silently always fails, which is
#     worse than a build without the feature. `shim/craft_offline.c` supplies the two symbols
#     `main.c` calls, and says so.
#   - `client.c` **is** compiled: it is the network client, it is inert until `client_enable()`
#     is called, and it costs nothing to keep the code path honest.
# The wildcard needs `upstream/` to exist, and the fetch that creates it lives in
# `common/upstream.mk`, which this file includes at the top for that reason - `common/app.mk` at
# the bottom would be eighty lines too late, and on a fresh checkout this would match nothing.
CRAFT_UPSTREAM_SRCS := $(filter-out upstream/src/auth.c,$(wildcard upstream/src/*.c))

# The vendored libraries Craft carries. lodepng and noise are pure C and need nothing; sqlite
# and tinycthread are the two with a platform underneath them, and are what docs/PORTING.md is
# mostly about.
CRAFT_DEP_SRCS := upstream/deps/lodepng/lodepng.c \
                  upstream/deps/noise/noise.c \
                  upstream/deps/sqlite/sqlite3.c \
                  upstream/deps/tinycthread/tinycthread.c

# --- flags -----------------------------------------------------------------
#
# `-Ishim/include` first, then upstream's own include directories in the order its CMakeLists
# lists them.
#
# **`SQLITE_OMIT_LOAD_EXTENSION` and friends are not a tuning choice.** They remove the parts of
# SQLite that need `dlopen`, a thread library and a VFS this target does not have; what is left
# still needs a VFS, which is the gap docs/PORTING.md names.
CRAFT_CFLAGS := -Ishim/include \
                -Iupstream/src \
                -Iupstream/deps/lodepng \
                -Iupstream/deps/noise \
                -Iupstream/deps/sqlite \
                -Iupstream/deps/tinycthread \
                -DOOPS_POSIX_HOME=\"/data/craft\" \
                -DSQLITE_OMIT_LOAD_EXTENSION=1 \
                -DSQLITE_THREADSAFE=0 \
                -DSQLITE_OMIT_WAL=1 \
                $(OOPS_POSIX_INCLUDE)

# **Upstream's code, under upstream's warnings.** `common/app.mk` compiles at `-Werror` with
# `-Wconversion`, `-Wsign-conversion`, `-Wstrict-prototypes` and the rest, which is right for
# code written here and is stricter than Craft was written for: the first build produced 33
# missing prototypes, 44 implicit float conversions and a dozen `/*` inside block comments,
# none of which is a portability problem and all of which are `-Werror` here.
#
# These are switched off rather than patched away, because a patch to 6,000 lines of somebody
# else's C is a rebase burden for no behaviour change - and because a warning upstream has lived
# with for a decade on four compilers is not news. **`-Wno-conversion` is the wide one** and it
# is deliberate: the alternative is a list of six narrower flags that has to grow every time
# upstream touches a float.
#
# They apply to `shim/` too, which is the cost. The shim is four files and is read on the way in.
CRAFT_UPSTREAM_WARNINGS := -Wno-strict-prototypes -Wno-missing-prototypes -Wno-conversion \
                           -Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-comment \
                           -Wno-unused-parameter -Wno-unused-but-set-variable \
                           -Wno-shadow -Wno-sign-compare -Wno-char-subscripts \
                           -Wno-string-concatenation -Wno-cast-function-type-mismatch

EXTRA_TARGET_CFLAGS += $(CRAFT_CFLAGS) $(CRAFT_UPSTREAM_WARNINGS)

# **The payload's sources, named and not yet armed.**
#
# `common/app.mk` builds an ELF whenever `PAYLOAD_SRCS` is non-empty, so assigning it now would
# make `make` fail on a link that two missing subsystems already rule out - and a title that
# fails its default target is one CI has to special-case and a reader has to learn to ignore.
# The lists above are still worth having: they are what says what this title is made of, and
# they are what the measurement in README.md was taken against.
#
# **Turning it on is this one line**, once `docs/PORTING.md`'s two gaps are closed:
#
#   PAYLOAD_SRCS := $(CRAFT_SHIM_SRCS) $(CRAFT_UPSTREAM_SRCS) $(CRAFT_DEP_SRCS) $(OOPS_POSIX_SRCS)
CRAFT_PAYLOAD_SRCS := $(CRAFT_SHIM_SRCS) $(CRAFT_UPSTREAM_SRCS) $(CRAFT_DEP_SRCS) \
                      $(OOPS_POSIX_SRCS)

# ---------------------------------------------------------------------------
# `make check`: every shader Craft ships, compiled for the console.
#
# **This is the check that matters most for this title and it is not a formality.** Craft draws
# everything through four shader pairs, and oops-gl's console back end compiles a fragment
# shader to real gfx1030 instructions or **refuses it by name**. A refusal is not a slow path -
# the draw fails with `GL_INVALID_OPERATION` - so a shader the back end will not take is a title
# that does not render, and that is a thing to find out on a build machine.
#
# It reads `upstream/shaders/*.glsl` directly, so it checks the files the game will load rather
# than a copy of them, and it re-runs whenever either side moves. It also prints each shader's
# cost against the four limits that can refuse one, so a shader creeping towards a limit is
# visible before it crosses it.
# ---------------------------------------------------------------------------
# system.c, fs.c and heap.c travel together: the disk sink reaches fs.c, fs.c reaches the
# allocator.
HOST_TEST_SRCS = tools/shadercheck.c \
                 $(OOPS_GL_SRCS) \
                 $(OOPS_SDK_DIR)/src/system/system.c \
                 $(OOPS_SDK_DIR)/src/system/fs.c \
                 $(OOPS_SDK_DIR)/src/memory/heap.c \
                 $(OOPS_SDK_DIR)/src/system/freestd.c
HOST_TEST_LIBS = -lm

include $(OOPS_APPS_ROOT)/common/app.mk
