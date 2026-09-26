# SDL3 build integration. Include this from a title's Makefile, before `common/app.mk`:
#
#   OOPS_SDL3 ?= $(abspath ../../oops-deps/sdl3)
#   include $(OOPS_SDL3)/oops-sdl3.mk
#
#   EXTRA_TARGET_CFLAGS  += $(OOPS_SDL3_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_SDL3_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_SDL3_LIB)
#
# For ports that are SDL3 programs; `oops-deps/sdl2` serves SDL2 programs. A title includes one
# `.mk` or the other, and the variables are prefixed differently so a mix fails at link.
#
# SDL3's private-platform hooks do the registration: `SDL_PLATFORM_PRIVATE` selects
# `include/SDL_build_config_private.h`, and `SDL_video.c:89`, `SDL_audio.c:29` and
# `SDL_joystick.c:55` begin their driver lists with a `_PRIVATE` entry. `patches/0001` covers
# the thread backend, which the config chain does not route.
#
# Built as an archive so upstream compiles without the title's `-Werror -Wconversion` set.

# Simply expanded rather than `?=`, which would re-evaluate `$(lastword $(MAKEFILE_LIST))` in a
# recipe, by which time it names `common/app.mk`.
ifndef OOPS_SDL3_DIR
OOPS_SDL3_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_SDL3_UPSTREAM ?= $(OOPS_SDL3_DIR)/upstream

# `SDL_PLATFORM_PRIVATE` is the whole of the platform registration; see the config header.
#
# `backend/` is on the include path because `SDL_thread_c.h` needs `SYS_ThreadHandle` to lay out
# `struct SDL_Thread`.
OOPS_SDL3_INCLUDE := \
    -DSDL_PLATFORM_PRIVATE=1 \
    -I$(OOPS_SDL3_DIR)/include \
    -I$(OOPS_SDL3_DIR)/backend \
    -I$(OOPS_SDL3_UPSTREAM)/include \
    -I$(OOPS_SDL3_UPSTREAM)/include/build_config \
    -I$(OOPS_SDL3_UPSTREAM)/src

# Directories by wildcard, chosen to match the config, so a file upstream adds is not lost.
# Absent: `src/test/`, `src/hidapi/`, `src/core/`, `src/gpu/{d3d12,metal,vulkan}/`, every
# platform backend but the dummies, and render back ends other than OpenGL and software.
#
# `src/main/SDL_main_callbacks.c` alone of `src/main/`: the event loop calls
# `SDL_HasMainCallbacks` and `SDL_IterateMainCallbacks` whatever style a program uses.
# `src/haptic/dummy/` answers for `SDL_haptic.c`, which `SDL_HAPTIC_DISABLED` does not remove.
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

# The generic thread primitives `backend/SDL_prosperothread.c` replaces. The rest of
# `src/thread/generic/` (condition variable, thread-local storage) is built on them and stays.
OOPS_SDL3_THREAD_DROP := \
    $(OOPS_SDL3_UPSTREAM)/src/thread/generic/SDL_systhread.c \
    $(OOPS_SDL3_UPSTREAM)/src/thread/generic/SDL_sysmutex.c \
    $(OOPS_SDL3_UPSTREAM)/src/thread/generic/SDL_syssem.c
OOPS_SDL3_C_SRCS := $(filter-out $(OOPS_SDL3_THREAD_DROP),$(OOPS_SDL3_C_SRCS))

# The archive is built with the consumer's code-generation flags and without its warning set.
# `TARGET_CFLAGS` is set by `common/app.mk`, so these are recursive (`=`).
OOPS_SDL3_BUILD ?= $(OOPS_SDL3_DIR)/build
OOPS_SDL3_LIB := $(OOPS_SDL3_BUILD)/libSDL3.a

OOPS_SDL3_WARN_DROP := -Werror -Wconversion -Wsign-conversion -Wshadow -Wcast-qual \
                       -Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition \
                       -Wdouble-promotion -Wpedantic -pedantic
OOPS_SDL3_CFLAGS = $(filter-out $(OOPS_SDL3_WARN_DROP),$(TARGET_CFLAGS)) -w $(OOPS_SDL3_INCLUDE)

OOPS_SDL3_OBJS := $(patsubst %.c,$(OOPS_SDL3_BUILD)/%.o,$(subst /,_,$(OOPS_SDL3_C_SRCS)))

# `--whole-archive` because `app.mk` puts LDFLAGS before the objects that need them, and an
# archive seen first would contribute nothing.
OOPS_SDL3_LDFLAGS := -Wl,--whole-archive $(OOPS_SDL3_LIB) -Wl,--no-whole-archive

# Objects are numbered and `ar` is handed the list: `ar` stores members by basename, SDL repeats
# file names across backends, and a directory glob would keep stale objects (see `oops-sdl.mk`).
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

# `make sdl3-census`: which of SDL3's sources compile for this target, with the first error of
# each failing file grouped by message.
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
