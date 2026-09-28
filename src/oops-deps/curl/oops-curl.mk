# cURL build integration.
#
#   OOPS_CURL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/curl)
#   include $(OOPS_CURL)/oops-curl.mk      (after oops-mbedtls.mk and oops-zlib.mk)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_CURL_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_CURL_LIB)
#
# libcurl's `lib/Makefile.inc` `CSOURCES` less the QUIC and SSH backends, which this build does
# not enable: `lib/`, `vauth/`, `vdns/`, `vtls/`, `curlx/`, and `vquic/vquic.c`, which holds the
# "no HTTP/3 here" answers the HTTP code asks for. Every backend source guards its own contents,
# so the ones not selected compile to nothing. `include/curl_config.h` stands in for CMake's
# probe and says what is on.
#
# Two builds, as `../mbedtls` has, for the same reason: hosted (`OOPS_RENDERER = mesa`) against
# the Mesa sysroot, freestanding against oops-sdk and `common/posix`. A consumer includes
# `<curl/curl.h>` through `OOPS_CURL_INCLUDE`, which carries `CURL_STATICLIB`.
ifndef OOPS_CURL_MK
OOPS_CURL_MK := 1

ifndef OOPS_CURL_DIR
OOPS_CURL_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_CURL_HOSTED ?= $(if $(filter mesa,$(OOPS_RENDERER))$(filter 1,$(USE_MESA)),1,0)
OOPS_CURL_UPSTREAM ?= $(OOPS_CURL_DIR)/upstream
ifeq ($(OOPS_CURL_HOSTED),1)
OOPS_CURL_BUILD ?= $(OOPS_CURL_DIR)/build-hosted
OOPS_CURL_SYS = --sysroot=$(OOPS_MESA_SYSROOT) -DOOPS_CURL_HOSTED=1
else
OOPS_CURL_BUILD ?= $(OOPS_CURL_DIR)/build
OOPS_CURL_SYS = -ffreestanding -fno-builtin -nostdlibinc $(OOPS_POSIX_INCLUDE) \
                $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)
endif
OOPS_CURL_INCLUDE := -I$(OOPS_CURL_UPSTREAM)/include -DCURL_STATICLIB
OOPS_CURL_LIB := $(OOPS_CURL_BUILD)/libcurl.a
OOPS_CURL_LDFLAGS := $(OOPS_CURL_LIB)
OOPS_CURL_SRCS := $(sort $(wildcard $(OOPS_CURL_UPSTREAM)/lib/*.c \
                                    $(OOPS_CURL_UPSTREAM)/lib/vauth/*.c \
                                    $(OOPS_CURL_UPSTREAM)/lib/vdns/*.c \
                                    $(OOPS_CURL_UPSTREAM)/lib/vtls/*.c \
                                    $(OOPS_CURL_UPSTREAM)/lib/curlx/*.c \
                                    $(OOPS_CURL_UPSTREAM)/lib/vquic/vquic.c))
OOPS_CURL_CFLAGS = -target x86_64-unknown-freebsd -nostdlib -fPIC -O2 -w -std=gnu11 \
                   -DHAVE_CONFIG_H -DBUILDING_LIBCURL -I$(OOPS_CURL_DIR)/include \
                   $(OOPS_CURL_INCLUDE) -I$(OOPS_CURL_UPSTREAM)/lib \
                   $(OOPS_MBEDTLS_INCLUDE) $(OOPS_ZLIB_INCLUDE) $(OOPS_CURL_SYS)

# The objects are numbered by position and `ar` is handed the list (see `common/deps.mk`).
$(OOPS_CURL_LIB): $(OOPS_CURL_SRCS) $(OOPS_CURL_DIR)/include/curl_config.h \
                  $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_CURL_BUILD)
	@rm -f $@
	@n=0; objs=""; for s in $(OOPS_CURL_SRCS); do n=$$((n+1)); o=$(OOPS_CURL_BUILD)/cu$$n.o; \
	   $(TARGET_CC) $(OOPS_CURL_CFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 echo "curl: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs
	@echo "curl: $@"

.PHONY: curl-clean
curl-clean:
	@rm -rf $(OOPS_CURL_BUILD)

endif
