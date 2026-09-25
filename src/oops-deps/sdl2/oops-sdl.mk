# SDL2 build integration. Include this from a title's Makefile, before `common/app.mk`:
#
#   OOPS_SDL ?= $(abspath ../../oops-deps/sdl2)
#   include $(OOPS_SDL)/oops-sdl.mk
#
#   EXTRA_TARGET_CFLAGS  += $(OOPS_SDL_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_SDL_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_SDL_LIB)
#
# # An archive, and not source inclusion
#
# oops-sdk#D003 settled on source inclusion *for the SDK*, so every source compiles under the
# consumer's own target flags. This is not the SDK: it is somebody else's tree, and the
# consumer's flags here are `-Werror -Wconversion -Wsign-conversion -Wshadow`. Upstream SDL does
# not compile clean under those and has no reason to - `src/video/yuv2rgb/` alone produces
# hundreds of sign-conversion errors from SSE intrinsics that are correct as written.
#
# The choice is between relaxing our warnings for the whole payload, including the title's own
# code, or giving upstream its own. An archive gives upstream its own, and `common/app.mk`
# already has the shape for it: Mesa reaches a payload as archive paths in
# `EXTRA_TARGET_LDFLAGS` with the files named again in `PAYLOAD_EXTRA_DEPS`, and this follows it.
#
# **`--whole-archive`, deliberately.** `app.mk` puts LDFLAGS before the sources on the link line,
# and a static archive seen before the objects that need it contributes nothing. Mesa brackets
# its GL entry points the same way for the same reason. The cost is that unused parts of SDL
# stay in the payload; the alternative is a link that succeeds and leaves out the driver.
#
# The fetch is not here. `make sdl2-upstream` below puts the tree on disk, and a title that
# carries its own `upstream.lock` gets that one through `common/app.mk`.

# **Simply expanded, and `?=` will not do.** `?=` defines a *recursive* variable, so
# `$(lastword $(MAKEFILE_LIST))` would be re-evaluated every time the variable is used - and by
# the time a recipe runs, `MAKEFILE_LIST` ends with `common/app.mk` rather than this file. The
# paths then point at `common/`, which is where 130 object files went before a link failure said
# so. `ifndef` plus `:=` keeps it overridable from the outside and resolves it here, once.
ifndef OOPS_SDL_DIR
OOPS_SDL_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_SDL_UPSTREAM ?= $(OOPS_SDL_DIR)/upstream

# `__PROSPERO__` is what selects `include/SDL_config_prospero.h` through the arm that
# `patches/0001-*` adds to upstream's config chain. Defining it here rather than patching
# `SDL_platform.h` is what keeps the patch to one line per file.
#
# `backend/` is on the include path because one patch hunk needs it: `src/thread/SDL_thread_c.h`
# has to see `SYS_ThreadHandle` before it can lay out `struct SDL_Thread`, and our definition of
# it lives outside upstream's tree like the rest of our code.
OOPS_SDL_INCLUDE := \
    -D__PROSPERO__=1 \
    -I$(OOPS_SDL_DIR)/include \
    -I$(OOPS_SDL_DIR)/backend \
    -I$(OOPS_SDL_UPSTREAM)/include \
    -I$(OOPS_SDL_UPSTREAM)/src

# # `#include <SDL2/SDL.h>`, for the ports that write it that way
#
#   EXTRA_TARGET_CFLAGS += $(OOPS_SDL_INCLUDE) $(OOPS_SDL_PREFIX_INCLUDE)
#   <your archive>: | $(OOPS_SDL_PREFIX_STAMP)
#
# Every port here so far writes `#include "SDL.h"`, which is what upstream's own sources do and
# what the include path above serves. A distribution installs the headers to
# `/usr/include/SDL2/` instead, so a port that was developed against a package manager writes
# `<SDL2/SDL.h>` - libultraship does, in 67 of its 138 sources.
#
# **The directory is built rather than committed.** Ninety-four one-line forwarding headers would
# be ninety-four files in this repository that exist only to contain the word `SDL2`, and one of
# them would eventually be missing after a bump. A symlink would be one file, and would not
# survive a checkout on Windows. So this copies the header directory under the name the port
# expects, into `build/`, which is already ignored.
#
# It is a separate variable because it is a separate question: nothing that compiles today needs
# it, and a title asks for it by naming it. The stamp is order-only on purpose - the copy has to
# have happened, but a header that is newer than an object is not a reason to relink.
# The rule is further down, where `OOPS_SDL_BUILD` has been defined.

