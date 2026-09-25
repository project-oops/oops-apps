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

# **`OOPS_SDK_DIR` early, because this file names files under it before `app.mk` is included.**
# `app.mk` defines it at its line 11, which is too late for an immediately-expanded (`:=`) list
# down the file - the symptom is a rule wanting `/src/thread/thread.c`, rooted at nothing.
# `HOST_TEST_SRCS` below gets away with naming it because it is recursive (`=`) and resolves later.
# The `?=` matches app.mk's own default, so including app.mk afterwards changes nothing.
OOPS_SDK ?= $(abspath $(OOPS_APPS_ROOT)/../oops-sdk)
OOPS_SDK_DIR := $(abspath $(OOPS_SDK))

# **`gl2`, because Craft draws everything through shaders.** There is no fixed-function path in
# it: the four shader pairs `make check` compiles are the whole renderer, so `gl1` would link a
# back end that cannot run any of them.
#
# This was absent while the title was `check-only`, and absent did not mean `gl1` - it meant the
# *payload* got no GL sources at all, while `HOST_TEST_SRCS` below still named `$(OOPS_GL_SRCS)`
# for the shader checker. `common/app.mk`'s host/payload guard is what caught it, reporting 22
# shared sources the self-test would link and the payload would not.
OOPS_RENDERER = gl2

# **First, because this file names files under `upstream/` while it is being read.** The fetch
# is normally done by `common/app.mk` at the bottom, which is too late for the wildcard below.
# `common/upstream.mk` says the rest; it is guarded, so `app.mk`'s own include costs nothing.
include $(OOPS_APPS_ROOT)/common/upstream.mk

# **An ELF, as of 2026-09-25.** This was `check-only` while two subsystems were missing: sockets
# for `client.c` and a platform layer for SQLite. Both landed - `common/posix` grew BSD sockets
# over `oops/net.h`, and `common/sqlite` is a VFS and a mutex implementation over the SDK - so
# `make census` now reports 18 of 19 sources compiling, the one exception being `auth.c`, which
# this build excludes on purpose.
FORMATS = elf

# --- the POSIX shim --------------------------------------------------------
#
# `fopen` and friends come from oops-sdk's libc; this is the layer above - `stat`, `opendir`,
# `getcwd` - which Craft reaches through SQLite rather than directly. Shared with Neverball and
# Extreme Tux Racer, which is why it lives in `common/`.
include $(OOPS_APPS_ROOT)/common/posix.mk

# SQLite's platform layer. Craft vendors `sqlite3.c` itself in `upstream/deps/sqlite`; this
# supplies the part that is about *this machine* - a VFS over `oops/fs.h` and a mutex
# implementation over `oops/thread.h` - and the defines that remove SQLite's unix backend.
#
# **`db.c` drives one connection from two threads**, a writer on `db_worker_run` and reads on the
# main thread, and its own locking covers 8 of the 21 functions that call SQLite. So the mutexes
# have to be real; `SQLITE_THREADSAFE=0`, which this Makefile used to set, was a data race.
OOPS_SQLITE_TEMP_DIR := /data/craft
include $(OOPS_APPS_ROOT)/common/sqlite.mk

