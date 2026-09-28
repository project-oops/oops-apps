# libtheora build integration: the decoder, `libtheoradec`.
#
#   OOPS_THEORA ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libtheora)
#   include $(OOPS_THEORA)/oops-libtheora.mk
#   EXTRA_TARGET_LDFLAGS += $(OOPS_THEORA_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_THEORA_LIB) $(OOPS_OGG_LIB)
#
# `lib/Makefile.am`'s `decoder_sources`: a game plays video and encodes none, so the encoder is
# not built. The x86 SIMD paths are chosen by a configure check this has no equivalent of, and
# are left out (`OC_X86_ASM` undefined), which selects the portable C ones. Theora reads its
# packets through libogg, which comes first.
ifndef OOPS_THEORA_MK
OOPS_THEORA_MK := 1

# This file's own directory first: `MAKEFILE_LIST` names libogg's once that is included.
ifndef OOPS_THEORA_DIR
OOPS_THEORA_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_OGG ?= $(abspath $(OOPS_THEORA_DIR)/../libogg)
include $(OOPS_OGG)/oops-libogg.mk
OOPS_THEORA_UPSTREAM ?= $(OOPS_THEORA_DIR)/upstream
OOPS_THEORA_BUILD ?= $(OOPS_THEORA_DIR)/build
OOPS_THEORA_INCLUDE := -I$(OOPS_THEORA_UPSTREAM)/include $(OOPS_OGG_INCLUDE)
OOPS_THEORA_LIB := $(OOPS_THEORA_BUILD)/libtheoradec.a
OOPS_THEORA_LDFLAGS := $(OOPS_THEORA_LIB) $(OOPS_OGG_LDFLAGS)
OOPS_THEORA_SRCS := $(addprefix $(OOPS_THEORA_UPSTREAM)/lib/, apiwrapper.c bitpack.c \
    decapiwrapper.c decinfo.c decode.c dequant.c fragment.c huffdec.c idct.c info.c \
    internal.c quant.c state.c)
OOPS_THEORA_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                     -nostdlibinc -fPIC -O2 -w $(OOPS_THEORA_INCLUDE) $(OOPS_POSIX_INCLUDE) \
                     $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

$(OOPS_THEORA_LIB): $(OOPS_THEORA_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_THEORA_BUILD)
	@rm -f $@
	@n=0; objs=""; for s in $(OOPS_THEORA_SRCS); do n=$$((n+1)); o=$(OOPS_THEORA_BUILD)/th$$n.o; \
	   $(TARGET_CC) $(OOPS_THEORA_CFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 echo "theora: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs
	@echo "theora: $@"

.PHONY: theora-clean
theora-clean:
	@rm -rf $(OOPS_THEORA_BUILD)

endif
