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

PAYLOAD_SRCS = sdl_probe_main.c \
               $(OOPS_GL_SRCS) \
               $(OOPS_SDK_DIR)/src/display.c \
               $(OOPS_SDK_DIR)/src/agc/agc_display.c \
               $(OOPS_SDK_DIR)/src/agc/agc_tiler.c \
               $(OOPS_SDK_DIR)/src/agc/agc_compute.c \
               $(OOPS_SDK_DIR)/src/input/input.c \
               $(OOPS_SDK_DIR)/src/input/keyboard.c \
               $(OOPS_SDK_DIR)/src/input/mouse.c \
               $(OOPS_SDK_DIR)/src/audio/audio.c \
               $(OOPS_SDK_DIR)/src/memory/memory.c \
               $(OOPS_SDK_DIR)/src/system/system.c \
               $(OOPS_SDK_DIR)/src/system/freestd.c \
               $(OOPS_SDK_DIR)/src/system/syscall.c \
               $(OOPS_SDK_DIR)/src/system/sysmodule.c \
               $(OOPS_SDK_DIR)/src/thread/thread.c \
               $(OOPS_SDK_DIR)/src/draw/draw.c

ENTRY_POINT := sdl_probe_start

include $(OOPS_APPS_ROOT)/common/app.mk
