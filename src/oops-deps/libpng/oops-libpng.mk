# libpng build integration. Include before `common/app.mk`; it pulls zlib in itself, because
# libpng does not build without it and a title should not have to know that.
#
#   OOPS_PNG ?= $(abspath ../../oops-deps/libpng)
#   include $(OOPS_PNG)/oops-libpng.mk
#
# `include/pnglibconf.h` is **upstream's own prebuilt config** with one line changed - see the
# comment on `PNG_CONVERT_tIME_SUPPORTED` in it, which is off because it is the only caller of
# `gmtime` and `oops-sdk` deliberately has no calendar.
ifndef OOPS_PNG_DIR
OOPS_PNG_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_ZLIB ?= $(abspath $(OOPS_PNG_DIR)/../zlib)
include $(OOPS_ZLIB)/oops-zlib.mk
OOPS_PNG_UPSTREAM ?= $(OOPS_PNG_DIR)/upstream
OOPS_PNG_BUILD ?= $(OOPS_PNG_DIR)/build
OOPS_PNG_INCLUDE := -I$(OOPS_PNG_UPSTREAM) -I$(OOPS_PNG_DIR)/include $(OOPS_ZLIB_INCLUDE)
OOPS_PNG_LIB := $(OOPS_PNG_BUILD)/libpng.a
OOPS_PNG_LDFLAGS := $(OOPS_PNG_LIB) $(OOPS_ZLIB_LDFLAGS)
OOPS_PNG_SRCS := $(addprefix $(OOPS_PNG_UPSTREAM)/,png.c pngerror.c pngget.c pngmem.c pngpread.c \
    pngread.c pngrio.c pngrtran.c pngrutil.c pngset.c pngtrans.c pngwio.c pngwrite.c pngwtran.c \
    pngwutil.c)
OOPS_PNG_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                  -nostdlibinc -fPIC -O2 -w $(OOPS_PNG_INCLUDE) $(OOPS_POSIX_INCLUDE) \
                  $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)
# `ar` is handed the list rather than the directory - `common/deps.mk` says what the glob cost.
$(OOPS_PNG_LIB): $(OOPS_PNG_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_PNG_BUILD)
	@rm -f $@
	@n=0; objs=""; for s in $(OOPS_PNG_SRCS); do n=$$((n+1)); o=$(OOPS_PNG_BUILD)/p$$n.o; \
	   $(TARGET_CC) $(OOPS_PNG_CFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 echo "libpng: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs
	@echo "libpng: $@"
.PHONY: libpng-clean
libpng-clean:
	@rm -rf $(OOPS_PNG_BUILD)
