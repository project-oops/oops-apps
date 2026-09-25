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

# `include/` is ours: `config.h` and `zipconf.h` are what CMake would generate, written out
# because there is no host here to probe. Each answer in them is a property of this collection's
# own libc, which is in this repository and can be read rather than tested for.
OOPS_LIBZIP_INCLUDE := -I$(OOPS_LIBZIP_UPSTREAM)/lib -I$(OOPS_LIBZIP_DIR)/include
OOPS_LIBZIP_LIB := $(OOPS_LIBZIP_BUILD)/libzip.a
OOPS_LIBZIP_LDFLAGS := $(OOPS_LIBZIP_LIB)

# **Named by what is left out, because that is the shorter and the more honest list.** Upstream's
# `lib/CMakeLists.txt` names 111 sources directly and adds the rest through `target_sources` under
# `if(WIN32)`, `if(HAVE_LIBBZ2)` and so on - so "the sources" is not a list anywhere, it is the
# directory minus the branches this platform does not take. Writing the exclusions down says which
# branches those are; a transcribed list of 113 filenames would not.
#
# Each exclusion is a line in `config.h` too. If one of those answers changes, both move together.
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

# The guard on the exclusion above. An upstream bump that renames a file leaves it *in* the build
# rather than out - a Windows source compiling on this target would at least fail loudly, but a
# dropped crypto backend would quietly start being compiled against headers that are not there.
# These are the counts at the pinned revision.
ifneq ($(words $(OOPS_LIBZIP_ALL_SRCS)),131)
$(error libzip: upstream/lib has $(words $(OOPS_LIBZIP_ALL_SRCS)) sources, expected 131 - \
        re-check OOPS_LIBZIP_EXCLUDE against lib/CMakeLists.txt after this bump)
endif
ifneq ($(words $(OOPS_LIBZIP_SRCS)),113)
$(error libzip: building $(words $(OOPS_LIBZIP_SRCS)) sources, expected 113 - \
        a name in OOPS_LIBZIP_EXCLUDE no longer matches anything)
endif

# `zip_err_str.c` is generated, and generated at build time rather than checked in: it pairs an
# error *number* with its message, and the numbers live in a header this repository does not own.
# `gen-err-str.sh` says what an upstream bump would otherwise do silently.
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
