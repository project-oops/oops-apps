# Neverputt, through oops-sdk.
#
#   make elf      the plain-ELF payload a homebrew loader runs
#   make title    complete title package (.zip)
#   make data     compile the courses (needed once, takes minutes)
#   make package  title plus the data - the one to run
#
# **This is the second binary of the Neverball tree**, not a separate program: seven sources
# under `putt/` and a subset of the `share/` files Neverball builds. Read `../neverball/Makefile`
# alongside this - the dependency stack, the archive-under-upstream's-flags arrangement and the
# `mapc` data step are all the same, and the reasoning is written out there rather than repeated
# here. What differs is in `NP_PUTT` and `NP_SHARE` below, and it is upstream's own `PUTT_OBJS`.
#
# **The data has to be built before this will run.** Neverball ships `.map` sources and no
# `.sol`; `make data` compiles them with a host tool. A payload without that has no courses.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Fixed-function OpenGL, freestanding. `common/app.mk`'s front door for choosing a renderer; it
# adds oops-gl's sources itself, so this title does not list them.
OOPS_RENDERER = gl1

# The SDK sources this title compiles carry no `-O` of their own, and unoptimised oops-gl cost
# Neverball around 7.7us per draw call. Upstream's own archive already asks for -O2 below.
EXTRA_TARGET_CFLAGS += -O2

# --- the stack ------------------------------------------------------------
#
# The same eight dependencies Neverball pins, in the same order, for the same reasons: SDL2
# before its ttf, FreeType before ttf, zlib and libogg pulled in by libpng and libvorbis.
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
# and `app.mk` is read after it. Guarded, so including it twice costs nothing.
include $(OOPS_APPS_ROOT)/common/deps.mk

# --- flags ----------------------------------------------------------------
#
# `ENABLE_NLS=0` drops gettext. The filesystem is stdio rather than PhysicsFS, upstream's own
# supported switch, which is why `share/fs_stdio.c` is in the list below and `fs_physfs.c` is
# not.
#
# No `OOPS_POSIX_HOME`: `pick_home_path` (share/base_config.c:70) reads `getenv("HOME")` and
# falls back to `fs_base_dir()` without ever calling `getpwuid`, so the define that feeds
# `getpwuid` would only create an empty directory on the console. The write directory is
# `/app0/.neverball` - the same one Neverball uses, because `CONFIG_USER` is a property of the
# tree rather than of the binary.
NP_CFLAGS := -DENABLE_NLS=0 -DZ_SOLO \
             -Ishim/include -Iupstream/share \
             -DOOPS \
             $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE) $(OOPS_TTF_INCLUDE) $(OOPS_FT_INCLUDE) \
             $(OOPS_PNG_INCLUDE) $(OOPS_JPEG_INCLUDE) $(OOPS_VORBIS_INCLUDE)

EXTRA_TARGET_CFLAGS  += $(NP_CFLAGS)
EXTRA_TARGET_LDFLAGS += $(OOPS_SDL_LDFLAGS) $(OOPS_TTF_LDFLAGS) $(OOPS_FT_LDFLAGS) \
                        $(OOPS_PNG_LDFLAGS) $(OOPS_JPEG_LDFLAGS) $(OOPS_VORBIS_LDFLAGS)
# zlib and libogg are named although this title never mentions them: the `.mk` files above put
# their archives on the link line, and a flag is not a prerequisite - make will not build a file
# because a linker was told about it.
PAYLOAD_EXTRA_DEPS   += $(OOPS_SDL_LIB) $(OOPS_TTF_LIB) $(OOPS_FT_LIB) \
                        $(OOPS_PNG_LIB) $(OOPS_ZLIB_LIB) $(OOPS_JPEG_LIB) \
                        $(OOPS_VORBIS_LIB) $(OOPS_OGG_LIB)

