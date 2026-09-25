# libgfxd build integration - a decoder for N64 display lists (F3D and its dialects).
#
#   OOPS_GFXD ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libgfxd)
#   include $(OOPS_GFXD)/oops-libgfxd.mk
#   EXTRA_TARGET_CFLAGS  += $(OOPS_GFXD_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_GFXD_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_GFXD_LIB)
#
# **This is upstream's own `OBJ`, not a glob**, and the difference is not cosmetic. `uc.c`,
# `uc_argfn.c`, `uc_argtbl.c`, `uc_macrofn.c` and `uc_macrotbl.c` are **not translation units** -
# `uc.c:8-10` includes three of them and each `uc_f3d*.c` includes `uc.c` with a different
# microcode selected. A `uc*.c` wildcard compiles them standalone and gets `unknown type name
# 'UCFUNC'` five files in, which is what happened here before the Makefile was read.
#
# The five that are real are one per microcode dialect - F3D, F3DB, F3DEX, F3DEXB, F3DEX2 - and
# all are built because libultraship's display-list viewer lets the user choose; a dialect left
# out is a decoder that prints nothing for it.
#
# It needed `<inttypes.h>`, which `-ffreestanding` does not supply and this SDK did not have -
# `PRIu32` and its family are now in `oops-sdk/include/libc/inttypes.h` - and `read`/`write` on a
# descriptor, now in `common/posix/`. Both are general rather than anything to do with this
# library, which is the usual shape: a dependency's first build is mostly finding what the C
# library still owes.
ifndef OOPS_GFXD_DIR
OOPS_GFXD_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_GFXD_UPSTREAM ?= $(OOPS_GFXD_DIR)/upstream
OOPS_GFXD_BUILD ?= $(OOPS_GFXD_DIR)/build
OOPS_GFXD_INCLUDE := -I$(OOPS_GFXD_UPSTREAM)
OOPS_GFXD_LIB := $(OOPS_GFXD_BUILD)/libgfxd.a
OOPS_GFXD_LDFLAGS := $(OOPS_GFXD_LIB)
OOPS_GFXD_SRCS := $(addprefix $(OOPS_GFXD_UPSTREAM)/,gfxd.c uc_f3d.c uc_f3db.c uc_f3dex.c \
                                                    uc_f3dexb.c uc_f3dex2.c)
OOPS_GFXD_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                   -nostdlibinc -fPIC -O2 -w $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE) \
                   $(OOPS_POSIX_INCLUDE) $(OOPS_GFXD_INCLUDE)

# Objects named after their sources rather than numbered, for the reason `common/deps.mk` gives:
# a numbered scheme renumbers everything after an added file and leaves the dropped one behind.
$(OOPS_GFXD_LIB): $(OOPS_GFXD_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_GFXD_BUILD)
	@rm -f $@
	@objs=""; for s in $(OOPS_GFXD_SRCS); do \
	   o=$(OOPS_GFXD_BUILD)/$$(basename $$s .c).o; \
	   $(TARGET_CC) $(OOPS_GFXD_CFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs; \
	 echo "libgfxd: $@ ($(words $(OOPS_GFXD_SRCS)) objects)"

.PHONY: libgfxd-clean
libgfxd-clean:
	@rm -rf $(OOPS_GFXD_BUILD)
