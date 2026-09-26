# Neverball, through oops-sdk.
#
#   make elf      the plain-ELF payload a homebrew loader runs
#   make title    complete title package (.zip)
#
#   make data     compile the levels (required before a package runs)
#   make package  the title package with Neverball's data, which is what runs
#
# Neverball ships `.map` files and no `.sol`: its levels are compiled by `mapc`, a host tool
# built from its own sources.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Fixed-function OpenGL; `common/app.mk` adds oops-gl's sources itself.
OOPS_RENDERER = gl1

# The SDK sources this title compiles carry no `-O` of their own, and unoptimised oops-gl is
# too slow for Neverball's draw-call count. Neverball's own archive asks for -O2 below.
EXTRA_TARGET_CFLAGS += -O2

# --- the stack ------------------------------------------------------------
#
# Each dependency is pinned in `src/oops-deps/`. The order matters where one needs another's
# headers on the path: SDL2 before its mixer and ttf, FreeType before ttf. zlib and libogg
# are pulled in by libpng and libvorbis themselves.
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

# The shared POSIX shim - `stat`, `opendir`, `readdir`, `getcwd`.
include $(OOPS_APPS_ROOT)/common/posix.mk

# Included here rather than left to `app.mk`, because the archive rule below uses
# `oops_depgen` and `app.mk` is read after it. The include is guarded.
include $(OOPS_APPS_ROOT)/common/deps.mk

# --- flags ----------------------------------------------------------------
#
# `ENABLE_NLS=0` drops gettext. The filesystem is stdio rather than PhysicsFS, upstream's own
# switch, so `share/fs_stdio.c` is in the list below and `fs_physfs.c` is not.
#
# No `OOPS_POSIX_HOME`: it only affects `getpwuid`, and `pick_home_path`
# (share/base_config.c:70) reads `getenv("HOME")` and falls back to `fs_base_dir()`. The
# write directory is `/app0/.neverball`.
NB_CFLAGS := -DENABLE_NLS=0 -DZ_SOLO \
             -Ishim/include -Iupstream/share \
             -I$(OOPS_APPS_ROOT)/common \
             -DOOPS \
             $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE) $(OOPS_TTF_INCLUDE) $(OOPS_FT_INCLUDE) \
             $(OOPS_PNG_INCLUDE) $(OOPS_JPEG_INCLUDE) $(OOPS_VORBIS_INCLUDE)

EXTRA_TARGET_CFLAGS  += $(NB_CFLAGS)
EXTRA_TARGET_LDFLAGS += $(OOPS_SDL_LDFLAGS) $(OOPS_TTF_LDFLAGS) $(OOPS_FT_LDFLAGS) \
                        $(OOPS_PNG_LDFLAGS) $(OOPS_JPEG_LDFLAGS) $(OOPS_VORBIS_LDFLAGS)
# `zlib` and `libogg` are named here because the link flags from `oops-libpng.mk` and
# `oops-libvorbis.mk` name their archives, and a flag is not a prerequisite make will build.
PAYLOAD_EXTRA_DEPS   += $(OOPS_SDL_LIB) $(OOPS_TTF_LIB) $(OOPS_FT_LIB) \
                        $(OOPS_PNG_LIB) $(OOPS_ZLIB_LIB) $(OOPS_JPEG_LIB) \
                        $(OOPS_VORBIS_LIB) $(OOPS_OGG_LIB)

# --- sources --------------------------------------------------------------
#
# Upstream's own `BALL_OBJS`, not a glob: a glob over `share/` sweeps in `mapc.c` (a host
# tool with its own `main`), `fs_physfs.c`, `hmd_openhmd.c` and the tilt backends.
# `solid_sim_sol` is the physics solver upstream adds at Makefile:337.
# The tilt backend upstream picks at Makefile:349-357 is replaced by a fourth arm for the
# pad's IMU, `shim/nb_tilt_dualsense.c` in PAYLOAD_SRCS below, so tilt needs no patch.
NB_SHARE := lang st_common vec3 base_image image solid_base solid_vary solid_draw solid_all \
            solid_sim_sol hmd_null hmd_common \
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
# Neverball's sources build into an archive, not `PAYLOAD_SRCS`: this repository compiles with
# `-Werror -Wconversion -Wsign-conversion -Wmissing-field-initializers`, which upstream does
# not meet, and our shim sources stay under those flags.
OOPS_NB_BUILD := build/nb
OOPS_NB_LIB := $(OOPS_NB_BUILD)/libneverball.a
# `-fcommon`: Neverball declares globals such as `text_input` in headers without `extern`
# (tentative definitions), which clang does not merge by default.
OOPS_NB_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                 -nostdlibinc -fPIC -O2 -w -std=c99 -fcommon $(NB_CFLAGS) \
                 $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

