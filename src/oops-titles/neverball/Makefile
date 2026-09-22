# Neverball, through oops-sdk.
#
#   make elf      the plain-ELF payload a homebrew loader runs
#   make title    complete title package (.zip)
#
# **The data has to be built before this will run**, and that is not optional. Neverball ships
# 407 `.map` files and no `.sol`: its levels are compiled by `mapc`, a **host** tool built from
# thirteen of its own sources. `make data` below does it. A payload without that has no levels.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Fixed-function OpenGL, freestanding. This is `common/app.mk`'s front door for choosing a
# renderer; it adds oops-gl's sources itself, so this title does not list them.
OOPS_RENDERER = gl1

# --- the stack ------------------------------------------------------------
#
# Eight dependencies, each pinned in `src/oops-deps/`. The order matters where one needs
# another's headers on the path: SDL2 before its mixer and ttf, FreeType before ttf, zlib and
# libogg are pulled in by libpng and libvorbis themselves.
OOPS_SDL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2)
include $(OOPS_SDL)/oops-sdl.mk

OOPS_FT ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/freetype)
include $(OOPS_FT)/oops-freetype.mk

OOPS_TTF ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2-ttf)
include $(OOPS_TTF)/oops-sdl2-ttf.mk

OOPS_PNG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libpng)
include $(OOPS_PNG)/oops-libpng.mk

OOPS_JPEG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libjpeg-turbo)
include $(OOPS_JPEG)/oops-libjpeg.mk

OOPS_VORBIS ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libvorbis)
include $(OOPS_VORBIS)/oops-libvorbis.mk

# The shared POSIX shim - `stat`, `opendir`, `readdir`, `getcwd`. Shared with Extreme Tux Racer.
include $(OOPS_APPS_ROOT)/common/posix.mk

# --- flags ----------------------------------------------------------------
#
# `ENABLE_NLS=0` drops gettext. The filesystem is stdio rather than PhysicsFS, which is
# upstream's own supported switch and is why `share/fs_stdio.c` is in the list below and
# `fs_physfs.c` is not.
#
NB_CFLAGS := -DENABLE_NLS=0 -DZ_SOLO \
             -Ishim/include -Iupstream/share \
             -DOOPS -DOOPS_POSIX_HOME=\"/data/neverball\" \
             $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE) $(OOPS_TTF_INCLUDE) $(OOPS_FT_INCLUDE) \
             $(OOPS_PNG_INCLUDE) $(OOPS_JPEG_INCLUDE) $(OOPS_VORBIS_INCLUDE)

EXTRA_TARGET_CFLAGS  += $(NB_CFLAGS)
EXTRA_TARGET_LDFLAGS += $(OOPS_SDL_LDFLAGS) $(OOPS_TTF_LDFLAGS) $(OOPS_FT_LDFLAGS) \
                        $(OOPS_PNG_LDFLAGS) $(OOPS_JPEG_LDFLAGS) $(OOPS_VORBIS_LDFLAGS)
# `zlib` and `libogg` are named here although this title never mentions them: `oops-libpng.mk`
# and `oops-libvorbis.mk` include them and put their archives on the link line, but a *flag* is
# not a prerequisite - make will not build a file just because a linker was told about it. Left
# out, the first link fails on two archives that nothing had been asked to produce.
PAYLOAD_EXTRA_DEPS   += $(OOPS_SDL_LIB) $(OOPS_TTF_LIB) $(OOPS_FT_LIB) \
                        $(OOPS_PNG_LIB) $(OOPS_ZLIB_LIB) $(OOPS_JPEG_LIB) \
                        $(OOPS_VORBIS_LIB) $(OOPS_OGG_LIB)

# --- sources --------------------------------------------------------------
#
# **This is upstream's own `BALL_OBJS`**, not a glob. A glob over `share/` sweeps in `mapc.c`
# (a host tool with its own `main`), `fs_physfs.c`, `hmd_openhmd.c` and the two tilt files - all
# of which the switches above exclude and none of which would compile. Reading upstream's build
# is cheaper than discovering that from link errors, which is the same lesson libvorbis taught.
#
# `solid_sim_sol` and `tilt_null` are the two upstream adds further down its own Makefile rather
# than in `BALL_OBJS` itself, which is why reading only that variable left them out: the physics
# solver is chosen at line 337 and the tilt backend at 351-356, where `tilt_null.c` is the arm
# for a build with no Wiimote and no accelerometer. Ours is that arm.
NB_SHARE := lang st_common vec3 base_image image solid_base solid_vary solid_draw solid_all \
            solid_sim_sol tilt_null hmd_null hmd_common \
            mtrl part geom ball gui font theme base_config config video glext binary state \
            audio text common list queue cmd array dir fbo glsl fs_common fs_stdio fs_png \
            fs_jpg fs_rwops fs_ov log
NB_BALL  := hud game_common game_client game_server game_proxy game_draw score level progress \
            set demo demo_dir util st_conf st_demo st_save st_goal st_fail st_done st_level \
            st_over st_play st_set st_start st_title st_help st_name st_shared st_pause st_ball \
            main

NB_SRCS := $(addprefix upstream/share/,$(addsuffix .c,$(NB_SHARE))) \
           $(addprefix upstream/ball/,$(addsuffix .c,$(NB_BALL)))

