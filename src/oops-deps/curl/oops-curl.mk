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
# Freestanding or hosted by the including title (`common/dep-sys.mk`); `curl_config.h` reads
# `OOPS_CURL_HOSTED` for the few calls only the hosted C library answers. A consumer includes
# `<curl/curl.h>` through `OOPS_CURL_INCLUDE`, which carries `CURL_STATICLIB`.
ifndef OOPS_CURL_MK
OOPS_CURL_MK := 1

ifndef OOPS_CURL_DIR
OOPS_CURL_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
include $(OOPS_CURL_DIR)/../../../common/deps.mk
include $(OOPS_CURL_DIR)/../../../common/dep-sys.mk
OOPS_CURL_UPSTREAM ?= $(OOPS_CURL_DIR)/upstream
OOPS_CURL_BUILD ?= $(OOPS_CURL_DIR)/build$(OOPS_DEP_BUILD_SUFFIX)
OOPS_CURL_SYS = $(OOPS_DEP_SYS) $(if $(filter 1,$(OOPS_DEPS_HOSTED)),-DOOPS_CURL_HOSTED=1)
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

# One rule per object (`common/deps.mk`), so `make -j` compiles them in parallel and a header
# change rebuilds only its users; `ar` reads the list from a file, which a long list of
# absolute paths needs on Windows.
OOPS_CURL_OBJS := $(call oops_objs,$(OOPS_CURL_BUILD)/obj,$(OOPS_CURL_SRCS))
$(call oops_ar_check,$(OOPS_CURL_OBJS))
-include $(OOPS_CURL_OBJS:.o=.d)
$(call oops_obj_rules,$(OOPS_CURL_BUILD)/obj,TARGET_CC,OOPS_CURL_CFLAGS,$(OOPS_CURL_SRCS))

$(OOPS_CURL_LIB): $(OOPS_CURL_OBJS)
	@rm -f $@
	@:$(call oops_rsp,$@.rsp,$(OOPS_CURL_OBJS))
	@a=$$(command -v $(AR) 2>/dev/null || command -v llvm-ar 2>/dev/null || command -v ar); \
	 "$$a" rcs $@ @$@.rsp
	@echo "curl: $@ ($(words $(OOPS_CURL_OBJS)) objects)"

.PHONY: curl-clean
curl-clean:
	@rm -rf $(OOPS_CURL_BUILD)

endif
