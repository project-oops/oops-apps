# SDL2 build integration. Include this from a title's Makefile, before `common/app.mk`:
#
#   OOPS_SDL ?= $(abspath ../../oops-deps/sdl2)
#   include $(OOPS_SDL)/oops-sdl.mk
#
#   EXTRA_TARGET_CFLAGS  += $(OOPS_SDL_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_SDL_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_SDL_LIB)
#
# SDL is built as its own archive, not by source inclusion (oops-sdk#D003 covers the SDK only),
# so upstream compiles without this repository's warning set and the title keeps it.
# `--whole-archive` because `app.mk` puts LDFLAGS before the objects, and SDL's drivers are
# reached only through bootstrap tables. `make sdl2-upstream` fetches the tree.

# `ifndef` plus `:=` resolves the path once, here: a recursive `?=` would re-evaluate
# `MAKEFILE_LIST` in recipes, where it ends with `common/app.mk`.
ifndef OOPS_SDL_DIR
OOPS_SDL_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_SDL_UPSTREAM ?= $(OOPS_SDL_DIR)/upstream

# `__PROSPERO__` selects `include/SDL_config_prospero.h` through the arm `patches/0001-*` adds
# to upstream's config chain. `backend/` is on the path because the patched
# `src/thread/SDL_thread_c.h` needs its `SYS_ThreadHandle`.
OOPS_SDL_INCLUDE := \
    -D__PROSPERO__=1 \
    -I$(OOPS_SDL_DIR)/include \
    -I$(OOPS_SDL_DIR)/backend \
    -I$(OOPS_SDL_UPSTREAM)/include \
    -I$(OOPS_SDL_UPSTREAM)/src

# Ports that write `#include <SDL2/SDL.h>` add the prefix view:
#
#   EXTRA_TARGET_CFLAGS += $(OOPS_SDL_INCLUDE) $(OOPS_SDL_PREFIX_INCLUDE)
#   <your archive>: | $(OOPS_SDL_PREFIX_STAMP)
#
# It copies the headers into `build/prefix/SDL2/` rather than committing forwarding headers or
# a symlink. The stamp is order-only: a newer header is not a reason to relink.

# Wildcards over the directories the config enables, so a file upstream adds is not lost.
# Absent: `src/main/` (the payload has its own entry point), `src/test/`, `src/hidapi/`,
# `src/core/`, and every platform backend except the dummies.
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

# `SDL_steam_virtual_gamepad.c` stays in: `SDL_joystick.c` calls it unconditionally. Its
# `<sys/stat.h>` use is patched out in `patches/0001-*`.

# `backend/SDL_prosperothread.c` provides threads, mutexes and semaphores, so the generic ones
# are dropped. The generic condition variable and thread-local storage remain, built on them.
OOPS_SDL_C_SRCS := $(filter-out \
    $(OOPS_SDL_UPSTREAM)/src/thread/generic/SDL_systhread.c \
    $(OOPS_SDL_UPSTREAM)/src/thread/generic/SDL_sysmutex.c \
    $(OOPS_SDL_UPSTREAM)/src/thread/generic/SDL_syssem.c, \
    $(OOPS_SDL_C_SRCS))

# The archive is built with the consumer's code-generation flags and without its warning set.
# `TARGET_CFLAGS` is defined later by `common/app.mk`, so the flags are recursive (`=`).
OOPS_SDL_BUILD ?= $(OOPS_SDL_DIR)/build
OOPS_SDL_LIB := $(OOPS_SDL_BUILD)/libSDL2.a

# The `SDL2/`-prefixed header view, defined after `OOPS_SDL_BUILD`, which its rule names.
OOPS_SDL_PREFIX_DIR := $(OOPS_SDL_BUILD)/prefix
OOPS_SDL_PREFIX_INCLUDE := -I$(OOPS_SDL_PREFIX_DIR)
OOPS_SDL_PREFIX_STAMP := $(OOPS_SDL_PREFIX_DIR)/.stamp

# `include/SDL_config_prospero.h` is copied in too: SDL's headers reach their config through a
# quoted include, resolved beside the including file first.
$(OOPS_SDL_PREFIX_STAMP): $(wildcard $(OOPS_SDL_UPSTREAM)/include/*.h) \
                          $(wildcard $(OOPS_SDL_DIR)/include/*.h)
	@rm -rf $(OOPS_SDL_PREFIX_DIR)/SDL2
	@mkdir -p $(OOPS_SDL_PREFIX_DIR)/SDL2
	@cp $(OOPS_SDL_UPSTREAM)/include/*.h $(OOPS_SDL_PREFIX_DIR)/SDL2/
	@cp $(OOPS_SDL_DIR)/include/*.h $(OOPS_SDL_PREFIX_DIR)/SDL2/
	@echo "SDL: $$(ls $(OOPS_SDL_PREFIX_DIR)/SDL2 | wc -l) headers under SDL2/"
	@touch $@

# Warnings upstream's code trips and is not written against.
OOPS_SDL_WARN_DROP := -Werror -Wconversion -Wsign-conversion -Wshadow -Wcast-qual \
                      -Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition \
                      -Wdouble-promotion -Wpedantic -pedantic
OOPS_SDL_CFLAGS = $(filter-out $(OOPS_SDL_WARN_DROP),$(TARGET_CFLAGS)) -w $(OOPS_SDL_INCLUDE)

OOPS_SDL_OBJS := $(patsubst %.c,$(OOPS_SDL_BUILD)/%.o,$(subst /,_,$(OOPS_SDL_C_SRCS)))

OOPS_SDL_LDFLAGS := -Wl,--whole-archive $(OOPS_SDL_LIB) -Wl,--no-whole-archive

# One recipe walks the list and numbers the objects, because `ar` stores members by basename
# and SDL2 has the same file name in several backends. `ar` is handed the list rather than
# the directory, so objects left from a longer list never enter the archive (`common/deps.mk`).
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

# Fetches the tree on its own. A title gets the fetch through `common/app.mk`, since a wildcard
# over an absent tree expands to nothing.
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