# --- upstream's C, under upstream's flags ---------------------------------
#
# **Neverball's own sources build into an archive, not into `PAYLOAD_SRCS`**, for the reason
# `oops-sdl.mk` and `common/cxx.mk` give: this repository compiles with `-Werror -Wconversion
# -Wsign-conversion -Wmissing-field-initializers`, and a 2010 C codebase does not survive that.
#
# The first attempt put these in `PAYLOAD_SRCS` and added `-Wno-` flags as each class appeared -
# field initializers, then sign comparisons, and there is no end to that list. Worse, every flag
# added to silence upstream also silences it for `shim/nb_start.c`, which **is** ours and should
# be held to the repository's standard. An archive separates the two properly.
OOPS_NB_BUILD := build/nb
OOPS_NB_LIB := $(OOPS_NB_BUILD)/libneverball.a
# **`-fcommon` is not optional here.** Neverball declares globals like `text_input` in headers
# without `extern`, which C calls a tentative definition: every translation unit that includes
# the header emits one, and the linker was expected to merge them. GCC did that by default until
# version 10 and clang does not, so without this the link fails on a duplicate symbol in each of
# the four files that include `text.h`.
#
# The alternative is patching upstream's headers to add `extern` and picking one file to hold
# each variable - a real change to somebody else's code, for a compiler default that changed
# after it was written. A flag that says "behave like the compiler this was written for" is the
# smaller and more honest answer.
OOPS_NB_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                 -nostdlibinc -fPIC -O2 -w -std=c99 -fcommon $(NB_CFLAGS) \
                 $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

# `--whole-archive` because `app.mk` puts LDFLAGS before the sources on the link line, and
# because `main` itself lives in here - a plain archive seen too early contributes nothing.
EXTRA_TARGET_LDFLAGS += -Wl,--whole-archive $(OOPS_NB_LIB) -Wl,--no-whole-archive
PAYLOAD_EXTRA_DEPS   += $(OOPS_NB_LIB)

$(OOPS_NB_LIB): $(NB_SRCS) $(MAKEFILE_LIST)
	@mkdir -p $(OOPS_NB_BUILD)
	@rm -f $@
	@n=0; for s in $(NB_SRCS); do n=$$((n+1)); \
	   $(TARGET_CC) $(OOPS_NB_CFLAGS) -c -o $(OOPS_NB_BUILD)/nb$$n.o "$$s" || exit 1; done; \
	 echo "neverball: compiled $$n sources"
	@a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $(OOPS_NB_BUILD)/nb*.o
	@echo "neverball: $@"

# `shim/nb_diag.c` is here rather than in `NB_SHARE` deliberately: it holds the format strings
# behind the one patch this title carries, and the archive above compiles with `-w`. In
# `PAYLOAD_SRCS` it gets `-Werror -Wformat` like the rest of our code. See `shim/include/nb_diag.h`.
PAYLOAD_SRCS = shim/nb_start.c \
               shim/nb_diag.c \
               $(OOPS_SDK_DIR)/src/system/savedata.c \
               $(OOPS_POSIX_SRCS) \
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

ENTRY_POINT := nb_start

include $(OOPS_APPS_ROOT)/common/app.mk

# --- the data -------------------------------------------------------------
#
# `mapc` is a host tool: it runs on the build machine, reads a `.map` and writes a `.sol` beside
# the rest of the data. It is built with the host compiler against the *same pinned* zlib, libpng
# and libjpeg sources the payload uses - the build machine has no dev packages for those, and
# reaching for them would put a dependency outside the lock files.
#
# `scripts/mapc.sh` does the whole of it. It is a separate target because it takes minutes and
# nothing else needs re-running when only the payload changes.
.PHONY: data data-clean package
data:
	@bash scripts/mapc.sh

# **`make package` is the one to run**, not `make title`.
#
# `common/app.mk` builds a title directory out of the payload and `sce_sys/`, which is everything
# an app written here needs. A *port* also ships the program's own content, and Neverball's is
# 252 MB of levels, textures, fonts and music that it will not start without. `app.mk` has no
# hook for that and should not grow one for a single title, so this copies it afterwards.
#
# The data must be compiled first: `make data` turns 407 `.map` files into the `.sol` the game
# actually loads, and a package built before that runs has textures and no levels.
package: title
	@test -d upstream/data || { echo "neverball: no upstream/data - run make first" >&2; exit 1; }
	@if [ "$$(find upstream/data -name '*.sol' | wc -l)" -eq 0 ]; then \
	    echo "neverball: no compiled levels - run 'make data' first" >&2; exit 1; fi
	@echo "neverball: copying $$(du -sh upstream/data | cut -f1) of data into the package"
	@rm -rf build/title/$(TITLE_ID)/data
	@cp -r upstream/data build/title/$(TITLE_ID)/data
	@echo "neverball: package is build/title/$(TITLE_ID) ($$(du -sh build/title/$(TITLE_ID) | cut -f1))"

data-clean:
	@find upstream/data -name '*.sol' -delete 2>/dev/null || true
	@rm -rf build/mapc
	@echo "neverball: removed compiled levels"
