# FreeType build integration. Include from a title's Makefile before `common/app.mk`:
#
#   OOPS_FT ?= $(abspath ../../oops-deps/freetype)
#   include $(OOPS_FT)/oops-freetype.mk
#
#   EXTRA_TARGET_CFLAGS  += $(OOPS_FT_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_FT_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_FT_LIB)
#
# # Fifteen files, not two hundred
#
# FreeType builds every font format it knows: Type 1, CFF, CID, PCF, BDF, PFR, Windows FNT, plus
# validators and a cache. The titles here render TrueType, so this compiles the core, the
# `sfnt`/`truetype` driver pair, the two rasterisers and the autohinter - which is the set
# upstream's own docs call a minimal TrueType build.
#
# The list is not trimmed for size but for **surface**: a format nothing opens is still a parser
# reading untrusted bytes, and still something to rebuild on a bump.
#
# `gzip/ftgzip.c` is in because `ftstdlib.h` wires compression into the core's option defaults.
# It is the one entry that brings `setjmp` with it - see `oops-sdk/include/libc/setjmp.h`, which
# is declarations over the platform's, and note that nothing in a TrueType path calls it.

ifndef OOPS_FT_DIR
OOPS_FT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_FT_UPSTREAM ?= $(OOPS_FT_DIR)/upstream
OOPS_FT_BUILD ?= $(OOPS_FT_DIR)/build

OOPS_FT_INCLUDE := -I$(OOPS_FT_UPSTREAM)/include
OOPS_FT_LIB := $(OOPS_FT_BUILD)/libfreetype.a
OOPS_FT_LDFLAGS := $(OOPS_FT_LIB)

OOPS_FT_SRCS := \
    $(OOPS_FT_UPSTREAM)/src/base/ftsystem.c \
    $(OOPS_FT_UPSTREAM)/src/base/ftinit.c \
    $(OOPS_FT_UPSTREAM)/src/base/ftdebug.c \
    $(OOPS_FT_UPSTREAM)/src/base/ftbase.c \
    $(OOPS_FT_UPSTREAM)/src/base/ftbbox.c \
    $(OOPS_FT_UPSTREAM)/src/base/ftglyph.c \
    $(OOPS_FT_UPSTREAM)/src/base/ftbitmap.c \
    $(OOPS_FT_UPSTREAM)/src/sfnt/sfnt.c \
    $(OOPS_FT_UPSTREAM)/src/truetype/truetype.c \
    $(OOPS_FT_UPSTREAM)/src/psnames/psnames.c \
    $(OOPS_FT_UPSTREAM)/src/smooth/smooth.c \
    $(OOPS_FT_UPSTREAM)/src/raster/raster.c \
    $(OOPS_FT_UPSTREAM)/src/autofit/autofit.c \
    $(OOPS_FT_UPSTREAM)/src/pshinter/pshinter.c \
    $(OOPS_FT_UPSTREAM)/src/base/ftstroke.c \
    $(OOPS_FT_UPSTREAM)/src/base/ftmm.c

# **Three of these were added after a link said so**, which is the intended way for this list to
# grow:
#
#   * `base/ftstroke.c` - SDL2_ttf draws outlined text with `FT_Stroker_New` and its kin.
#   * `base/ftmm.c` - `FT_Set_Named_Instance`, the variable-font entry point sfnt calls.
#
# **`gzip/ftgzip.c` is not here, and removing it from this list was not enough.** It is the only
# thing in a font build that calls `setjmp`/`longjmp`, which the module packager refuses: the
# mined corpus does not say which library exports them, so a module cannot declare where to
# resolve them. Dropping the file only moved the undefined symbol, because `sfnt/sfwoff.c` and
# `sfnt/ttsvg.c` call `FT_Gzip_Uncompress` directly.
#
# `include/ftoption-oops.h` turns off `FT_CONFIG_OPTION_USE_ZLIB`, which removes the callers as
# well as the module. That costs WOFF fonts and gzip-compressed SVG glyphs, neither of which any
# title here ships.

# `-nostdlibinc` for the reason `oops-libcxx.mk` gives: without it the build machine's
# `/usr/include` stays on the path and a FreeBSD freestanding target compiles against glibc.
# `FT2_BUILD_LIBRARY` is FreeType's own switch for "this is the library, not a consumer".
# `FT_CONFIG_MODULES_H` points at our own module list - see `include/ftmodule-oops.h` for why
# the default one cannot be used with a trimmed source set.
OOPS_FT_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                 -nostdlibinc -fPIC -O2 -w -DFT2_BUILD_LIBRARY \
                 '-DFT_CONFIG_MODULES_H=<ftmodule-oops.h>' \
                 '-DFT_CONFIG_OPTIONS_H=<ftoption-oops.h>' \
                 -I$(OOPS_FT_DIR)/include \
                 $(OOPS_FT_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

$(OOPS_FT_LIB): $(OOPS_FT_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_FT_BUILD)
	@rm -f $@
	@n=0; for src in $(OOPS_FT_SRCS); do \
	    n=$$((n+1)); \
	    $(TARGET_CC) $(OOPS_FT_CFLAGS) -c -o $(OOPS_FT_BUILD)/ft$$n.o "$$src" || exit 1; \
	done; \
	echo "freetype: compiled $$n sources"
	@ar_tool=$$(command -v $(AR) 2>/dev/null || command -v llvm-ar 2>/dev/null || command -v ar); \
	 "$$ar_tool" rcs $@ $(OOPS_FT_BUILD)/ft*.o
	@echo "freetype: $@"

.PHONY: freetype-clean freetype-upstream freetype-upstream-clean
freetype-clean:
	@rm -rf $(OOPS_FT_BUILD)
	@echo "freetype: removed build/"

freetype-upstream:
	@$(OOPS_FT_DIR)/../../../common/upstream-fetch.sh \
	    "$$(sed -n 's/^UPSTREAM_KIND=//p' $(OOPS_FT_DIR)/upstream.lock)" \
	    "$$(sed -n 's/^UPSTREAM_URL=//p'  $(OOPS_FT_DIR)/upstream.lock)" \
	    "$$(sed -n 's/^UPSTREAM_REV=//p'  $(OOPS_FT_DIR)/upstream.lock)" \
	    "$(OOPS_FT_UPSTREAM)" "$(OOPS_FT_DIR)/patches"

freetype-upstream-clean:
	@rm -rf $(OOPS_FT_UPSTREAM)
	@echo "freetype: removed upstream/"