# `--whole-archive` because `app.mk` puts LDFLAGS before the sources on the link line, and
# because `main` itself lives in here - a plain archive seen too early contributes nothing.
EXTRA_TARGET_LDFLAGS += -Wl,--whole-archive $(OOPS_NB_LIB) -Wl,--no-whole-archive
PAYLOAD_EXTRA_DEPS   += $(OOPS_NB_LIB)

# Header dependencies are tracked (`common/deps.mk`), since this archive carries `main`.
# Each object is named after its source and `ar` is handed the list, so a source removed
# from `NB_SHARE` or `NB_BALL` leaves nothing behind in the archive.
OOPS_NB_OBJS := $(call oops_objs,$(OOPS_NB_BUILD)/obj,$(NB_SRCS))
$(call oops_ar_check,$(OOPS_NB_OBJS))
-include $(OOPS_NB_OBJS:.o=.d)
$(call oops_obj_rules,$(OOPS_NB_BUILD)/obj,TARGET_CC,OOPS_NB_CFLAGS,$(NB_SRCS))

$(OOPS_NB_LIB): $(OOPS_NB_OBJS) $(oops_makefiles)
	@mkdir -p $(OOPS_NB_BUILD)
	@rm -f $@
	@a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $(OOPS_NB_OBJS)
	@echo "neverball: $@ ($(words $(OOPS_NB_OBJS)) objects)"

# `shim/nb_diag.c` holds the format strings behind this title's patch; in `PAYLOAD_SRCS` it
# gets `-Werror -Wformat`, which the `-w` archive would not. See `shim/include/nb_diag.h`.
# SDK sources come from the feature table through `common/app.mk`; listing them too would
# duplicate symbols. `thread` is for `common/haptics.c`'s decay thread.
OOPS_FEATURES := display input keyboard mouse audio suspend thread draw

PAYLOAD_SRCS = shim/nb_start.c \
               shim/nb_diag.c \
               shim/nb_tilt_dualsense.c \
               $(OOPS_APPS_ROOT)/common/haptics.c \
               $(OOPS_POSIX_SRCS)

ENTRY_POINT := nb_start

include $(OOPS_APPS_ROOT)/common/app.mk

# --- the data -------------------------------------------------------------
#
# `mapc` is a host tool that reads a `.map` and writes a `.sol` beside the rest of the data.
# It is built with the host compiler against the same pinned zlib, libpng and libjpeg sources
# the payload uses, so no dependency sits outside the lock files. `scripts/mapc.sh` does it;
# it is a separate target because it is slow and independent of the payload.
.PHONY: data data-clean package
data:
	@bash scripts/mapc.sh

# `make package` adds Neverball's content (levels, textures, fonts, music) to the title
# package built by `common/app.mk`. `make data` must run first, or the package has no levels.
package: title
	@test -d upstream/data || { echo "neverball: no upstream/data - run make first" >&2; exit 1; }
	@if [ "$$(find upstream/data -name '*.sol' | wc -l)" -eq 0 ]; then \
	    echo "neverball: no compiled levels - run 'make data' first" >&2; exit 1; fi
	@echo "neverball: copying $$(du -sh upstream/data | cut -f1) of data into the package"
	@rm -rf build/title/$(TITLE_ID)/data
	@cp -r upstream/data build/title/$(TITLE_ID)/data
	@# **The controls card's artwork, copied in here rather than into `upstream/data`.** The
	@# help screen loads it by path like any other GUI image, but it is ours and shared with
	@# every other port - so it goes into the *packaged* data and upstream's tree stays exactly
	@# as upstream ships it. See `common/assets/controls/`.
	@cp $(OOPS_APPS_ROOT)/common/assets/controls/controller.png \
	    build/title/$(TITLE_ID)/data/gui/oops-controller.png
	@echo "neverball: package is build/title/$(TITLE_ID) ($$(du -sh build/title/$(TITLE_ID) | cut -f1))"

data-clean:
	@find upstream/data -name '*.sol' -delete 2>/dev/null || true
	@rm -rf build/mapc
	@echo "neverball: removed compiled levels"
