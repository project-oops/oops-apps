# Opus build integration. Pulled in by `oops-opusfile.mk`; rarely included directly.
#
# The source list is upstream's three trees minus the parts that need a build system to select them:
# `src/opus_demo.c` and friends are programs, the `*_sse*.c` and `*_neon*.c` files are chosen by a
# configure check this has no equivalent of, and the fixed-point tree is an alternative to the
# floating-point one. `OPUS_BUILD` and the disables below stand in for the generated config header.
#
# The float API is selected by a name being *absent*: `#ifndef DISABLE_FLOAT_API` guards
# `FLOAT2INT16`, so `-DDISABLE_FLOAT_API=0` switches it off rather than on.
#
# Guarded as a whole, as libogg is, so that a title naming both this and `oops-opusfile.mk` does not
# define the same recipes twice.
ifndef OOPS_OPUS_MK
OOPS_OPUS_MK := 1

ifndef OOPS_OPUS_DIR
OOPS_OPUS_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_OPUS_UPSTREAM ?= $(OOPS_OPUS_DIR)/upstream
OOPS_OPUS_BUILD ?= $(OOPS_OPUS_DIR)/build
OOPS_OPUS_INCLUDE := -I$(OOPS_OPUS_DIR)/include -I$(OOPS_OPUS_UPSTREAM)/include
OOPS_OPUS_LIB := $(OOPS_OPUS_BUILD)/libopus.a
OOPS_OPUS_LDFLAGS := $(OOPS_OPUS_LIB)

# Programs, test harnesses and the architecture-specific kernels, which a configure check would
# pick. Everything else in the three trees is portable C.
OOPS_OPUS_EXCLUDE := %/opus_demo.c %/repacketizer_demo.c %/opus_compare.c %/trivial_example.c \
                     %_sse.c %_sse2.c %_sse4_1.c %_avx2.c %_neon_intr.c %_arm.c %_dump.c \
                     %/mlp_train.c %/dump_modes.c %/tansig_table.c

OOPS_OPUS_SRCS := $(filter-out $(OOPS_OPUS_EXCLUDE), \
    $(wildcard $(OOPS_OPUS_UPSTREAM)/src/*.c) \
    $(wildcard $(OOPS_OPUS_UPSTREAM)/celt/*.c) \
    $(wildcard $(OOPS_OPUS_UPSTREAM)/silk/*.c) \
    $(wildcard $(OOPS_OPUS_UPSTREAM)/silk/float/*.c))

OOPS_OPUS_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                   -nostdlibinc -fPIC -O2 -w -std=gnu11 \
                   -DOPUS_BUILD=1 -DUSE_ALLOCA=0 -DVAR_ARRAYS=1 \
                   -DOPUS_HAVE_RTCD=0 -DHAVE_LRINTF=1 -DHAVE_LRINT=1 \
                   -DPACKAGE_VERSION=\"1.5.2\" \
                   $(OOPS_OPUS_INCLUDE) \
                   -I$(OOPS_OPUS_UPSTREAM)/celt -I$(OOPS_OPUS_UPSTREAM)/silk \
                   -I$(OOPS_OPUS_UPSTREAM)/silk/float -I$(OOPS_OPUS_UPSTREAM) \
                   $(OOPS_POSIX_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

# `ar` is handed the object list rather than a directory glob (see `common/deps.mk`). Objects are
# numbered because the three trees share file names - `bands.c` is in celt, `tables.c` in silk.
$(OOPS_OPUS_LIB): $(OOPS_OPUS_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_OPUS_BUILD)
	@rm -f $@
	@n=0; objs=""; for s in $(OOPS_OPUS_SRCS); do n=$$((n+1)); o=$(OOPS_OPUS_BUILD)/o$$n.o; \
	   $(TARGET_CC) $(OOPS_OPUS_CFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 echo "opus: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs
	@echo "opus: $@"

.PHONY: opus-clean
opus-clean:
	@rm -rf $(OOPS_OPUS_BUILD)

endif
