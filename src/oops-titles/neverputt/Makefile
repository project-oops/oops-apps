# Neverputt, through oops-sdk.
#
#   make elf      the plain-ELF payload a homebrew loader runs
#   make title    complete title package (.zip)
#   make data     compile the courses (required before a package runs)
#   make package  the title package with the data, which is what runs
#
# Neverputt is the second binary of the Neverball tree: the sources under `putt/` and a
# subset of `share/`. The stack, the archive and the `mapc` step match
# `../neverball/Makefile`, which gives the reasons; this differs in `NP_PUTT` and `NP_SHARE`.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Fixed-function OpenGL; `common/app.mk` adds oops-gl's sources itself.
OOPS_RENDERER = gl1

# The SDK sources this title compiles carry no `-O` of their own, and unoptimised oops-gl is
# too slow per draw call. Upstream's own archive asks for -O2 below.
EXTRA_TARGET_CFLAGS += -O2

# --- the stack ------------------------------------------------------------
#
# Neverball's dependencies, in the same order: SDL2 before its ttf, FreeType before ttf,
# zlib and libogg pulled in by libpng and libvorbis.
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

# Included here rather than left to `app.mk`, because the archive rule below uses `oops_depgen`
# and `app.mk` is read after it. The include is guarded.
include $(OOPS_APPS_ROOT)/common/deps.mk

# --- flags ----------------------------------------------------------------
#
# `ENABLE_NLS=0` drops gettext. The filesystem is stdio rather than PhysicsFS, upstream's own
# switch, so `share/fs_stdio.c` is in the list below and `fs_physfs.c` is not.
#
# No `OOPS_POSIX_HOME`: it only affects `getpwuid`, and `pick_home_path`
# (share/base_config.c:70) reads `getenv("HOME")` and falls back to `fs_base_dir()`. The
# write directory is `/app0/.neverball`, as for Neverball: `CONFIG_USER` belongs to the tree.
NP_CFLAGS := -DENABLE_NLS=0 -DZ_SOLO \
             -Ishim/include -Iupstream/share \
             -I$(OOPS_APPS_ROOT)/common \
             -DOOPS \
             $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE) $(OOPS_TTF_INCLUDE) $(OOPS_FT_INCLUDE) \
             $(OOPS_PNG_INCLUDE) $(OOPS_JPEG_INCLUDE) $(OOPS_VORBIS_INCLUDE)

EXTRA_TARGET_CFLAGS  += $(NP_CFLAGS)
EXTRA_TARGET_LDFLAGS += $(OOPS_SDL_LDFLAGS) $(OOPS_TTF_LDFLAGS) $(OOPS_FT_LDFLAGS) \
                        $(OOPS_PNG_LDFLAGS) $(OOPS_JPEG_LDFLAGS) $(OOPS_VORBIS_LDFLAGS)
# zlib and libogg are named because the `.mk` files above put their archives on the link line,
# and a flag is not a prerequisite make will build.
PAYLOAD_EXTRA_DEPS   += $(OOPS_SDL_LIB) $(OOPS_TTF_LIB) $(OOPS_FT_LIB) \
                        $(OOPS_PNG_LIB) $(OOPS_ZLIB_LIB) $(OOPS_JPEG_LIB) \
                        $(OOPS_VORBIS_LIB) $(OOPS_OGG_LIB)

# --- sources --------------------------------------------------------------
#
# Upstream's own `PUTT_OBJS`, not a glob. Unlike `BALL_OBJS` it has no tilt backend
# (Makefile:349-357 adds one to `BALL_OBJS` only), no `queue` or `cmd` (Neverball's
# client/server command stream), and `hmd_null` without `hmd_common`, which belongs to the
# openhmd and libovr arms (Makefile:361-370). A glob would also take `mapc.c` and
# `fs_physfs.c`.
NP_SHARE := lang st_common vec3 base_image image solid_base solid_vary solid_draw solid_all \
            solid_sim_sol hmd_null \
            mtrl part geom ball base_config config video glext binary audio state gui font \
            theme text common list array dir fbo glsl log \
            fs_common fs_stdio fs_png fs_jpg fs_rwops fs_ov
