# opusfile build integration. Include it from a title's Makefile:
#
#   OOPS_OPUSFILE ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/opusfile)
#   include $(OOPS_OPUSFILE)/oops-opusfile.mk
#   EXTRA_TARGET_CFLAGS  += $(OOPS_OPUSFILE_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_OPUSFILE_LDFLAGS)
#
# It pulls in opus and libogg, which it reads its streams out of, so a consumer names only this one.
ifndef OOPS_OPUSFILE_MK
OOPS_OPUSFILE_MK := 1

OOPS_OPUSFILE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
OOPS_OPUS ?= $(abspath $(OOPS_OPUSFILE_DIR)/../opus)
include $(OOPS_OPUS)/oops-opus.mk
OOPS_OGG ?= $(abspath $(OOPS_OPUSFILE_DIR)/../libogg)
include $(OOPS_OGG)/oops-libogg.mk

OOPS_OPUSFILE_UPSTREAM ?= $(OOPS_OPUSFILE_DIR)/upstream
OOPS_OPUSFILE_BUILD ?= $(OOPS_OPUSFILE_DIR)/build
OOPS_OPUSFILE_INCLUDE := -I$(OOPS_OPUSFILE_UPSTREAM)/include \
                         $(OOPS_OPUS_INCLUDE) $(OOPS_OGG_INCLUDE)
OOPS_OPUSFILE_LIB := $(OOPS_OPUSFILE_BUILD)/libopusfile.a
OOPS_OPUSFILE_LDFLAGS := $(OOPS_OPUSFILE_LIB) $(OOPS_OPUS_LDFLAGS) $(OOPS_OGG_LDFLAGS)

# `http.c` and `wincerts.c` are the network and Windows certificate halves: OP_DISABLE_HTTP drops
# the first, and the second is Windows only. What remains reads a stream a caller supplies.
OOPS_OPUSFILE_SRCS := $(OOPS_OPUSFILE_UPSTREAM)/src/info.c \
                      $(OOPS_OPUSFILE_UPSTREAM)/src/internal.c \
                      $(OOPS_OPUSFILE_UPSTREAM)/src/opusfile.c \
                      $(OOPS_OPUSFILE_UPSTREAM)/src/stream.c

OOPS_OPUSFILE_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                       -nostdlibinc -fPIC -O2 -w -std=gnu11 \
                       -DOP_DISABLE_HTTP=1 -DOP_DISABLE_FLOAT_API=0 \
                       $(OOPS_OPUSFILE_INCLUDE) $(OOPS_POSIX_INCLUDE) \
                       $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

$(OOPS_OPUSFILE_LIB): $(OOPS_OPUSFILE_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_OPUSFILE_BUILD)
	@rm -f $@
	@n=0; objs=""; for s in $(OOPS_OPUSFILE_SRCS); do n=$$((n+1)); \
	   o=$(OOPS_OPUSFILE_BUILD)/f$$n.o; \
	   $(TARGET_CC) $(OOPS_OPUSFILE_CFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 echo "opusfile: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs
	@echo "opusfile: $@"

.PHONY: opusfile-clean
opusfile-clean:
	@rm -rf $(OOPS_OPUSFILE_BUILD)

endif