# --- what the port replaces ------------------------------------------------
#
# **GLFW and GLEW are not compiled.** `upstream/deps/glfw` and `upstream/deps/glew` are in the
# fetched tree and stay there; `shim/` answers the twenty-three GLFW entry points and the two
# GLEW ones Craft actually calls, over oops-sdk's display, input and clock. `shim/include` goes
# on the path *before* anything else so that `#include <GLFW/glfw3.h>` finds ours.
#
# The surface is measured, not chosen: it is every `glfw*` token in Craft's own sources.
CRAFT_SHIM_SRCS := shim/glfw_shim.c shim/craft_offline.c shim/craft_start.c $(OOPS_SQLITE_SRCS)

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
#
# **`-nostdlibinc` is no longer set here.** It moved into `common/app.mk`'s `TARGET_CFLAGS` on
# 2026-09-24, on the freestanding side of the fork that already decides whether a title gets
# oops-sdk's libc headers - so every freestanding title has it now and this one does not need its
# own copy. The measurement that moved it started here: without the flag, `client.c`, `sqlite3.c`
# and `tinycthread.c` all died on `bits/wordsize.h`, a glibc internal three levels below the
# header each one was actually missing. docs/PORTING.md, *The host headers*, keeps the table.
#
# # `CRAFT_DATA_DIR` is the real path, not `/app0`, and that is measured
#
# `/app0` is the conventional answer and it does not work for a homebrew title launched this way.
# The payload can open neither `/app0` nor anything under it - `oops_fs_opendir("/app0")` fails and
# so does `oops_fs_open("/app0/textures/texture.png")` - while its own eboot was loaded from
# `/data/homebrew/CRFT00001` and the assets are demonstrably there. `/user/app/CRFT00001` is a
# payload-less stub holding a `mount.lnk` that correctly names that directory, so the link is right
# and the mount simply is not live for the process.
#
# So the prefix is the directory itself. The title id is written out rather than `$(TITLE_ID)`:
# that variable is defined by `common/app.mk`, which is included at the bottom of this file, and an
# immediately-expanded (`:=`) list up here would silently get `/data/homebrew/` with nothing after
# the slash - which is what happened on the first attempt.
CRAFT_CFLAGS := -Ishim/include \
                -Iupstream/src \
                -Iupstream/deps/lodepng \
                -Iupstream/deps/noise \
                -Iupstream/deps/sqlite \
                -Iupstream/deps/tinycthread \
                -DOOPS_POSIX_HOME=\"/data/craft\" \
                -DCRAFT_DATA_DIR=\"/data/homebrew/CRFT00001\" \
                -DDB_PATH=\"/data/craft/craft.db\" \
                $(OOPS_POSIX_INCLUDE) $(OOPS_SQLITE_INCLUDE) $(OOPS_SQLITE_DEFS)

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
                           -Wno-string-concatenation -Wno-cast-function-type-mismatch \
                           -Wno-null-pointer-subtraction -Wno-unused-const-variable \
                           -Wno-unused-function

EXTRA_TARGET_CFLAGS += $(CRAFT_CFLAGS) $(CRAFT_UPSTREAM_WARNINGS)

# **The payload's sources, named and not yet armed.**
#
# `common/app.mk` builds an ELF whenever `PAYLOAD_SRCS` is non-empty, so assigning it now would
# make `make` fail on a link that two missing subsystems already rule out - and a title that
# fails its default target is one CI has to special-case and a reader has to learn to ignore.
# The lists above are still worth having: they are what says what this title is made of, and
# they are what the measurement in README.md was taken against.
#
# **Armed.** `common/app.mk` builds an ELF whenever `PAYLOAD_SRCS` is non-empty, and the two gaps
# that kept this list unassigned - `docs/PORTING.md`'s sockets and SQLite platform layer - are
# closed. `CRAFT_SHIM_SRCS` carries `common/sqlite`'s VFS along with the GLFW shim.
CRAFT_PAYLOAD_SRCS := $(CRAFT_SHIM_SRCS) $(CRAFT_UPSTREAM_SRCS) $(CRAFT_DEP_SRCS) \
                      $(OOPS_POSIX_SRCS)
PAYLOAD_SRCS := $(CRAFT_PAYLOAD_SRCS)

# The SDK subsystems this title uses, declared as capabilities via OOPS_FEATURES.
OOPS_FEATURES := display input keyboard mouse thread net

# **And named as files as well, because `OOPS_FEATURES` links nothing today.** `common/app.mk:120`
# prints "the SDK feature table (OOPS_FEATURES_AVAILABLE) is not available yet - ignoring it for
# now" and moves on, so a title relying on the declaration above gets no display, no input, no
# threads and no sockets - the link then fails on 20-odd `oops_*` symbols, or worse, would have
# faulted on the console if the link guard were not there.
#
# The two lists coexist on purpose: the feature declaration is the intended spelling and is already
# correct, so when `oops-sdk.mk` commits its table this block is what gets deleted, not rewritten.
# Nothing is compiled twice in the meantime, precisely because the declaration is inert.
CRAFT_SDK_SRCS := $(OOPS_SDK_DIR)/src/display.c \
                  $(OOPS_SDK_DIR)/src/agc/agc_display.c \
                  $(OOPS_SDK_DIR)/src/agc/agc_tiler.c \
                  $(OOPS_SDK_DIR)/src/agc/agc_compute.c \
                  $(OOPS_SDK_DIR)/src/input/input.c \
                  $(OOPS_SDK_DIR)/src/input/keyboard.c \
                  $(OOPS_SDK_DIR)/src/input/mouse.c \
                  $(OOPS_SDK_DIR)/src/thread/thread.c \
                  $(OOPS_SDK_DIR)/src/net/net.c \
                  $(OOPS_SDK_DIR)/src/net/dns.c \
                  $(OOPS_SDK_DIR)/src/net/netctl.c
