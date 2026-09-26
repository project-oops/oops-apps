# libogg build integration. Pulled in by `oops-libvorbis.mk`; rarely included directly.
#
# `include/ogg/config_types.h` stands in for the header autotools generates.
#
# Guarded as a whole: both `oops-libvorbis.mk` and `oops-opusfile.mk` pull it in, and a title that
# names either ends up including this twice, which make reports as a recipe overriding itself.
ifndef OOPS_OGG_MK
OOPS_OGG_MK := 1

ifndef OOPS_OGG_DIR
OOPS_OGG_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_OGG_UPSTREAM ?= $(OOPS_OGG_DIR)/upstream
OOPS_OGG_BUILD ?= $(OOPS_OGG_DIR)/build
OOPS_OGG_INCLUDE := -I$(OOPS_OGG_UPSTREAM)/include -I$(OOPS_OGG_DIR)/include
OOPS_OGG_LIB := $(OOPS_OGG_BUILD)/libogg.a
OOPS_OGG_LDFLAGS := $(OOPS_OGG_LIB)
OOPS_OGG_SRCS := $(OOPS_OGG_UPSTREAM)/src/framing.c $(OOPS_OGG_UPSTREAM)/src/bitwise.c
OOPS_OGG_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                  -nostdlibinc -fPIC -O2 -w $(OOPS_OGG_INCLUDE) $(OOPS_POSIX_INCLUDE) \
                  $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)
# `ar` is handed the object list rather than a directory glob (see `common/deps.mk`).
$(OOPS_OGG_LIB): $(OOPS_OGG_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_OGG_BUILD)
	@rm -f $@
	@n=0; objs=""; for s in $(OOPS_OGG_SRCS); do n=$$((n+1)); o=$(OOPS_OGG_BUILD)/g$$n.o; \
	   $(TARGET_CC) $(OOPS_OGG_CFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 echo "libogg: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs
	@echo "libogg: $@"
.PHONY: libogg-clean
libogg-clean:
	@rm -rf $(OOPS_OGG_BUILD)

endif
