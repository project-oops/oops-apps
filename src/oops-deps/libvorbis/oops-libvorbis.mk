# libvorbis and libvorbisfile. Include before `common/app.mk`; it pulls libogg in itself.
#
#   OOPS_VORBIS ?= $(abspath ../../oops-deps/libvorbis)
#   include $(OOPS_VORBIS)/oops-libvorbis.mk
#
# The source list is upstream's `libvorbis_la_SOURCES` (`lib/Makefile.am`), not `lib/*.c`, which
# also holds the standalone programs `psytune.c` and `tone.c` and the debug helper `misc.c`.
#
# `vorbisfile.c` is its own library upstream (`libvorbisfile_la_SOURCES`) and is what Neverball
# calls; it shares this archive.
#
# Freestanding or hosted by the including title (`common/dep-sys.mk`).
ifndef OOPS_VORBIS_MK
OOPS_VORBIS_MK := 1

ifndef OOPS_VORBIS_DIR
OOPS_VORBIS_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
include $(OOPS_VORBIS_DIR)/../../../common/dep-sys.mk
OOPS_OGG ?= $(abspath $(OOPS_VORBIS_DIR)/../libogg)
include $(OOPS_OGG)/oops-libogg.mk
OOPS_VORBIS_UPSTREAM ?= $(OOPS_VORBIS_DIR)/upstream
OOPS_VORBIS_BUILD ?= $(OOPS_VORBIS_DIR)/build$(OOPS_DEP_BUILD_SUFFIX)
OOPS_VORBIS_INCLUDE := -I$(OOPS_VORBIS_UPSTREAM)/include $(OOPS_OGG_INCLUDE)
OOPS_VORBIS_LIB := $(OOPS_VORBIS_BUILD)/libvorbis.a
OOPS_VORBIS_LDFLAGS := $(OOPS_VORBIS_LIB) $(OOPS_OGG_LDFLAGS)
OOPS_VORBIS_SRCS := $(addprefix $(OOPS_VORBIS_UPSTREAM)/lib/,mdct.c smallft.c block.c envelope.c \
    window.c lsp.c lpc.c analysis.c synthesis.c psy.c info.c floor1.c floor0.c res0.c mapping0.c \
    registry.c codebook.c sharedbook.c lookup.c bitrate.c vorbisfile.c)
OOPS_VORBIS_CFLAGS = -target x86_64-unknown-freebsd -nostdlib -fPIC -O2 -w \
                     -I$(OOPS_VORBIS_UPSTREAM)/lib $(OOPS_VORBIS_INCLUDE) $(OOPS_DEP_SYS)
# `ar` is handed the object list rather than a directory glob (see `common/deps.mk`).
$(OOPS_VORBIS_LIB): $(OOPS_VORBIS_SRCS) $(OOPS_VORBIS_DIR)/oops-libvorbis.mk
	@mkdir -p $(OOPS_VORBIS_BUILD)
	@rm -f $@
	@n=0; objs=""; for s in $(OOPS_VORBIS_SRCS); do n=$$((n+1)); o=$(OOPS_VORBIS_BUILD)/v$$n.o; \
	   $(TARGET_CC) $(OOPS_VORBIS_CFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 echo "libvorbis: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs
	@echo "libvorbis: $@"
.PHONY: libvorbis-clean
libvorbis-clean:
	@rm -rf $(OOPS_VORBIS_BUILD)

endif
