# SDL3 build integration. Include this from a title's Makefile, before `common/app.mk`:
#
#   OOPS_SDL3 ?= $(abspath ../../oops-deps/sdl3)
#   include $(OOPS_SDL3)/oops-sdl3.mk
#
#   EXTRA_TARGET_CFLAGS  += $(OOPS_SDL3_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_SDL3_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_SDL3_LIB)
#
# # A second SDL, not a bump
#
# `oops-deps/sdl2` stays where it is - five titles are built against it, and Extreme Tux Racer
# reaches it through `sdl12-compat`, which is itself an SDL2 program. This is for the ports that
# are SDL3 programs. A title includes one `.mk` or the other; nothing includes both, and the
# variables are prefixed differently so that a title which tried would get a link error rather
# than a silent mix.
#
# **`sdl2-compat` is not a way to avoid this.** The name reads the other way round: it runs SDL2
# programs on top of SDL3.
#
# # No patches, and that is SDL3's doing
#
# `patches/` is empty. SDL3 has a supported extension point for a platform it has never heard of -
# `SDL_PLATFORM_PRIVATE` selects `include/SDL_build_config_private.h`, and `SDL_video.c:89`,
# `SDL_audio.c:29` and `SDL_joystick.c:55` each begin their driver list with a `_PRIVATE` entry.
# There is a `_PRIVATE` selector for every subsystem a console has to supply. `oops-deps/sdl2`
# carries a patch that adds an arm to the config chain and registers each backend by hand; none of
# that is needed here, and a patch appearing in this directory would be a sign of working against
# an interface SDL already provides.
#
# # An archive, and not source inclusion
#
# The same reasoning `oops-sdl.mk` gives: this is somebody else's tree and the consumer's flags are
# `-Werror -Wconversion -Wsign-conversion -Wshadow`. Upstream does not compile clean under those
# and has no reason to. An archive gives upstream its own flags without relaxing the title's.

# **Simply expanded, and `?=` will not do** - `?=` defines a recursive variable, so
# `$(lastword $(MAKEFILE_LIST))` would be re-evaluated in a recipe, by which time it names
# `common/app.mk`. `oops-sdl.mk` records the 130 object files that went into `common/` before a
# link failure said so.
ifndef OOPS_SDL3_DIR
OOPS_SDL3_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_SDL3_UPSTREAM ?= $(OOPS_SDL3_DIR)/upstream

# `SDL_PLATFORM_PRIVATE` is the whole of the platform registration; see the config header.
#
# `backend/` is on the include path because SDL's own headers reach our types: `SDL_thread_c.h`
# has to see `SYS_ThreadHandle` before it can lay out `struct SDL_Thread`, exactly as in SDL2.
OOPS_SDL3_INCLUDE := \
    -DSDL_PLATFORM_PRIVATE=1 \
    -I$(OOPS_SDL3_DIR)/include \
    -I$(OOPS_SDL3_DIR)/backend \
    -I$(OOPS_SDL3_UPSTREAM)/include \
    -I$(OOPS_SDL3_UPSTREAM)/include/build_config \
    -I$(OOPS_SDL3_UPSTREAM)/src

