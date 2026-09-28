# zstd build integration.
#
#   OOPS_ZSTD ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/zstd)
#   include $(OOPS_ZSTD)/oops-zstd.mk
#   EXTRA_TARGET_LDFLAGS += $(OOPS_ZSTD_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_ZSTD_LIB)
#
# The library's three portable trees - common, compress, decompress - which is upstream's own
# `lib/libzstd.mk` without the dictionary builder, the legacy-format readers or the deprecated
# API. Two switches stand in for what its build would detect:
#
# - `ZSTD_DISABLE_ASM`: the decoder's `huf_decompress_amd64.S` is assembled only by a build that
#   can, and without it the C loop in `huf_decompress.c` is the one used.
# - No `ZSTD_MULTITHREAD`: compression runs on the caller's thread, so the library needs no
#   thread pool of its own.
ifndef OOPS_ZSTD_MK
OOPS_ZSTD_MK := 1

ifndef OOPS_ZSTD_DIR
OOPS_ZSTD_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_ZSTD_UPSTREAM ?= $(OOPS_ZSTD_DIR)/upstream
OOPS_ZSTD_BUILD ?= $(OOPS_ZSTD_DIR)/build
OOPS_ZSTD_INCLUDE := -I$(OOPS_ZSTD_UPSTREAM)/lib
OOPS_ZSTD_LIB := $(OOPS_ZSTD_BUILD)/libzstd.a
OOPS_ZSTD_LDFLAGS := $(OOPS_ZSTD_LIB)
OOPS_ZSTD_SRCS := $(wildcard $(OOPS_ZSTD_UPSTREAM)/lib/common/*.c \
                             $(OOPS_ZSTD_UPSTREAM)/lib/compress/*.c \
                             $(OOPS_ZSTD_UPSTREAM)/lib/decompress/*.c)
OOPS_ZSTD_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                   -nostdlibinc -fPIC -O2 -w -DZSTD_DISABLE_ASM=1 -DXXH_NAMESPACE=ZSTD_ \
                   $(OOPS_ZSTD_INCLUDE) $(OOPS_POSIX_INCLUDE) \
                   $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

# The objects are numbered by position and `ar` is handed the list (see `common/deps.mk`).
$(OOPS_ZSTD_LIB): $(OOPS_ZSTD_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_ZSTD_BUILD)
	@rm -f $@
	@n=0; objs=""; for s in $(OOPS_ZSTD_SRCS); do n=$$((n+1)); o=$(OOPS_ZSTD_BUILD)/zs$$n.o; \
	   $(TARGET_CC) $(OOPS_ZSTD_CFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 echo "zstd: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs
	@echo "zstd: $@"

.PHONY: zstd-clean
zstd-clean:
	@rm -rf $(OOPS_ZSTD_BUILD)

endif