# **Wildcards, not a list of files.** SDL builds with CMake and we do not, so the file list is
# ours to keep correct across bumps - and a hand-written list of two hundred names is a list that
# silently loses a file the day upstream adds one. A directory is a smaller thing to be wrong
# about: the config decides which directories are in play, and everything in them compiles.
#
# What is deliberately absent: `src/main/` (the payload has its own entry point), `src/test/`,
# `src/hidapi/`, `src/loadso/` and `src/core/` (all disabled in our config), and every platform
# backend but the dummies we asked for.
OOPS_SDL_C_SRCS := \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/atomic/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/audio/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/audio/dummy/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/cpuinfo/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/dynapi/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/events/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/file/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/filesystem/dummy/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/haptic/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/haptic/dummy/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/joystick/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/joystick/dummy/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/libm/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/loadso/dummy/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/locale/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/locale/dummy/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/misc/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/misc/dummy/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/power/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/render/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/render/opengl/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/render/software/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/sensor/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/sensor/dummy/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/stdlib/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/thread/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/thread/generic/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/timer/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/video/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/video/dummy/*.c) \
    $(wildcard $(OOPS_SDL_UPSTREAM)/src/video/yuv2rgb/*.c) \
    $(wildcard $(OOPS_SDL_DIR)/backend/*.c)

# Ours last, deliberately: `backend/` is the only entry that is not a wildcard over somebody
# else's tree, and putting it at the end keeps that visible in the variable rather than buried.

# `SDL_steam_virtual_gamepad.c` was excluded here at first, and that was a latent link failure:
# `SDL_joystick.c` calls `SDL_InitSteamVirtualGamepadInfo` and three of its siblings
# unconditionally, so dropping the file compiles and then fails to link. It is back in the list
# and carries two hunks of `patches/0001-*` instead - it wants `<sys/stat.h>` for a `stat()` that
# watches a file only a desktop Steam client ever writes.
#
# The lesson is about the sweep, not the file: `-fsyntax-only` over every source says nothing
# about undefined symbols, which is what `common/app.mk`'s link check is for.

# **The three generic thread primitives our backend replaces.** These are the one place a
# directory really is the wrong unit: `src/thread/generic/` holds five files, we provide three of
# them, and the two that remain - the condition variable and the thread-local storage - are real
# implementations built on the three. Compiling all five would be duplicate symbols.
#
# `SDL_systhread.c` there is a "threads are not supported on this platform" stub; the other two
# are circular with each other (generic mutex is built on a semaphore, generic semaphore on a
# mutex and a condition variable), which is why a platform has to provide both.
OOPS_SDL_C_SRCS := $(filter-out \
    $(OOPS_SDL_UPSTREAM)/src/thread/generic/SDL_systhread.c \
    $(OOPS_SDL_UPSTREAM)/src/thread/generic/SDL_sysmutex.c \
    $(OOPS_SDL_UPSTREAM)/src/thread/generic/SDL_syssem.c, \
    $(OOPS_SDL_C_SRCS))

# ---------------------------------------------------------------------------
# The archive
#
# Built with the consumer's target flags for everything that decides code generation - the
# triple, `-ffreestanding`, the target macros - and **without** the warning set, which is ours
# and not upstream's. `TARGET_CFLAGS` is not known until `common/app.mk` has been read, so these
# are recursive (`=`) and resolve when the recipe runs rather than when this file is included.
OOPS_SDL_BUILD ?= $(OOPS_SDL_DIR)/build
OOPS_SDL_LIB := $(OOPS_SDL_BUILD)/libSDL2.a

# The `SDL2/`-prefixed header view described beside `OOPS_SDL_INCLUDE` above. Down here because it
# names `OOPS_SDL_BUILD`, and a rule's target is expanded when make reads the line.
OOPS_SDL_PREFIX_DIR := $(OOPS_SDL_BUILD)/prefix
OOPS_SDL_PREFIX_INCLUDE := -I$(OOPS_SDL_PREFIX_DIR)
OOPS_SDL_PREFIX_STAMP := $(OOPS_SDL_PREFIX_DIR)/.stamp

# `include/SDL_config_prospero.h` is copied in beside the rest, and has to be: `SDL.h` reaches its
# config through a *quoted* include, which a compiler resolves next to the including file first.
# Without the copy, `SDL2/SDL_platform.h` would find the config through `-I` anyway - but only
# because `OOPS_SDL_INCLUDE` happens to be on the same command line, and a caller using the prefix
# view alone would get a confusing failure deep inside SDL's header chain.
$(OOPS_SDL_PREFIX_STAMP): $(wildcard $(OOPS_SDL_UPSTREAM)/include/*.h) \
                          $(wildcard $(OOPS_SDL_DIR)/include/*.h)
	@rm -rf $(OOPS_SDL_PREFIX_DIR)/SDL2
	@mkdir -p $(OOPS_SDL_PREFIX_DIR)/SDL2
	@cp $(OOPS_SDL_UPSTREAM)/include/*.h $(OOPS_SDL_PREFIX_DIR)/SDL2/
	@cp $(OOPS_SDL_DIR)/include/*.h $(OOPS_SDL_PREFIX_DIR)/SDL2/
	@echo "SDL: $$(ls $(OOPS_SDL_PREFIX_DIR)/SDL2 | wc -l) headers under SDL2/"
	@touch $@

# The warnings dropped, each because upstream's code trips it and is right to.
OOPS_SDL_WARN_DROP := -Werror -Wconversion -Wsign-conversion -Wshadow -Wcast-qual \
                      -Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition \
                      -Wdouble-promotion -Wpedantic -pedantic
OOPS_SDL_CFLAGS = $(filter-out $(OOPS_SDL_WARN_DROP),$(TARGET_CFLAGS)) -w $(OOPS_SDL_INCLUDE)

OOPS_SDL_OBJS := $(patsubst %.c,$(OOPS_SDL_BUILD)/%.o,$(subst /,_,$(OOPS_SDL_C_SRCS)))

OOPS_SDL_LDFLAGS := -Wl,--whole-archive $(OOPS_SDL_LIB) -Wl,--no-whole-archive

# One rule per source would need one pattern rule per directory, so the archive is built by a
# single recipe that walks the list. It reruns when any source or this file changes, which for a
# dependency that moves only on a bump is the right granularity - and it keeps the object names
# flat, so two `SDL_sysjoystick.c` in different directories cannot collide. That is the reason
# the payload's per-source objects in `common/deps.mk` are *not* used here: `ar` stores a member
# under its basename alone, and SDL2 is the one tree in the collection where the same file name
# really does appear in several backends.
#
# **`ar` is handed the list rather than the directory**, which it was not until 2026-09-23. It
# collected `sdl*.o`, so shortening `OOPS_SDL_C_SRCS` left the highest-numbered objects behind
# for the glob to pick up and the removed file's code stayed in the archive, with the link
# staying clean. Numbering by position makes that worse than it sounds: dropping one source
# renumbers every object after it, so the leftovers are the *last* run's, not obviously stale
# ones. `common/deps.mk` has the whole reasoning.
$(OOPS_SDL_LIB): $(OOPS_SDL_C_SRCS) $(OOPS_SDL_DIR)/include/SDL_config_prospero.h \
                 $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_SDL_BUILD)
	@rm -f $@
	@n=0; objs=""; for src in $(OOPS_SDL_C_SRCS); do \
	    n=$$((n+1)); o=$(OOPS_SDL_BUILD)/sdl$$n.o; \
	    $(TARGET_CC) $(OOPS_SDL_CFLAGS) -c -o "$$o" "$$src" || exit 1; objs="$$objs $$o"; \
	done; \
	echo "oops-sdl: compiled $$n sources"; \
	ar_tool=$$(command -v $(AR) 2>/dev/null || command -v llvm-ar 2>/dev/null || command -v ar); \
	"$$ar_tool" rcs $@ $$objs
	@echo "oops-sdl: $@"

.PHONY: sdl2-clean
sdl2-clean:
	@rm -rf $(OOPS_SDL_BUILD)
	@echo "sdl2: removed build/"

# Fetching this on its own, for a first build or after changing the lock. A title that includes
# this file gets the fetch through `common/app.mk` instead, because make resolves prerequisites
# before it runs recipes and a wildcard over an absent tree expands to nothing.
.PHONY: sdl2-upstream sdl2-upstream-clean
sdl2-upstream:
	@$(OOPS_SDL_DIR)/../../../common/upstream-fetch.sh \
	    "$$(sed -n 's/^UPSTREAM_KIND=//p' $(OOPS_SDL_DIR)/upstream.lock)" \
	    "$$(sed -n 's/^UPSTREAM_URL=//p'  $(OOPS_SDL_DIR)/upstream.lock)" \
	    "$$(sed -n 's/^UPSTREAM_REV=//p'  $(OOPS_SDL_DIR)/upstream.lock)" \
	    "$(OOPS_SDL_UPSTREAM)" "$(OOPS_SDL_DIR)/patches"

sdl2-upstream-clean:
	@rm -rf $(OOPS_SDL_UPSTREAM)
	@echo "sdl2: removed upstream/"
