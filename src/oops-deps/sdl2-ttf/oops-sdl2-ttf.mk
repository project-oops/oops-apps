# SDL2_ttf build integration. Include from a title's Makefile **after** `oops-sdl.mk` and
# `oops-freetype.mk`, before `common/app.mk`:
#
#   OOPS_TTF ?= $(abspath ../../oops-deps/sdl2-ttf)
#   include $(OOPS_TTF)/oops-sdl2-ttf.mk
#
#   EXTRA_TARGET_CFLAGS  += $(OOPS_TTF_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_TTF_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_TTF_LIB)
#
# It needs SDL2's headers and FreeType's, and includes neither of those files itself - a title
# wanting text has already decided which SDL it uses, and FreeType stands alone for the title
# that wants it directly.
#
# # One source file
#
# SDL_ttf is a thin layer: `SDL_ttf.c` over FreeType, rendering glyphs into an `SDL_Surface`.
# `glfont.c` and `showfont.c` beside it are upstream's demos and are not built.
#
# That it is one file is the point rather than a curiosity - the expensive half of "a title wants
# text" is FreeType, which is pinned next door and compiles. A dependency that arrives cheaply
# usually means an earlier one was paid for properly.
#
# **HarfBuzz is off**, which is the default. It is a text shaper for scripts whose glyphs depend
# on their neighbours; Neverball's menus are Latin, and a shaper nothing needs is a large
# dependency and a large parser.

ifndef OOPS_TTF_DIR
OOPS_TTF_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_TTF_UPSTREAM ?= $(OOPS_TTF_DIR)/upstream
OOPS_TTF_BUILD ?= $(OOPS_TTF_DIR)/build

OOPS_TTF_INCLUDE := -I$(OOPS_TTF_UPSTREAM)
OOPS_TTF_LIB := $(OOPS_TTF_BUILD)/libSDL2_ttf.a
OOPS_TTF_LDFLAGS := $(OOPS_TTF_LIB)

OOPS_TTF_SRCS := $(OOPS_TTF_UPSTREAM)/SDL_ttf.c

OOPS_TTF_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                  -nostdlibinc -fPIC -O2 -w -D__PROSPERO__=1 \
                  $(OOPS_TTF_INCLUDE) $(OOPS_FT_INCLUDE) $(OOPS_SDL_INCLUDE) \
                  $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

$(OOPS_TTF_LIB): $(OOPS_TTF_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_TTF_BUILD)
	@rm -f $@
	$(TARGET_CC) $(OOPS_TTF_CFLAGS) -c -o $(OOPS_TTF_BUILD)/SDL_ttf.o $(OOPS_TTF_SRCS)
	@ar_tool=$$(command -v $(AR) 2>/dev/null || command -v llvm-ar 2>/dev/null || command -v ar); \
	 "$$ar_tool" rcs $@ $(OOPS_TTF_BUILD)/SDL_ttf.o
	@echo "sdl2-ttf: $@"

.PHONY: sdl2-ttf-clean sdl2-ttf-upstream sdl2-ttf-upstream-clean
sdl2-ttf-clean:
	@rm -rf $(OOPS_TTF_BUILD)
	@echo "sdl2-ttf: removed build/"

sdl2-ttf-upstream:
	@$(OOPS_TTF_DIR)/../../../common/upstream-fetch.sh \
	    "$$(sed -n 's/^UPSTREAM_KIND=//p' $(OOPS_TTF_DIR)/upstream.lock)" \
	    "$$(sed -n 's/^UPSTREAM_URL=//p'  $(OOPS_TTF_DIR)/upstream.lock)" \
	    "$$(sed -n 's/^UPSTREAM_REV=//p'  $(OOPS_TTF_DIR)/upstream.lock)" \
	    "$(OOPS_TTF_UPSTREAM)" "$(OOPS_TTF_DIR)/patches"

sdl2-ttf-upstream-clean:
	@rm -rf $(OOPS_TTF_UPSTREAM)
	@echo "sdl2-ttf: removed upstream/"
