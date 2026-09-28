# libpng build integration. Include before `common/app.mk`; it pulls zlib in itself.
#
#   OOPS_PNG ?= $(abspath ../../oops-deps/libpng)
#   include $(OOPS_PNG)/oops-libpng.mk
#
# `include/pnglibconf.h` is upstream's prebuilt config with `PNG_CONVERT_tIME_SUPPORTED` off:
# it is the only caller of `gmtime`, and `oops-sdk` has no calendar.
#
# Freestanding or hosted by the including title (`common/dep-sys.mk`).
ifndef OOPS_PNG_MK
OOPS_PNG_MK := 1

ifndef OOPS_PNG_DIR
OOPS_PNG_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
include $(OOPS_PNG_DIR)/../../../common/dep-sys.mk
OOPS_ZLIB ?= $(abspath $(OOPS_PNG_DIR)/../zlib)
include $(OOPS_ZLIB)/oops-zlib.mk
OOPS_PNG_UPSTREAM ?= $(OOPS_PNG_DIR)/upstream
OOPS_PNG_BUILD ?= $(OOPS_PNG_DIR)/build$(OOPS_DEP_BUILD_SUFFIX)
OOPS_PNG_INCLUDE := -I$(OOPS_PNG_UPSTREAM) -I$(OOPS_PNG_DIR)/include $(OOPS_ZLIB_INCLUDE)
OOPS_PNG_LIB := $(OOPS_PNG_BUILD)/libpng.a
OOPS_PNG_LDFLAGS := $(OOPS_PNG_LIB) $(OOPS_ZLIB_LDFLAGS)
OOPS_PNG_SRCS := $(addprefix $(OOPS_PNG_UPSTREAM)/,png.c pngerror.c pngget.c pngmem.c pngpread.c \
    pngread.c pngrio.c pngrtran.c pngrutil.c pngset.c pngtrans.c pngwio.c pngwrite.c pngwtran.c \
    pngwutil.c)
OOPS_PNG_CFLAGS = -target x86_64-unknown-freebsd -nostdlib -fPIC -O2 -w $(OOPS_PNG_INCLUDE) \
                  $(OOPS_DEP_SYS)
# `ar` is handed the object list rather than a directory glob (see `common/deps.mk`).
$(OOPS_PNG_LIB): $(OOPS_PNG_SRCS) $(OOPS_PNG_DIR)/oops-libpng.mk
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

endif