# --- sources --------------------------------------------------------------
#
# **This is upstream's own `PUTT_OBJS`**, read from its Makefile rather than derived from
# Neverball's list or globbed. The differences from `BALL_OBJS` are real and would each have
# been a link error or a stowaway:
#
#   - no `tilt_null`: upstream adds a tilt backend to `BALL_OBJS` only (Makefile:349-357).
#     Neverputt has no tilt sensing at all.
#   - no `queue` and no `cmd`: those carry the client/server command stream that Neverball's
#     replay and networked-simulation split needs. Neverputt simulates in one place.
#   - `hmd_null` without `hmd_common`: the common half belongs to the openhmd and libovr arms
#     (Makefile:361-370), and the null arm of both binaries takes only the stub.
#
# A glob over `share/` would also sweep in `mapc.c`, which is a host tool with its own `main`,
# and `fs_physfs.c`, which the switches above exclude.
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
# Upstream's sources build into an archive rather than into `PAYLOAD_SRCS`, because this
# repository compiles with `-Werror -Wconversion -Wsign-conversion` and a 2010 C codebase does
# not survive that - and because every `-Wno-` added to silence upstream would also silence it
# for `shim/np_start.c`, which is ours. `../neverball/Makefile` has the full reasoning.
OOPS_NP_BUILD := build/np
OOPS_NP_LIB := $(OOPS_NP_BUILD)/libneverputt.a
# `-fcommon` is not optional: upstream declares globals like `text_input` in headers without
# `extern`, which GCC merged by default until version 10 and clang does not. Without it the link
# fails on a duplicate symbol in each file that includes `text.h`.
OOPS_NP_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                 -nostdlibinc -fPIC -O2 -w -std=c99 -fcommon $(NP_CFLAGS) \
                 $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

# `--whole-archive` because `app.mk` puts LDFLAGS before the sources on the link line, and
# because `main` itself lives in here - a plain archive seen too early contributes nothing.
EXTRA_TARGET_LDFLAGS += -Wl,--whole-archive $(OOPS_NP_LIB) -Wl,--no-whole-archive
PAYLOAD_EXTRA_DEPS   += $(OOPS_NP_LIB)

# Objects named after their sources, not numbered and globbed: a numbered scheme renumbers every
# object after an added or removed file, and leaves the dropped file's object behind for the glob
# to pick up - so its code stays in the archive and the link stays clean.
OOPS_NP_OBJS := $(call oops_objs,$(OOPS_NP_BUILD)/obj,$(NP_SRCS))
$(call oops_ar_check,$(OOPS_NP_OBJS))
-include $(OOPS_NP_OBJS:.o=.d)
$(call oops_obj_rules,$(OOPS_NP_BUILD)/obj,TARGET_CC,OOPS_NP_CFLAGS,$(NP_SRCS))

$(OOPS_NP_LIB): $(OOPS_NP_OBJS) $(oops_makefiles)
	@mkdir -p $(OOPS_NP_BUILD)
	@rm -f $@
	@a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $(OOPS_NP_OBJS)
	@echo "neverputt: $@ ($(words $(OOPS_NP_OBJS)) objects)"

# **The SDK sources are named, not requested by feature.** `OOPS_FEATURES` is a newer mechanism
# landing in `common/app.mk` in parallel with this title, and it is inert until the SDK ships the
# table it reads - so a Makefile written against it links here, where the working tree has that
# change, and fails in CI, where `app.mk` is the committed version. That is exactly what happened
# to the first version of this file: `make elf` was clean locally and the CI link listed thirty
# undefined symbols, `oops_thread_*`, `oops_sem_*`, `oops_system_*` and the rest.
#
# So this is the same explicit list `../neverball/Makefile` carries. When the feature table lands,
# both titles convert together and this comment goes with them.
PAYLOAD_SRCS = shim/np_start.c \
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
               $(OOPS_SDK_DIR)/src/system/suspend.c \
               $(OOPS_SDK_DIR)/src/system/freestd.c \
               $(OOPS_SDK_DIR)/src/system/syscall.c \
               $(OOPS_SDK_DIR)/src/system/sysmodule.c \
               $(OOPS_SDK_DIR)/src/thread/thread.c \
               $(OOPS_SDK_DIR)/src/draw/draw.c

ENTRY_POINT := np_start

include $(OOPS_APPS_ROOT)/common/app.mk

# --- the data -------------------------------------------------------------
#
# `mapc` is a host tool: it runs on the build machine, reads a `.map` and writes a `.sol` beside
# the rest of the data. It is built against the *same pinned* zlib, libpng and libjpeg sources
# the payload uses, because the build machine has no dev packages for those and reaching for them
# would put a dependency outside the lock files.
#
# **It compiles every `.map` in the tree, Neverball's levels included.** Splitting it by binary
# would mean teaching the script which courses belong to which title, and the tool costs minutes
# either way. The package step below is what selects.
.PHONY: data data-clean package
data:
	@bash scripts/mapc.sh

# **`make package` is the one to run**, not `make title`. `common/app.mk` builds a title
# directory out of the payload and `sce_sys/`; a port also ships the program's own content, and
# this game will not start without it.
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