PAYLOAD_SRCS += $(CRAFT_SDK_SRCS)

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

# ---------------------------------------------------------------------------
# `make stage-data`: the assets, into the packaged title
#
#   make title stage-data
#
# **Craft does not start without these.** `main` loads four textures and four shader pairs before
# it draws anything, and `load_png_texture` calls `exit(1)` when one is missing - so a title
# packaged without them is not a degraded Craft, it is a title that closes immediately.
#
# It is a separate target because `title` belongs to `common/app.mk` and has no post-staging hook;
# `mesa-demos` has the same split for the same reason. That makes it forgettable, so `restore`
# below depends on it and refuses to send a title that has not been staged. A hook in `app.mk`
# would be the better answer and is worth adding when that file is not being edited elsewhere.
# ---------------------------------------------------------------------------
CRAFT_TITLE_DIR = $(BUILD)/title/$(TITLE_ID)

.PHONY: stage-data
stage-data:
	@test -d $(CRAFT_TITLE_DIR) || { \
	    echo "stage-data: no packaged title at $(CRAFT_TITLE_DIR) - run 'make title' first"; \
	    exit 1; \
	}
	@mkdir -p $(CRAFT_TITLE_DIR)/textures $(CRAFT_TITLE_DIR)/shaders
	@cp upstream/textures/*.png $(CRAFT_TITLE_DIR)/textures/
	@cp upstream/shaders/*.glsl $(CRAFT_TITLE_DIR)/shaders/
	@t=$$(ls $(CRAFT_TITLE_DIR)/textures | wc -l); s=$$(ls $(CRAFT_TITLE_DIR)/shaders | wc -l); \
	 printf 'stage-data: %s textures, %s shaders -> %s\n' "$$t" "$$s" "$(CRAFT_TITLE_DIR)"; \
	 test "$$t" -eq 4 || { echo "stage-data: expected 4 textures"; exit 1; }; \
	 test "$$s" -eq 8 || { echo "stage-data: expected 8 shaders"; exit 1; }

# ---------------------------------------------------------------------------
# `make census`: which of this title's sources compile for the target, today
# ---------------------------------------------------------------------------
#
# **Because a count in a document is a claim and this is a measurement.** README.md and
# docs/PORTING.md both carried per-file tallies, and both had drifted: `auth.c` was counted as
# compiling when it does not - it is excluded on purpose - and `tinycthread.c` was listed among
# the vendored libraries that build while the same document named the `signal.h` it needs. Two
# numbers, both written in good faith, both wrong by the time anybody read them.
#
# So the numbers now come from the compiler. Every source this title would link is compiled to
# `/dev/null` under the real target flags, and each one either passes or prints the first error
# it hit - which for a missing subsystem is the header, and the header is the whole diagnosis.
#
# `check` depends on this, so `make check` answers both of this title's open questions: whether
# the shaders still compile, and what still does not. Adding a prerequisite to a target whose
# recipe lives in `common/app.mk` is legal make - only one rule may carry the recipe, and that
# one does.
#
# **It does not fail the build.** The failures are this title's status, not a regression; the
# gate that would catch a *change* is the census output being read, and docs/PORTING.md carries
# the expected set. `make` stopping here would be a default target that always fails, which is
# what `FORMATS = check-only` above exists to avoid.
.PHONY: census
check: census
census:
	@echo "craft census: $(words $(CRAFT_PAYLOAD_SRCS)) sources, under the target's own flags"
	@ok=0; bad=0; \
	for f in $(CRAFT_UPSTREAM_SRCS) upstream/src/auth.c $(CRAFT_DEP_SRCS) $(CRAFT_SHIM_SRCS); do \
	    err=$$($(TARGET_CC) $(TARGET_CFLAGS) -c -o /dev/null "$$f" 2>&1 | \
	           grep -m1 -E 'error:' | sed 's/.*error: //'); \
	    if [ -z "$$err" ]; then \
	        printf '  %-40s ok\n' "$$f"; ok=$$((ok+1)); \
	    else \
	        printf '  %-40s FAIL  %s\n' "$$f" "$$err"; bad=$$((bad+1)); \
	    fi; \
	done; \
	echo "craft census: $$ok compile, $$bad do not"
