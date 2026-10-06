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

# The objects are numbered by position and `ar` is handed the list (see `common/deps.mk`).
$(OOPS_MBEDTLS_LIB): $(OOPS_MBEDTLS_SRCS) $(OOPS_MBEDTLS_DIR)/oops_config.h \
                     $(OOPS_MBEDTLS_DIR)/oops-mbedtls.mk
	@mkdir -p $(OOPS_MBEDTLS_BUILD)
	@rm -f $@
	@# The source list is read from a file: expanded inline it passes Windows' command-line
	@# limit and the shell gets a script cut off mid-`for` (`src/oops-deps/sdl2/oops-sdl.mk`
	@# says more). CRs are stripped because `read` keeps them.
	@:$(call oops_rsp,$(OOPS_MBEDTLS_BUILD)/sources.list,$(OOPS_MBEDTLS_SRCS))
	@tr -d '\r' < $(OOPS_MBEDTLS_BUILD)/sources.list > $(OOPS_MBEDTLS_BUILD)/sources.txt; \
	 : > $(OOPS_MBEDTLS_BUILD)/objects.list; \
	 n=0; while IFS= read -r s; do [ -n "$$s" ] || continue; n=$$((n+1)); o=$(OOPS_MBEDTLS_BUILD)/mt$$n.o; \
	   $(TARGET_CC) $(OOPS_MBEDTLS_CFLAGS) -c -o "$$o" "$$s" || exit 1; \
	   echo "$$o" >> $(OOPS_MBEDTLS_BUILD)/objects.list; \
	 done < $(OOPS_MBEDTLS_BUILD)/sources.txt; \
	 echo "mbedtls: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ @$(OOPS_MBEDTLS_BUILD)/objects.list
	@echo "mbedtls: $@"

.PHONY: mbedtls-clean
mbedtls-clean:
	@rm -rf $(OOPS_MBEDTLS_BUILD)

endif
