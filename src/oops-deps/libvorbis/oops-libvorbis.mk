# libvorbis and libvorbisfile. Include before `common/app.mk`; it pulls libogg in itself.
#
#   OOPS_VORBIS ?= $(abspath ../../oops-deps/libvorbis)
#   include $(OOPS_VORBIS)/oops-libvorbis.mk
#
# **The source list is upstream's `libvorbis_la_SOURCES`, not `lib/*.c`.** That glob was tried
# first and swept in `psytune.c` and `tone.c`, which are standalone tuning programs with their
# own `main()`, and `misc.c`, which is debug scaffolding. Upstream's Makefile.am already knows
# which files are the library; reading it is cheaper than discovering the difference from
# compile errors.
#
# `vorbisfile.c` is its own library upstream (`libvorbisfile_la_SOURCES`) and is what Neverball
# actually calls - `ov_open_callbacks`, `ov_read`. It is in the same archive here because a
# payload links one archive either way.
ifndef OOPS_VORBIS_DIR
OOPS_VORBIS_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_OGG ?= $(abspath $(OOPS_VORBIS_DIR)/../libogg)
include $(OOPS_OGG)/oops-libogg.mk
OOPS_VORBIS_UPSTREAM ?= $(OOPS_VORBIS_DIR)/upstream
OOPS_VORBIS_BUILD ?= $(OOPS_VORBIS_DIR)/build
OOPS_VORBIS_INCLUDE := -I$(OOPS_VORBIS_UPSTREAM)/include $(OOPS_OGG_INCLUDE)
OOPS_VORBIS_LIB := $(OOPS_VORBIS_BUILD)/libvorbis.a
OOPS_VORBIS_LDFLAGS := $(OOPS_VORBIS_LIB) $(OOPS_OGG_LDFLAGS)
OOPS_VORBIS_SRCS := $(addprefix $(OOPS_VORBIS_UPSTREAM)/lib/,mdct.c smallft.c block.c envelope.c \
    window.c lsp.c lpc.c analysis.c synthesis.c psy.c info.c floor1.c floor0.c res0.c mapping0.c \
    registry.c codebook.c sharedbook.c lookup.c bitrate.c vorbisfile.c)
OOPS_VORBIS_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                     -nostdlibinc -fPIC -O2 -w -I$(OOPS_VORBIS_UPSTREAM)/lib \
                     $(OOPS_VORBIS_INCLUDE) $(OOPS_POSIX_INCLUDE) \
                     $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)
$(OOPS_VORBIS_LIB): $(OOPS_VORBIS_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_VORBIS_BUILD)
	@rm -f $@
	@n=0; for s in $(OOPS_VORBIS_SRCS); do n=$$((n+1)); \
	   $(TARGET_CC) $(OOPS_VORBIS_CFLAGS) -c -o $(OOPS_VORBIS_BUILD)/v$$n.o "$$s" || exit 1; done; \
	 echo "libvorbis: compiled $$n sources"
	@a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $(OOPS_VORBIS_BUILD)/v*.o
	@echo "libvorbis: $@"
.PHONY: libvorbis-clean
libvorbis-clean:
	@rm -rf $(OOPS_VORBIS_BUILD)
