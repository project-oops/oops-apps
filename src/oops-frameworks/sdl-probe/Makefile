# sdl-probe - upstream SDL2 on the console, through oops-sdl.
#
#   make elf        the plain-ELF payload a homebrew loader runs
#   make title      complete title package (.zip)
#
# **The first app to include `oops-sdl.mk`**, and that is its job as much as the frames it draws:
# a dependency that nothing builds is a dependency nobody has checked, and the undefined-symbol
# check `common/app.mk` runs on every payload link is the only place a missing SDL source shows
# up before the console.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# SDL2, pinned and patched. `oops-sdl.mk` expands a file list and declares an archive rule; it
# fetches nothing by itself, so `make -C ../../oops-deps/sdl2 sdl2-upstream` is what puts the
# tree on disk the first time.
#
# SDL arrives as an archive rather than as sources in `PAYLOAD_SRCS`, so upstream compiles under
# its own warning flags instead of this repository's `-Werror -Wsign-conversion`. That is the
# same route Mesa takes through `common/app.mk`, and `oops-sdl.mk` says why.
OOPS_SDL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2)
include $(OOPS_SDL)/oops-sdl.mk

EXTRA_TARGET_CFLAGS  += $(OOPS_SDL_INCLUDE)
EXTRA_TARGET_LDFLAGS += $(OOPS_SDL_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(OOPS_SDL_LIB)

OOPS_RENDERER = gl1
OOPS_FEATURES := display input keyboard mouse audio thread draw

PAYLOAD_SRCS = sdl_probe_main.c

ENTRY_POINT := sdl_probe_start

include $(OOPS_APPS_ROOT)/common/app.mk