# **Wildcards, not a list of files.** SDL builds with CMake and we do not, so the file list is ours
# to keep correct across bumps - and a hand-written list of three hundred names is one that
# silently loses a file the day upstream adds one. The config decides which directories are in
# play, and everything in them compiles.
#
# What is deliberately absent, each because the config above turns it off or the platform has no
# use for it: `src/test/`, `src/hidapi/`, `src/core/` (per-OS bring-up),
# `src/gpu/{d3d12,metal,vulkan}/` (the GPU API's backends, none of which exists here), every
# `src/*/` platform backend but the dummies, and the render back ends other than OpenGL and
# software.
#
# **Two entries here are a single file rather than a directory, and both were found by the link
# rather than by reading.**
#
#   `src/main/SDL_main_callbacks.c`  The rest of `src/main/` is the entry-point machinery a payload
#                                    replaces, so the directory is out - but `SDL_HasMainCallbacks`
#                                    and `SDL_IterateMainCallbacks` are called from the event loop
#                                    whether or not a program uses that style.
#   `src/haptic/dummy/`              `SDL_HAPTIC_DISABLED` does not remove `SDL_haptic.c`; the
#                                    dummy backend is what answers for it, and its own guard is
#                                    `#if defined(SDL_HAPTIC_DUMMY) || defined(SDL_HAPTIC_DISABLED)`.
#                                    Twenty-one `SDL_SYS_Haptic*` symbols said so.
OOPS_SDL3_C_SRCS := \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/atomic/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/audio/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/audio/dummy/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/camera/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/camera/dummy/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/cpuinfo/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/dialog/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/dialog/dummy/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/dynapi/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/events/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/filesystem/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/gpu/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/haptic/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/haptic/dummy/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/io/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/io/generic/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/joystick/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/libm/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/loadso/dummy/*.c) \
    $(OOPS_SDL3_UPSTREAM)/src/main/SDL_main_callbacks.c \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/locale/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/locale/dummy/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/misc/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/misc/dummy/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/power/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/process/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/process/dummy/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/render/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/render/opengl/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/render/software/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/sensor/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/sensor/dummy/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/stdlib/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/storage/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/storage/generic/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/thread/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/thread/generic/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/time/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/timer/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/tray/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/tray/dummy/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/video/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/video/dummy/*.c) \
    $(wildcard $(OOPS_SDL3_UPSTREAM)/src/video/yuv2rgb/*.c) \
    $(wildcard $(OOPS_SDL3_DIR)/backend/*.c)

# Ours last, deliberately: `backend/` is the only entry that is not a wildcard over somebody else's
# tree, and putting it at the end keeps that visible in the variable rather than buried.

# **The generic thread primitives our backend replaces.** A directory is the wrong unit here:
# `src/thread/generic/` holds the "threads are not supported" stubs beside real implementations
# built on the primitives a platform provides. Compiling ours *and* theirs is duplicate symbols;
# compiling only ours loses the condition variable and the thread-local storage, which are real.
# The list is short and is checked by the link rather than assumed - `oops-deps/sdl2` learned the
# same lesson one file at a time.
OOPS_SDL3_THREAD_DROP := \
    $(OOPS_SDL3_UPSTREAM)/src/thread/generic/SDL_systhread.c \
    $(OOPS_SDL3_UPSTREAM)/src/thread/generic/SDL_sysmutex.c \
    $(OOPS_SDL3_UPSTREAM)/src/thread/generic/SDL_syssem.c
OOPS_SDL3_C_SRCS := $(filter-out $(OOPS_SDL3_THREAD_DROP),$(OOPS_SDL3_C_SRCS))

# ---------------------------------------------------------------------------
# The archive
#
# Built with the consumer's target flags for everything that decides code generation - the triple,
# `-ffreestanding`, the target macros - and **without** the warning set, which is ours and not
# upstream's. `TARGET_CFLAGS` is not known until `common/app.mk` has been read, so these are
# recursive (`=`) and resolve when the recipe runs.
OOPS_SDL3_BUILD ?= $(OOPS_SDL3_DIR)/build
OOPS_SDL3_LIB := $(OOPS_SDL3_BUILD)/libSDL3.a

OOPS_SDL3_WARN_DROP := -Werror -Wconversion -Wsign-conversion -Wshadow -Wcast-qual \
                       -Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition \
                       -Wdouble-promotion -Wpedantic -pedantic
OOPS_SDL3_CFLAGS = $(filter-out $(OOPS_SDL3_WARN_DROP),$(TARGET_CFLAGS)) -w $(OOPS_SDL3_INCLUDE)

OOPS_SDL3_OBJS := $(patsubst %.c,$(OOPS_SDL3_BUILD)/%.o,$(subst /,_,$(OOPS_SDL3_C_SRCS)))

# **`--whole-archive`, deliberately.** `app.mk` puts LDFLAGS before the sources on the link line,
# and a static archive seen before the objects that need it contributes nothing. The cost is that
# unused parts of SDL stay in the payload; the alternative is a link that succeeds and leaves out
# the driver.
OOPS_SDL3_LDFLAGS := -Wl,--whole-archive $(OOPS_SDL3_LIB) -Wl,--no-whole-archive

# One recipe walking the list rather than a pattern rule per directory, and **`ar` is handed the
# list rather than the directory**. `oops-sdl.mk` records why: `ar` stores a member under its
# basename alone, SDL is the one tree where the same file name appears in several backends, and a
# glob over the build directory leaves the previous run's objects in the archive when the source
# list shortens - with the link staying clean.
$(OOPS_SDL3_LIB): $(OOPS_SDL3_C_SRCS) $(OOPS_SDL3_DIR)/include/SDL_build_config_private.h \
                  $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_SDL3_BUILD)
	@rm -f $@
	@n=0; objs=""; for s in $(OOPS_SDL3_C_SRCS); do n=$$((n+1)); \
	   o=$(OOPS_SDL3_BUILD)/s$$n.o; \
	   $(TARGET_CC) $(OOPS_SDL3_CFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 echo "SDL3: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs
	@echo "SDL3: $@"

.PHONY: sdl3-clean
sdl3-clean:
	@rm -rf $(OOPS_SDL3_BUILD)

# ---------------------------------------------------------------------------
# `make sdl3-census`: which of SDL3's sources compile for this target, and what each died on
#
# The same shape as a title's census, and here for the same reason: bringing up a backend is a long
# sequence of "what does it want next", and one error per file grouped by message answers that far
# faster than a build that stops at the first one.
# ---------------------------------------------------------------------------
.PHONY: sdl3-census
sdl3-census:
	@ok=0; bad=0; mkdir -p $(OOPS_SDL3_BUILD); : > $(OOPS_SDL3_BUILD)/census.txt; \
	 for s in $(OOPS_SDL3_C_SRCS); do \
	    if $(TARGET_CC) $(OOPS_SDL3_CFLAGS) -fsyntax-only "$$s" 2>$(OOPS_SDL3_BUILD)/e.txt; \
	      then ok=$$((ok+1)); \
	    else bad=$$((bad+1)); \
	      grep -m1 'error:' $(OOPS_SDL3_BUILD)/e.txt | sed 's|.*error: ||' | cut -c1-70 \
	        >> $(OOPS_SDL3_BUILD)/census.txt; \
	    fi; \
	 done; \
	 printf 'SDL3 census: %s compile, %s do not\n' "$$ok" "$$bad"; \
	 if [ "$$bad" -gt 0 ]; then printf '\n-- blockers, most files first --\n'; \
	    sort $(OOPS_SDL3_BUILD)/census.txt | uniq -c | sort -rn | head -25; fi
