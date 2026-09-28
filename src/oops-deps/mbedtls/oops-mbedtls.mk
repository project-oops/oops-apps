# Mbed TLS build integration.
#
#   OOPS_MBEDTLS ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/mbedtls)
#   include $(OOPS_MBEDTLS)/oops-mbedtls.mk
#   EXTRA_TARGET_LDFLAGS += $(OOPS_MBEDTLS_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_MBEDTLS_LIB)
#
# Upstream's three libraries - crypto, X.509 and TLS - as one archive: every source under
# `library/`, which is what its own `library/Makefile` builds, plus `oops_platform.c`. The
# configuration is upstream's default with `oops_config.h` layered over it; a consumer
# compiling against the headers needs `OOPS_MBEDTLS_INCLUDE`, which carries that define too,
# or its view of the structures would differ from the library's.
#
# Two builds, as `../libcxx` has: a freestanding title compiles against oops-sdk's C library
# and `common/posix` (for `pthread`, the key store's stdio and `getentropy`), and a hosted one -
# `OOPS_RENDERER = mesa`, set before this is included - against the Mesa sysroot. The two
# disagree about `FILE` and `clock_t`, so they land in different directories.
ifndef OOPS_MBEDTLS_MK
OOPS_MBEDTLS_MK := 1

ifndef OOPS_MBEDTLS_DIR
OOPS_MBEDTLS_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_MBEDTLS_HOSTED ?= $(if $(filter mesa,$(OOPS_RENDERER))$(filter 1,$(USE_MESA)),1,0)
OOPS_MBEDTLS_UPSTREAM ?= $(OOPS_MBEDTLS_DIR)/upstream
ifeq ($(OOPS_MBEDTLS_HOSTED),1)
OOPS_MBEDTLS_BUILD ?= $(OOPS_MBEDTLS_DIR)/build-hosted
OOPS_MBEDTLS_SYS = --sysroot=$(OOPS_MESA_SYSROOT)
OOPS_MBEDTLS_THREADS := -DOOPS_MBEDTLS_THREADS=1
else
OOPS_MBEDTLS_BUILD ?= $(OOPS_MBEDTLS_DIR)/build
OOPS_MBEDTLS_SYS = -ffreestanding -fno-builtin -nostdlibinc $(OOPS_POSIX_INCLUDE) \
                   $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)
endif
OOPS_MBEDTLS_INCLUDE := -I$(OOPS_MBEDTLS_UPSTREAM)/include $(OOPS_MBEDTLS_THREADS) \
    '-DMBEDTLS_USER_CONFIG_FILE="$(OOPS_MBEDTLS_DIR)/oops_config.h"'
OOPS_MBEDTLS_LIB := $(OOPS_MBEDTLS_BUILD)/libmbedtls.a
OOPS_MBEDTLS_LDFLAGS := $(OOPS_MBEDTLS_LIB)
OOPS_MBEDTLS_SRCS := $(sort $(wildcard $(OOPS_MBEDTLS_UPSTREAM)/library/*.c)) \
                     $(OOPS_MBEDTLS_DIR)/oops_platform.c
OOPS_MBEDTLS_CFLAGS = -target x86_64-unknown-freebsd -nostdlib -fPIC -O2 -w \
                      -I$(OOPS_MBEDTLS_UPSTREAM)/library $(OOPS_MBEDTLS_INCLUDE) \
                      $(OOPS_MBEDTLS_SYS)

# The objects are numbered by position and `ar` is handed the list (see `common/deps.mk`).
$(OOPS_MBEDTLS_LIB): $(OOPS_MBEDTLS_SRCS) $(OOPS_MBEDTLS_DIR)/oops_config.h \
                     $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_MBEDTLS_BUILD)
	@rm -f $@
	@n=0; objs=""; for s in $(OOPS_MBEDTLS_SRCS); do n=$$((n+1)); o=$(OOPS_MBEDTLS_BUILD)/mt$$n.o; \
	   $(TARGET_CC) $(OOPS_MBEDTLS_CFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 echo "mbedtls: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs
	@echo "mbedtls: $@"

.PHONY: mbedtls-clean
mbedtls-clean:
	@rm -rf $(OOPS_MBEDTLS_BUILD)

endif
