# libzip build integration.
#
#   OOPS_LIBZIP ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libzip)
#   include $(OOPS_LIBZIP)/oops-libzip.mk
#   EXTRA_TARGET_CFLAGS += $(OOPS_LIBZIP_INCLUDE)
#   EXTRA_LDFLAGS       += $(OOPS_LIBZIP_LDFLAGS)
#
# The other half of the archive-reading path: libultraship reads `.otr` through StormLib and
# `.o2r` through this. Needs zlib, so include `oops-zlib.mk` first.
ifndef OOPS_LIBZIP_DIR
OOPS_LIBZIP_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_LIBZIP_UPSTREAM ?= $(OOPS_LIBZIP_DIR)/upstream
OOPS_LIBZIP_BUILD ?= $(OOPS_LIBZIP_DIR)/build

# `include/config.h` and `include/zipconf.h` stand in for the headers CMake generates.
OOPS_LIBZIP_INCLUDE := -I$(OOPS_LIBZIP_UPSTREAM)/lib -I$(OOPS_LIBZIP_DIR)/include
OOPS_LIBZIP_LIB := $(OOPS_LIBZIP_BUILD)/libzip.a
OOPS_LIBZIP_LDFLAGS := $(OOPS_LIBZIP_LIB)

# The sources are `lib/` minus the `target_sources` branches of upstream's `lib/CMakeLists.txt`
# this platform does not take (`if(WIN32)`, `if(HAVE_LIBBZ2)` and so on). Each exclusion matches
# a line in `include/config.h`; they change together.
OOPS_LIBZIP_EXCLUDE := \
    zip_source_file_win32.c zip_source_file_win32_ansi.c zip_source_file_win32_named.c \
    zip_source_file_win32_utf16.c zip_source_file_win32_utf8.c \
    zip_random_win32.c zip_random_uwp.c zip_crypto_win.c \
    zip_crypto_commoncrypto.c zip_crypto_gnutls.c zip_crypto_mbedtls.c zip_crypto_openssl.c \
    zip_winzip_aes.c zip_source_winzip_aes_decode.c zip_source_winzip_aes_encode.c \
    zip_algorithm_bzip2.c zip_algorithm_xz.c zip_algorithm_zstd.c
OOPS_LIBZIP_ALL_SRCS := $(notdir $(wildcard $(OOPS_LIBZIP_UPSTREAM)/lib/*.c))
OOPS_LIBZIP_SRCS := $(addprefix $(OOPS_LIBZIP_UPSTREAM)/lib/,\
                      $(filter-out $(OOPS_LIBZIP_EXCLUDE),$(OOPS_LIBZIP_ALL_SRCS)))

# A bump that renames an excluded file would leave it in the build, so the counts at the pinned
# revision are checked.
ifneq ($(words $(OOPS_LIBZIP_ALL_SRCS)),131)
$(error libzip: upstream/lib has $(words $(OOPS_LIBZIP_ALL_SRCS)) sources, expected 131 - \
        re-check OOPS_LIBZIP_EXCLUDE against lib/CMakeLists.txt after this bump)
endif
ifneq ($(words $(OOPS_LIBZIP_SRCS)),113)
$(error libzip: building $(words $(OOPS_LIBZIP_SRCS)) sources, expected 113 - \
        a name in OOPS_LIBZIP_EXCLUDE no longer matches anything)
endif

# `zip_err_str.c` is generated at build time from the pinned headers (see `gen-err-str.sh`).
OOPS_LIBZIP_ERR_STR := $(OOPS_LIBZIP_BUILD)/zip_err_str.c

OOPS_LIBZIP_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                     -nostdlibinc -fPIC -O2 -w -DHAVE_CONFIG_H \
                     $(OOPS_LIBZIP_INCLUDE) $(OOPS_ZLIB_INCLUDE) \
                     $(OOPS_POSIX_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

$(OOPS_LIBZIP_ERR_STR): $(OOPS_LIBZIP_UPSTREAM)/lib/zip.h $(OOPS_LIBZIP_UPSTREAM)/lib/zipint.h \
                        $(OOPS_LIBZIP_DIR)/gen-err-str.sh
	@mkdir -p $(OOPS_LIBZIP_BUILD)
	@sh $(OOPS_LIBZIP_DIR)/gen-err-str.sh $(OOPS_LIBZIP_UPSTREAM) $@

$(OOPS_LIBZIP_LIB): $(OOPS_LIBZIP_SRCS) $(OOPS_LIBZIP_ERR_STR) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_LIBZIP_BUILD)
	@rm -f $@
	@n=0; objs=""; for s in $(OOPS_LIBZIP_SRCS) $(OOPS_LIBZIP_ERR_STR); do \
	   n=$$((n+1)); o=$(OOPS_LIBZIP_BUILD)/z$$n.o; \
	   $(TARGET_CC) $(OOPS_LIBZIP_CFLAGS) -std=gnu11 -c -o "$$o" "$$s" || exit 1; \
	   objs="$$objs $$o"; done; \
	 echo "libzip: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs
	@echo "libzip: $@"

.PHONY: libzip-clean
libzip-clean:
	@rm -rf $(OOPS_LIBZIP_BUILD)