NP_PUTT  := hud game hole course st_all st_conf main

NP_SRCS := $(addprefix upstream/share/,$(addsuffix .c,$(NP_SHARE))) \
           $(addprefix upstream/putt/,$(addsuffix .c,$(NP_PUTT)))

# --- upstream's C, under upstream's flags ---------------------------------
#
# Upstream's sources build into an archive rather than `PAYLOAD_SRCS`: this repository
# compiles with `-Werror -Wconversion -Wsign-conversion`, which upstream does not meet, and
# `shim/np_start.c` stays under those flags.
OOPS_NP_BUILD := build/np
OOPS_NP_LIB := $(OOPS_NP_BUILD)/libneverputt.a
# `-fcommon`: upstream declares globals such as `text_input` in headers without `extern`
# (tentative definitions), which clang does not merge by default.
OOPS_NP_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                 -nostdlibinc -fPIC -O2 -w -std=c99 -fcommon $(NP_CFLAGS) \
                 $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

# `--whole-archive` because `app.mk` puts LDFLAGS before the sources on the link line, and
# because `main` itself lives in here - a plain archive seen too early contributes nothing.
EXTRA_TARGET_LDFLAGS += -Wl,--whole-archive $(OOPS_NP_LIB) -Wl,--no-whole-archive
PAYLOAD_EXTRA_DEPS   += $(OOPS_NP_LIB)

# Each object is named after its source and `ar` is handed the list, so a removed source
# leaves nothing behind in the archive.
OOPS_NP_OBJS := $(call oops_objs,$(OOPS_NP_BUILD)/obj,$(NP_SRCS))
$(call oops_ar_check,$(OOPS_NP_OBJS))
-include $(OOPS_NP_OBJS:.o=.d)
$(call oops_obj_rules,$(OOPS_NP_BUILD)/obj,TARGET_CC,OOPS_NP_CFLAGS,$(NP_SRCS))

$(OOPS_NP_LIB): $(OOPS_NP_OBJS) $(oops_makefiles)
	@mkdir -p $(OOPS_NP_BUILD)
	@rm -f $@
	@a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $(OOPS_NP_OBJS)
	@echo "neverputt: $@ ($(words $(OOPS_NP_OBJS)) objects)"

# SDK sources come from the feature table through `common/app.mk`; listing them too would
# duplicate symbols. `thread` is for `common/haptics.c`'s decay thread.
OOPS_FEATURES := display input keyboard mouse audio suspend thread draw

PAYLOAD_SRCS = shim/np_start.c \
               $(OOPS_APPS_ROOT)/common/haptics.c \
               $(OOPS_POSIX_SRCS)

ENTRY_POINT := np_start

include $(OOPS_APPS_ROOT)/common/app.mk

# --- the data -------------------------------------------------------------
#
# `mapc` is a host tool that reads a `.map` and writes a `.sol` beside the rest of the data,
# built against the same pinned zlib, libpng and libjpeg sources the payload uses. It
# compiles every `.map` in the tree, Neverball's levels included.
.PHONY: data data-clean package
data:
	@bash scripts/mapc.sh

# `make package` adds the game's content to the title package built by `common/app.mk`; the
# game does not start without it.
package: title
	@test -d upstream/data || { echo "neverputt: no upstream/data - run make first" >&2; exit 1; }
	@if [ "$$(find upstream/data -name '*.sol' | wc -l)" -eq 0 ]; then \
	    echo "neverputt: no compiled courses - run 'make data' first" >&2; exit 1; fi
	@echo "neverputt: copying $$(du -sh upstream/data | cut -f1) of data into the package"
	@rm -rf build/title/$(TITLE_ID)/data
	@cp -r upstream/data build/title/$(TITLE_ID)/data
	@echo "neverputt: package is build/title/$(TITLE_ID) ($$(du -sh build/title/$(TITLE_ID) | cut -f1))"

data-clean:
	@find upstream/data -name '*.sol' -delete 2>/dev/null || true
	@rm -rf build/mapc
	@echo "neverputt: removed compiled courses"
