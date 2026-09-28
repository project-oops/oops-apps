# zlib build integration. Pulled in by `oops-libpng.mk`; a title rarely includes this directly.
#
#   OOPS_ZLIB ?= $(abspath ../../oops-deps/zlib)
#   include $(OOPS_ZLIB)/oops-zlib.mk
#
# The deflate and inflate core, the one-shot `compress`/`uncompress` on top of it (LÖVE's
# `love.data.compress` and its PNG and EXR paths call them), and zlib's default allocator over
# `malloc`. The `gzopen`/`gzread` file family is not in the archive - it is the only part that
# wants `<fcntl.h>` and `<unistd.h>` - but `OOPS_ZLIB_GZ_SRCS` names it for a title that links
# `common/posix` and compiles them with its own flags (NetSurf reads its `Messages` through it).
#
# `Z_SOLO` used to be defined for every consumer. It hides the file family, but it also removes
# the default allocator and the one-shot functions built on it, which is more than was meant.
# A consumer that still defines it (Neverball does, in its own flags) sees fewer declarations
# over the same `z_stream`, which is harmless.
ifndef OOPS_ZLIB_DIR
OOPS_ZLIB_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_ZLIB_UPSTREAM ?= $(OOPS_ZLIB_DIR)/upstream
OOPS_ZLIB_BUILD ?= $(OOPS_ZLIB_DIR)/build
OOPS_ZLIB_INCLUDE := -I$(OOPS_ZLIB_UPSTREAM)
OOPS_ZLIB_LIB := $(OOPS_ZLIB_BUILD)/libz.a
OOPS_ZLIB_LDFLAGS := $(OOPS_ZLIB_LIB)
OOPS_ZLIB_SRCS := $(addprefix $(OOPS_ZLIB_UPSTREAM)/,adler32.c crc32.c deflate.c inflate.c \
    inftrees.c inffast.c trees.c zutil.c compress.c uncompr.c infback.c)
OOPS_ZLIB_GZ_SRCS := $(addprefix $(OOPS_ZLIB_UPSTREAM)/,gzclose.c gzlib.c gzread.c gzwrite.c)
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
