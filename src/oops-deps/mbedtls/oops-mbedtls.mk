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
# Freestanding or hosted by the including title (`common/dep-sys.mk`). Freestanding, it needs
# `common/posix` for the key store's stdio and `getentropy`; hosted, it also takes the locks
# (`oops_config.h` says why only there).
ifndef OOPS_MBEDTLS_MK
OOPS_MBEDTLS_MK := 1

ifndef OOPS_MBEDTLS_DIR
OOPS_MBEDTLS_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
include $(OOPS_MBEDTLS_DIR)/../../../common/deps.mk
include $(OOPS_MBEDTLS_DIR)/../../../common/dep-sys.mk
OOPS_MBEDTLS_UPSTREAM ?= $(OOPS_MBEDTLS_DIR)/upstream
OOPS_MBEDTLS_BUILD ?= $(OOPS_MBEDTLS_DIR)/build$(OOPS_DEP_BUILD_SUFFIX)
OOPS_MBEDTLS_THREADS := $(if $(filter 1,$(OOPS_DEPS_HOSTED)),-DOOPS_MBEDTLS_THREADS=1)
OOPS_MBEDTLS_INCLUDE := -I$(OOPS_MBEDTLS_UPSTREAM)/include $(OOPS_MBEDTLS_THREADS) \
    '-DMBEDTLS_USER_CONFIG_FILE="$(OOPS_MBEDTLS_DIR)/oops_config.h"'
OOPS_MBEDTLS_LIB := $(OOPS_MBEDTLS_BUILD)/libmbedtls.a
OOPS_MBEDTLS_LDFLAGS := $(OOPS_MBEDTLS_LIB)
OOPS_MBEDTLS_SRCS := $(sort $(wildcard $(OOPS_MBEDTLS_UPSTREAM)/library/*.c)) \
                     $(OOPS_MBEDTLS_DIR)/oops_platform.c
OOPS_MBEDTLS_CFLAGS = -target x86_64-unknown-freebsd -nostdlib -fPIC -O2 -w \
                      -I$(OOPS_MBEDTLS_UPSTREAM)/library $(OOPS_MBEDTLS_INCLUDE) \
                      $(OOPS_DEP_SYS)

# One rule per object (`common/deps.mk`), so `make -j` compiles them in parallel and a change
# to `oops_config.h` rebuilds through the depfiles; `ar` reads the list from a file, which a
# long list of absolute paths needs on Windows.
OOPS_MBEDTLS_OBJS := $(call oops_objs,$(OOPS_MBEDTLS_BUILD)/obj,$(OOPS_MBEDTLS_SRCS))
$(call oops_ar_check,$(OOPS_MBEDTLS_OBJS))
-include $(OOPS_MBEDTLS_OBJS:.o=.d)
$(call oops_obj_rules,$(OOPS_MBEDTLS_BUILD)/obj,TARGET_CC,OOPS_MBEDTLS_CFLAGS,$(OOPS_MBEDTLS_SRCS))

$(OOPS_MBEDTLS_LIB): $(OOPS_MBEDTLS_OBJS)
	@rm -f $@
	@:$(call oops_rsp,$@.rsp,$(OOPS_MBEDTLS_OBJS))
	@a=$$(command -v $(AR) 2>/dev/null || command -v llvm-ar 2>/dev/null || command -v ar); \
	 "$$a" rcs $@ @$@.rsp
	@echo "mbedtls: $@ ($(words $(OOPS_MBEDTLS_OBJS)) objects)"

.PHONY: mbedtls-clean
mbedtls-clean:
	@rm -rf $(OOPS_MBEDTLS_BUILD)

endif
