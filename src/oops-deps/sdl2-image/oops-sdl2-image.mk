# SDL2_image build integration. Include from a title's Makefile **after** `oops-sdl.mk`,
# `oops-libpng.mk` and `oops-libjpeg.mk`, before `common/app.mk`:
#
#   OOPS_IMG ?= $(abspath ../../oops-deps/sdl2-image)
#   include $(OOPS_IMG)/oops-sdl2-image.mk
#
#   EXTRA_TARGET_CFLAGS  += $(OOPS_IMG_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_IMG_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_IMG_LIB)
#
# It needs SDL2's headers, libpng's and libjpeg's, and includes none of those files itself - the
# same division `oops-sdl2-ttf.mk` draws next door, for the same reason: a title wanting images
# has already decided which SDL it uses.
#
# # PNG and JPEG
#
# `LOAD_PNG` and `LOAD_JPG` are the formats switched on, because those are the formats the titles
# have files in. Extreme Tux Racer calls `IMG_Load` against 149 PNGs; SuperTux loads 2736 PNGs and
# 24 JPEGs, and the JPEGs are its level backgrounds - so without `LOAD_JPG` every level would
# draw over black. Its `.tga` and `.bmp` paths are its own screenshot *writers* and never arrive
# here.
#
# **One archive for every title, so one set of formats.** This builds into `oops-deps/`, which
# titles share; a per-title switch would have each title rebuild it over the other's, and
# whichever built last would decide what the next one got. JPEG is on for all of them, which costs
# a title with no JPEGs the decoder's size and nothing else - `IMG_isJPG` reads four bytes of
# magic before libjpeg is ever called.
#
# That is `README.md`'s "trim for surface, not size" rather than an economy: AVIF, JXL, WEBP, TIFF,
# SVG, XCF and the rest are parsers of untrusted input, and a decoder no title has a file for is
# still something to carry across every bump.
#
# **The other loaders are still compiled, and that is not a contradiction.** `IMG.c`'s `supported[]`
# table names every format's `is`/`load` pair unconditionally, so each `IMG_<fmt>.c` has to be
# present to satisfy the link - each one compiles to a stub returning NULL when its own `LOAD_<FMT>`
# is absent. Dropping the files would not drop the references.
#
# **libpng rather than the bundled stb_image.** `IMG_png.c` picks its back end at the top: define
# `USE_STBIMAGE` and it takes the header-only decoder, define nothing and the `#else` selects
# `WANT_LIBPNG`. Nothing is defined here, so it is libpng - which is pinned next door, already
# compiles, and is the decoder Neverball is already shipping. Two copies of a PNG decoder in one
# title would be the waste that `README.md`'s "a duplicate is usually the right answer" is careful
# *not* to license.
#
# **`SDL_IMAGE_SAVE_PNG` and `SDL_IMAGE_SAVE_JPG` are 0.** Each defaults to 1 and would pull in an
# encoder - libpng's writer, libjpeg's compressor - for `IMG_SavePNG`/`IMG_SaveJPG`, which nothing
# calls. `oops-libjpeg.mk` builds the decoder only, so `SAVE_JPG` at 1 would link calls to
# functions that are not there.

ifndef OOPS_IMG_DIR
OOPS_IMG_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_IMG_UPSTREAM ?= $(OOPS_IMG_DIR)/upstream
OOPS_IMG_BUILD ?= $(OOPS_IMG_DIR)/build

OOPS_IMG_INCLUDE := -I$(OOPS_IMG_UPSTREAM)/include
OOPS_IMG_LIB := $(OOPS_IMG_BUILD)/libSDL2_image.a
# The decoders after it, because the loader calls into them - `app.mk` puts LDFLAGS before the
# objects, and a static archive seen before its callers contributes nothing.
OOPS_IMG_LDFLAGS := $(OOPS_IMG_LIB) $(OOPS_PNG_LDFLAGS) $(OOPS_JPEG_LDFLAGS)

# Upstream's own list, from `Makefile.am`'s `libSDL2_image_la_SOURCES`, less the two platform
# back ends: `IMG_WIC.c` is Windows Imaging Component and `IMG_ImageIO.m` is Objective-C.
OOPS_IMG_SRCS := $(addprefix $(OOPS_IMG_UPSTREAM)/src/,IMG.c IMG_avif.c IMG_bmp.c IMG_gif.c \
    IMG_jpg.c IMG_jxl.c IMG_lbm.c IMG_pcx.c IMG_png.c IMG_pnm.c IMG_qoi.c IMG_stb.c IMG_svg.c \
    IMG_tga.c IMG_tif.c IMG_webp.c IMG_xcf.c IMG_xpm.c IMG_xv.c)

OOPS_IMG_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                  -nostdlibinc -fPIC -O2 -w -D__PROSPERO__=1 \
                  -DLOAD_PNG=1 -DSDL_IMAGE_SAVE_PNG=0 -DLOAD_JPG=1 -DSDL_IMAGE_SAVE_JPG=0 \
                  $(OOPS_IMG_INCLUDE) $(OOPS_PNG_INCLUDE) $(OOPS_JPEG_INCLUDE) $(OOPS_SDL_INCLUDE) \
                  $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

# Objects are named after their sources rather than numbered by position - `common/cxx.mk` says
# what numbering costs when a list changes. `ar` is handed the list, never the directory.
$(OOPS_IMG_LIB): $(OOPS_IMG_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_IMG_BUILD)
	@rm -f $@
	@n=0; objs=""; for s in $(OOPS_IMG_SRCS); do \
	   o=$(OOPS_IMG_BUILD)/$$(basename "$$s" .c).o; n=$$((n+1)); \
	   $(TARGET_CC) $(OOPS_IMG_CFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 echo "sdl2-image: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v llvm-ar 2>/dev/null || command -v ar); \
	 "$$a" rcs $@ $$objs
	@echo "sdl2-image: $@"

.PHONY: sdl2-image-clean sdl2-image-upstream sdl2-image-upstream-clean
sdl2-image-clean:
	@rm -rf $(OOPS_IMG_BUILD)
	@echo "sdl2-image: removed build/"

sdl2-image-upstream:
	@$(OOPS_IMG_DIR)/../../../common/upstream-fetch.sh \
	    "$$(sed -n 's/^UPSTREAM_KIND=//p' $(OOPS_IMG_DIR)/upstream.lock)" \
	    "$$(sed -n 's/^UPSTREAM_URL=//p'  $(OOPS_IMG_DIR)/upstream.lock)" \
	    "$$(sed -n 's/^UPSTREAM_REV=//p'  $(OOPS_IMG_DIR)/upstream.lock)" \
	    "$(OOPS_IMG_UPSTREAM)" "$(OOPS_IMG_DIR)/patches"

sdl2-image-upstream-clean:
	@rm -rf $(OOPS_IMG_UPSTREAM)
	@echo "sdl2-image: removed upstream/"
