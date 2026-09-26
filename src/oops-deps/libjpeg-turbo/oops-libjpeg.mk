# libjpeg-turbo build integration. Include before `common/app.mk`:
#
#   OOPS_JPEG ?= $(abspath ../../oops-deps/libjpeg-turbo)
#   include $(OOPS_JPEG)/oops-libjpeg.mk
#
# # Both halves now, and the encoder arrived because something asked
#
# It was the decode half alone until 2026-09-26 - Neverball reads JPEG textures and writes none -
# with a note that the encoder was a second body of code to add when a title wanted it. q3rally
# wants it: `renderercommon/tr_image_jpg.c` holds `RE_SaveJPG` beside the loader, so a title that
# takes screenshots links `jpeg_CreateCompress` and its six companions out of the same file that
# decodes its textures. **There was no version of this that stubbed out cleanly** - a stub would be
# a screenshot key that silently writes nothing.
#
# The lists below are transcribed from upstream's `CMakeLists.txt:637` rather than assembled from
# what a link happened to be missing, because getting a *precision wrapper* wrong does not fail the
# link - `jdmaster.c` dispatches on the image's sample depth at run time, so a missing 12-bit
# encoder is a fault when somebody opens a 12-bit JPEG and nothing before.
#
# Arithmetic coding stays off on both sides, so `jcarith.c` joins `jdarith.c` and `jaricom.c` in
# being absent. SIMD stays off; see below.
#
# **Arithmetic coding is off**, so `jdarith.c` and `jaricom.c` are not in the list - their error
# codes are not compiled either, which is how leaving them in first showed up.
#
# **SIMD is off.** The turbo in the name is per-architecture assembly built through NASM; the C
# path decodes the same images. A title that measures JPEG decode as its bottleneck can revisit
# that with a number in hand.
#
# `include/jconfig.h` and `include/jconfigint.h` are ours - CMake generates them upstream - and
# `include/jversion.h` is upstream's template with its one substitution made.
ifndef OOPS_JPEG_DIR
OOPS_JPEG_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_JPEG_UPSTREAM ?= $(OOPS_JPEG_DIR)/upstream
OOPS_JPEG_BUILD ?= $(OOPS_JPEG_DIR)/build
OOPS_JPEG_INCLUDE := -I$(OOPS_JPEG_DIR)/include -I$(OOPS_JPEG_UPSTREAM)/src
OOPS_JPEG_LIB := $(OOPS_JPEG_BUILD)/libjpeg.a
OOPS_JPEG_LDFLAGS := $(OOPS_JPEG_LIB)
# **The precision-independent core.** Everything here compiles once.
#
# `jpeg_nbits.c` is on the encode side only - it is the bit-count table `jchuff` and `jcphuff` index
# - which is why a decode-only build never needed it.
OOPS_JPEG_SRCS := $(addsuffix .c,$(addprefix $(OOPS_JPEG_UPSTREAM)/src/, \
    jcomapi jerror jmemmgr jmemnobs \
    jdapimin jdatasrc jdhuff jdinput jdmarker jdmaster jdtrans jdphuff jdicc jdlhuff \
    jcapimin jdatadst jchuff jcicc jcinit jclhuff jcmarker jcmaster jcparam jcphuff jctrans \
    jfdctflt jpeg_nbits))

# **And the sources that compile once per sample precision**, which is the part a plain file
# list gets wrong.
#
# libjpeg-turbo 3.x supports 8-, 12- and 16-bit samples by compiling a dozen files several times
# over, through one-line wrappers that `#define BITS_IN_JSAMPLE` and include the real source.
# `jmorecfg.h` then renames every function - `jinit_1pass_quantizer` becomes
# `j12init_1pass_quantizer` - and `jdmaster.c` dispatches on the image's precision at run time.
# So the 12-bit symbols must exist even for a build that only ever opens 8-bit JPEGs.
#
# CMake generates those wrappers from `src/wrapper/template.c`. We do not run its CMake, so the
# rule below writes them - two lines each, exactly what the template produces. Compiling only the
# base files gave 27 clean compiles and a link that failed on 18 undefined `j12*` symbols.
OOPS_JPEG_WRAP_3 := jdapistd jdcolor jddiffct jdlossls jdmainct jdpostct jdsample jutils \
                    jcapistd jccolor jcdiffct jclossls jcmainct jcprepct jcsample
OOPS_JPEG_WRAP_2 := jdcoefct jddctmgr jdmerge jidctflt jidctfst jidctint jidctred jquant1 jquant2 \
                    jccoefct jcdctmgr jfdctfst jfdctint
OOPS_JPEG_WRAPDIR := $(OOPS_JPEG_BUILD)/wrapper
OOPS_JPEG_WRAP_SRCS := \
    $(foreach f,$(OOPS_JPEG_WRAP_3),$(foreach b,8 12 16,$(OOPS_JPEG_WRAPDIR)/$(f)-$(b).c)) \
    $(foreach f,$(OOPS_JPEG_WRAP_2),$(foreach b,8 12,$(OOPS_JPEG_WRAPDIR)/$(f)-$(b).c))
OOPS_JPEG_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                   -nostdlibinc -fPIC -O2 -w $(OOPS_JPEG_INCLUDE) $(OOPS_POSIX_INCLUDE) \
                   $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)
# `ar` is handed the list rather than the directory - `common/deps.mk` says what the glob cost.
# It mattered most here of all the vendored archives, because half of what goes in is
# *generated* below: change the precision wrapper sets and the old wrappers' objects were still
# sitting in the directory for `j*.o` to collect.
$(OOPS_JPEG_LIB): $(OOPS_JPEG_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_JPEG_WRAPDIR)
	@rm -f $@
	@for f in $(OOPS_JPEG_WRAP_3); do for b in 8 12 16; do \
	   printf '#define BITS_IN_JSAMPLE %s\n#include "%s/src/%s.c"\n' \
	     "$$b" "$(OOPS_JPEG_UPSTREAM)" "$$f" > $(OOPS_JPEG_WRAPDIR)/$$f-$$b.c; done; done
	@for f in $(OOPS_JPEG_WRAP_2); do for b in 8 12; do \
	   printf '#define BITS_IN_JSAMPLE %s\n#include "%s/src/%s.c"\n' \
	     "$$b" "$(OOPS_JPEG_UPSTREAM)" "$$f" > $(OOPS_JPEG_WRAPDIR)/$$f-$$b.c; done; done
	@echo "libjpeg-turbo: generated $$(ls $(OOPS_JPEG_WRAPDIR)/*.c | wc -l) precision wrappers"
	@n=0; objs=""; for s in $(OOPS_JPEG_SRCS) $(OOPS_JPEG_WRAP_SRCS); do n=$$((n+1)); \
	   o=$(OOPS_JPEG_BUILD)/j$$n.o; \
	   $(TARGET_CC) $(OOPS_JPEG_CFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 echo "libjpeg-turbo: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs
	@echo "libjpeg-turbo: $@"
.PHONY: libjpeg-clean
libjpeg-clean:
	@rm -rf $(OOPS_JPEG_BUILD)
