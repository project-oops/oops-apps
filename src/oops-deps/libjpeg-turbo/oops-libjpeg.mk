# libjpeg-turbo build integration. Include before `common/app.mk`:
#
#   OOPS_JPEG ?= $(abspath ../../oops-deps/libjpeg-turbo)
#   include $(OOPS_JPEG)/oops-libjpeg.mk
#
# Both the decoder and the encoder: q3rally's `renderercommon/tr_image_jpg.c` holds `RE_SaveJPG`
# beside the loader. The lists are transcribed from upstream's `CMakeLists.txt:637`, because a
# missing precision wrapper does not fail the link - `jdmaster.c` dispatches on sample depth at
# run time. Arithmetic coding (`jcarith.c`, `jdarith.c`, `jaricom.c`) and SIMD are off.
#
# `include/jconfig.h` and `include/jconfigint.h` stand in for CMake's generated headers, and
# `include/jversion.h` is upstream's template with its one substitution made.
ifndef OOPS_JPEG_DIR
OOPS_JPEG_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_JPEG_UPSTREAM ?= $(OOPS_JPEG_DIR)/upstream
OOPS_JPEG_BUILD ?= $(OOPS_JPEG_DIR)/build
OOPS_JPEG_INCLUDE := -I$(OOPS_JPEG_DIR)/include -I$(OOPS_JPEG_UPSTREAM)/src
OOPS_JPEG_LIB := $(OOPS_JPEG_BUILD)/libjpeg.a
OOPS_JPEG_LDFLAGS := $(OOPS_JPEG_LIB)
# The precision-independent core, compiled once. `jpeg_nbits.c` is the bit-count table
# `jchuff` and `jcphuff` index.
OOPS_JPEG_SRCS := $(addsuffix .c,$(addprefix $(OOPS_JPEG_UPSTREAM)/src/, \
    jcomapi jerror jmemmgr jmemnobs \
    jdapimin jdatasrc jdhuff jdinput jdmarker jdmaster jdtrans jdphuff jdicc jdlhuff \
    jcapimin jdatadst jchuff jcicc jcinit jclhuff jcmarker jcmaster jcparam jcphuff jctrans \
    jfdctflt jpeg_nbits))

# The sources compiled once per sample precision. Each goes through a one-line wrapper that
# defines `BITS_IN_JSAMPLE` and includes the real source; `jmorecfg.h` renames every function
# (`jinit_1pass_quantizer` becomes `j12init_1pass_quantizer`) and `jdmaster.c` dispatches on
# precision at run time, so the 12-bit symbols exist even when only 8-bit JPEGs are opened.
# The rule below writes the wrappers exactly as CMake's `src/wrapper/template.c` does.
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
# `ar` is handed the object list rather than a directory glob (see `common/deps.mk`), so objects
# from a changed wrapper set are never collected.
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
