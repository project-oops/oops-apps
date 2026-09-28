# Monocypher build integration.
#
#   OOPS_MONOCYPHER ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/monocypher)
#   include $(OOPS_MONOCYPHER)/oops-monocypher.mk
#   EXTRA_TARGET_LDFLAGS += $(OOPS_MONOCYPHER_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_MONOCYPHER_LIB)
#
# The two sources libultraship's `cmake/dependencies/common.cmake` builds: the library and its
# optional Ed25519. Portable C with no calls out beyond `<stddef.h>` and `<stdint.h>`.
ifndef OOPS_MONOCYPHER_MK
OOPS_MONOCYPHER_MK := 1

ifndef OOPS_MONOCYPHER_DIR
OOPS_MONOCYPHER_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_MONOCYPHER_UPSTREAM ?= $(OOPS_MONOCYPHER_DIR)/upstream
OOPS_MONOCYPHER_BUILD ?= $(OOPS_MONOCYPHER_DIR)/build
OOPS_MONOCYPHER_INCLUDE := -I$(OOPS_MONOCYPHER_UPSTREAM)/src \
                           -I$(OOPS_MONOCYPHER_UPSTREAM)/src/optional
OOPS_MONOCYPHER_LIB := $(OOPS_MONOCYPHER_BUILD)/libmonocypher.a
OOPS_MONOCYPHER_LDFLAGS := $(OOPS_MONOCYPHER_LIB)
OOPS_MONOCYPHER_SRCS := $(OOPS_MONOCYPHER_UPSTREAM)/src/monocypher.c \
                        $(OOPS_MONOCYPHER_UPSTREAM)/src/optional/monocypher-ed25519.c
OOPS_MONOCYPHER_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                         -nostdlibinc -fPIC -O2 -w -std=c11 $(OOPS_MONOCYPHER_INCLUDE) \
                         $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

$(OOPS_MONOCYPHER_LIB): $(OOPS_MONOCYPHER_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_MONOCYPHER_BUILD)
	@rm -f $@
	@n=0; objs=""; for s in $(OOPS_MONOCYPHER_SRCS); do n=$$((n+1)); \
	   o=$(OOPS_MONOCYPHER_BUILD)/mc$$n.o; \
	   $(TARGET_CC) $(OOPS_MONOCYPHER_CFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; \
	 done; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs
	@echo "monocypher: $@"

.PHONY: monocypher-clean
monocypher-clean:
	@rm -rf $(OOPS_MONOCYPHER_BUILD)

endif
