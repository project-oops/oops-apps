# FreeType build integration. Include from a title's Makefile before `common/app.mk`:
#
#   OOPS_FT ?= $(abspath ../../oops-deps/freetype)
#   include $(OOPS_FT)/oops-freetype.mk
#
#   EXTRA_TARGET_CFLAGS  += $(OOPS_FT_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_FT_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_FT_LIB)
#
# The titles here render TrueType, so this compiles the core, the `sfnt`/`truetype` driver
# pair, the two rasterisers and the autohinter - upstream's minimal TrueType build. A format
# nothing opens is still a parser reading untrusted bytes, so the rest stay out.

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

# `base/ftstroke.c` serves SDL2_ttf's outlined text (`FT_Stroker_New`); `base/ftmm.c` serves
# `FT_Set_Named_Instance`, the variable-font entry point sfnt calls. `gzip/ftgzip.c` is left
# out, and `include/ftoption-oops.h` turns off `FT_CONFIG_OPTION_USE_ZLIB` to remove its callers.

# `-nostdlibinc` keeps the build machine's `/usr/include` off the path (see `oops-libcxx.mk`).
# `FT2_BUILD_LIBRARY` is FreeType's switch for building the library rather than a consumer.
# `FT_CONFIG_MODULES_H` names the trimmed module list in `include/ftmodule-oops.h`.
OOPS_FT_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                 -nostdlibinc -fPIC -O2 -w -DFT2_BUILD_LIBRARY \
                 '-DFT_CONFIG_MODULES_H=<ftmodule-oops.h>' \
                 '-DFT_CONFIG_OPTIONS_H=<ftoption-oops.h>' \
                 -I$(OOPS_FT_DIR)/include \
                 $(OOPS_FT_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

# `ar` is handed the object list rather than a directory glob (see `common/deps.mk`).
$(OOPS_FT_LIB): $(OOPS_FT_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_FT_BUILD)
	@rm -f $@
	@n=0; objs=""; for src in $(OOPS_FT_SRCS); do \
	    n=$$((n+1)); o=$(OOPS_FT_BUILD)/ft$$n.o; \
	    $(TARGET_CC) $(OOPS_FT_CFLAGS) -c -o "$$o" "$$src" || exit 1; objs="$$objs $$o"; \
	done; \
	echo "freetype: compiled $$n sources"; \
	ar_tool=$$(command -v $(AR) 2>/dev/null || command -v llvm-ar 2>/dev/null || command -v ar); \
	"$$ar_tool" rcs $@ $$objs
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
