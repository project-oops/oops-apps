# SDL2_image build integration. Include from a title's Makefile after `oops-sdl.mk`,
# `oops-libpng.mk` and `oops-libjpeg.mk`, before `common/app.mk`:
#
#   OOPS_IMG ?= $(abspath ../../oops-deps/sdl2-image)
#   include $(OOPS_IMG)/oops-sdl2-image.mk
#
#   EXTRA_TARGET_CFLAGS  += $(OOPS_IMG_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_IMG_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_IMG_LIB)
#
# It needs SDL2's, libpng's and libjpeg's headers and includes none of those files itself: the
# title chooses its SDL.
#
# Only PNG and JPEG loading are on; the archive is shared by every title, so it carries one set
# of formats. Other decoders are parsers of untrusted input that no title has files for. Their
# `IMG_<fmt>.c` files are still compiled because `IMG.c`'s `supported[]` table references every
# format; each compiles to a stub returning NULL without its `LOAD_<FMT>`.
#
# `IMG_png.c` uses libpng when `USE_STBIMAGE` is undefined, so PNG shares the pinned libpng
# rather than a second decoder. The save paths are 0: `oops-libjpeg.mk` builds no encoder.

ifndef OOPS_IMG_DIR
OOPS_IMG_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_IMG_UPSTREAM ?= $(OOPS_IMG_DIR)/upstream
OOPS_IMG_BUILD ?= $(OOPS_IMG_DIR)/build

OOPS_IMG_INCLUDE := -I$(OOPS_IMG_UPSTREAM)/include
OOPS_IMG_LIB := $(OOPS_IMG_BUILD)/libSDL2_image.a
# The decoder archives follow the loader, which calls into them.
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

# Objects are named after their sources, and `ar` is handed the list rather than the directory.
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
