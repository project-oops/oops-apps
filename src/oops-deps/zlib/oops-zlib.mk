# zlib build integration. Pulled in by `oops-libpng.mk`; a title rarely includes this directly.
#
#   OOPS_ZLIB ?= $(abspath ../../oops-deps/zlib)
#   include $(OOPS_ZLIB)/oops-zlib.mk
#
# `Z_SOLO` is zlib's switch for compression without file I/O: it removes the `gzopen`/`gzread`
# family, the only part that wants `<fcntl.h>`. Consumers use the deflate and inflate core only.
ifndef OOPS_ZLIB_DIR
OOPS_ZLIB_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_ZLIB_UPSTREAM ?= $(OOPS_ZLIB_DIR)/upstream
OOPS_ZLIB_BUILD ?= $(OOPS_ZLIB_DIR)/build
OOPS_ZLIB_INCLUDE := -I$(OOPS_ZLIB_UPSTREAM) -DZ_SOLO
OOPS_ZLIB_LIB := $(OOPS_ZLIB_BUILD)/libz.a
OOPS_ZLIB_LDFLAGS := $(OOPS_ZLIB_LIB)
OOPS_ZLIB_SRCS := $(addprefix $(OOPS_ZLIB_UPSTREAM)/,adler32.c crc32.c deflate.c inflate.c \
    inftrees.c inffast.c trees.c zutil.c compress.c uncompr.c infback.c)
OOPS_ZLIB_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                   -nostdlibinc -fPIC -O2 -w $(OOPS_ZLIB_INCLUDE) $(OOPS_POSIX_INCLUDE) \
                   $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)
# The objects are numbered by position and `ar` is handed the list, not a directory glob, so an
# object from a removed source never reaches the archive. See `common/deps.mk`.
$(OOPS_ZLIB_LIB): $(OOPS_ZLIB_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_ZLIB_BUILD)
	@rm -f $@
	@n=0; objs=""; for s in $(OOPS_ZLIB_SRCS); do n=$$((n+1)); o=$(OOPS_ZLIB_BUILD)/z$$n.o; \
	   $(TARGET_CC) $(OOPS_ZLIB_CFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 echo "zlib: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs
	@echo "zlib: $@"
.PHONY: zlib-clean
zlib-clean:
	@rm -rf $(OOPS_ZLIB_BUILD)
