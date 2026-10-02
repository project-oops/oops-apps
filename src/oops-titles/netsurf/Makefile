# NetSurf, through oops-sdk.
#
#   make title     the payload
#   make package   the payload with its resources, which is what runs
#   make census    compile every source for the target, set by set
#   make survey    the pin and the dependencies
#
# The browser (`upstream/netsurf-all-3.11/netsurf`) with its framebuffer front end
# (`frontends/framebuffer/`), which draws every pixel itself into a surface libnsfb presents.
# Nothing is proprietary, so the player supplies nothing.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# The browser makes no GL calls. libnsfb's `sdl` surface is SDL 1.2, which `sdl12-compat` puts on
# SDL2, whose OpenGL renderer presents it on fixed-function GL - as `../extreme-tux-racer` does.
OOPS_RENDERER = gl1

include $(OOPS_APPS_ROOT)/common/upstream.mk

# NetSurf's own libraries come in the release bundle (`upstream.lock`): `libwapcaplet`
# `libparserutils` `libhubbub` `libdom` `libcss` (the parsers), `libnsgif` `libnsbmp`
# `libsvgtiny` (images), `libnsutils` `libutf8proc`, and `libnsfb` (the surface). `libnslog` and
# `libnspsl` are optional and left out: the first needs flex and bison to build, and NetSurf
# logs through its own `utils/log.c` without it; the second is the public-suffix list, which
# only refines cookie scoping. JavaScript is off - `shim/sources.mk` says why. cURL fetches,
# and Mbed TLS under it is what makes most of the web load at all.
# Armed by default: the payload links with nothing unresolved.
NSRF_ARMED ?= 1
SCAFFOLD_ARM    := NSRF_ARMED
SCAFFOLD_DEPS   := sdl2 sdl12-compat freetype libpng libjpeg-turbo zlib curl mbedtls
SCAFFOLD_SUPPLY := nothing - the browser and its resources are free and ship in the package

OOPS_FEATURES := display input keyboard mouse audio suspend thread draw dialog net

include $(OOPS_APPS_ROOT)/common/posix.mk
OOPS_MBEDTLS ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/mbedtls)
include $(OOPS_MBEDTLS)/oops-mbedtls.mk
OOPS_ZLIB ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/zlib)
include $(OOPS_ZLIB)/oops-zlib.mk
OOPS_CURL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/curl)
include $(OOPS_CURL)/oops-curl.mk
OOPS_PNG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libpng)
include $(OOPS_PNG)/oops-libpng.mk
OOPS_JPEG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libjpeg-turbo)
include $(OOPS_JPEG)/oops-libjpeg.mk
OOPS_SDL12 ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl12-compat)
include $(OOPS_SDL12)/oops-sdl12.mk

# --- the sources ------------------------------------------------------------------------
#
# The browser's list comes from its own make fragments (`shim/sources.mk`), the libraries' from
# their `src/` trees less what this build does not select: libnsfb's X11, VNC and Wayland
# surfaces, and libdom's expat and libxml2 bindings (NetSurf parses HTML through hubbub).
NSRF_ALL := $(CURDIR)/upstream/netsurf-all-3.11
NS := $(NSRF_ALL)/netsurf
NSRF_GEN := $(CURDIR)/build/gen
NSRF_SRCS := $(addprefix $(NS)/,$(shell MAKEFLAGS= $(MAKE) -s --no-print-directory -C $(NS) \
                                   -f $(CURDIR)/shim/sources.mk print 2>/dev/null))
nsrf_lib = $(sort $(shell find $(NSRF_ALL)/$(1)/src -name '*.c' 2>/dev/null))
NSRF_LIBS := libwapcaplet libparserutils libhubbub libdom libcss libnsgif libnsbmp \
             libnsutils libutf8proc libnsfb
NSRF_LIB_libwapcaplet := $(call nsrf_lib,libwapcaplet)
NSRF_LIB_libparserutils := $(call nsrf_lib,libparserutils)
NSRF_LIB_libhubbub := $(call nsrf_lib,libhubbub)
NSRF_LIB_libdom := $(call nsrf_lib,libdom) $(NSRF_ALL)/libdom/bindings/hubbub/parser.c
NSRF_LIB_libnsgif := $(call nsrf_lib,libnsgif)
NSRF_LIB_libnsbmp := $(call nsrf_lib,libnsbmp)
NSRF_LIB_libnsutils := $(call nsrf_lib,libnsutils)
# `utf8proc_data.c` is the table `utf8proc.c` includes, not a source of its own.
NSRF_LIB_libutf8proc := $(NSRF_ALL)/libutf8proc/src/utf8proc.c
# libnsfb's own lists (`src/Makefile`, `src/plot/Makefile`, `src/surface/Makefile`) with the SDL
# surface: its `src/` also holds sources those lists do not name.
NSRF_LIB_libnsfb := $(addprefix $(NSRF_ALL)/libnsfb/src/, \
    libnsfb.c dump.c cursor.c palette.c \
    $(addprefix plot/,api.c util.c generic.c 32bpp-xrgb8888.c 32bpp-xbgr8888.c 16bpp.c 8bpp.c) \
    $(addprefix surface/,surface.c ram.c sdl.c))

# --- generated sources ------------------------------------------------------------------
#
# What each library's build generates, generated the same way where the tool is in the build
# image: hubbub's entity table and parserutils' charset aliases by their own Perl scripts, into
# their own trees as upstream writes them; libcss's per-property parsers by its own host C
# generator. The two gperf keyword tables go through `shim/gperf_lite.py`, which says why.
$(shell cd $(NSRF_ALL)/libhubbub && test -f src/tokeniser/entities.inc || \
        perl build/make-entities.pl >&2)
$(shell cd $(NSRF_ALL)/libparserutils && test -f src/charset/aliases.inc || \
        perl build/make-aliases.pl >&2)
$(shell mkdir -p $(NSRF_GEN)/hubbub/treebuilder && \
        python3 $(CURDIR)/shim/gperf_lite.py \
            $(NSRF_ALL)/libhubbub/src/treebuilder/element-type.gperf \
            $(NSRF_GEN)/hubbub/treebuilder/autogenerated-element-type.c >&2)

# libdom installs its hubbub binding's header as `dom/bindings/hubbub/parser.h`; in the tree it is
# `bindings/hubbub/parser.h`. The installed path is the one NetSurf includes.
$(shell mkdir -p $(NSRF_GEN)/include/dom/bindings/hubbub && \
        cmp -s $(NSRF_ALL)/libdom/bindings/hubbub/parser.h \
               $(NSRF_GEN)/include/dom/bindings/hubbub/parser.h || \
        cp $(NSRF_ALL)/libdom/bindings/hubbub/parser.h $(NSRF_GEN)/include/dom/bindings/hubbub/ && \
        cp $(NSRF_ALL)/libdom/bindings/hubbub/errors.h $(NSRF_GEN)/include/dom/bindings/hubbub/)

NSRF_CSS_PROPS_DIR := $(NSRF_ALL)/libcss/src/parse/properties
NSRF_CSS_PROPS := $(shell sed -n 's/^\([^\#][^:]*\):.*/\1/p' $(NSRF_CSS_PROPS_DIR)/properties.gen \
                    2>/dev/null)
NSRF_CSS_GEN := $(NSRF_GEN)/css
# Less the generator itself, which is a host program with its own `main`.
NSRF_LIB_libcss := $(filter-out %/css_property_parser_gen.c,$(call nsrf_lib,libcss)) \
                   $(addprefix $(NSRF_CSS_GEN)/autogenerated_,$(addsuffix .c,$(NSRF_CSS_PROPS)))
$(NSRF_CSS_GEN)/gen_parser: $(NSRF_CSS_PROPS_DIR)/css_property_parser_gen.c
	@mkdir -p $(@D)
	@clang -O1 -w -o $@ $<
$(NSRF_CSS_GEN)/autogenerated_%.c: $(NSRF_CSS_PROPS_DIR)/properties.gen $(NSRF_CSS_GEN)/gen_parser
	@$(NSRF_CSS_GEN)/gen_parser -o $@ "$$(grep "^$*:" $<)"
# Kept: they are sources, not intermediates to clean up after a census.
.SECONDARY: $(filter $(NSRF_CSS_GEN)/%,$(NSRF_LIB_libcss))

# The framebuffer front end's built-in images and font, as its `Makefile:85-168` generates them:
# each PNG under `res/` becomes a C bitmap through `tools/convert_image`, and the internal font's
# glyphs through `tools/convert_font`. Both are host programs; `convert_image` links libpng and
# zlib, built for the host here from the same vendored sources the payload uses.
NSRF_FB_RES := $(NS)/frontends/framebuffer/res
NSRF_FB_GEN := $(NSRF_GEN)/fb
NSRF_HOST := $(CURDIR)/build/host
NSRF_FB_IMAGES := $(shell sed -n 's/^FB_IMAGE_\([A-Za-z0-9_]*\) := \(.*\)/\1:\2/p' \
                    $(NS)/frontends/framebuffer/Makefile 2>/dev/null)
NSRF_FB_SRCS := $(foreach i,$(NSRF_FB_IMAGES),$(NSRF_FB_GEN)/image-$(firstword $(subst :, ,$(i))).c) \
                $(NSRF_FB_GEN)/font-ns-sans.c

$(NSRF_HOST)/convert_font: $(NS)/tools/convert_font.c
	@mkdir -p $(@D)
	@clang -O1 -w -o $@ $<
$(NSRF_HOST)/convert_image: $(NS)/tools/convert_image.c $(NS)/frontends/framebuffer/fbtk.h
	@mkdir -p $(@D)
	@clang -O1 -w -DPNG_NO_CONFIG_H $(OOPS_PNG_INCLUDE) -o $@ $< $(OOPS_PNG_SRCS) $(OOPS_ZLIB_SRCS) -lm

$(NSRF_FB_GEN)/font-ns-sans.c $(NSRF_FB_GEN)/font-ns-sans.h: $(NSRF_FB_RES)/fonts/glyph_data \
                                                              $(NSRF_HOST)/convert_font
	@mkdir -p $(@D)
	@$(NSRF_HOST)/convert_font -H $(NSRF_FB_GEN)/font-ns-sans.h $< $(NSRF_FB_GEN)/font-ns-sans.c

define nsrf_fb_image
$(NSRF_FB_GEN)/image-$(1).c: $(NSRF_FB_RES)/$(2) $(NSRF_HOST)/convert_image
	@mkdir -p $$(@D)
	@$(NSRF_HOST)/convert_image $$< $$@ $(1)
endef
$(foreach i,$(NSRF_FB_IMAGES),$(eval $(call nsrf_fb_image,$(firstword $(subst :, ,$(i))),$(lastword $(subst :, ,$(i))))))
.SECONDARY: $(NSRF_FB_SRCS) $(NSRF_FB_GEN)/font-ns-sans.h

# --- flags ------------------------------------------------------------------------------
# `__PROSPERO__` selects `patches/0001`'s arms, as SDL's include flags set it for its own.
# `_ALIGNED` and `STMTEXPR` are what the bundle's `buildsystem/makefiles/Makefile.clang` gives
# every library: without the first, `libcss`'s `} _ALIGNED;` declares a variable in each file.
NSRF_BASE := -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib -nostdlibinc \
             -fPIC -ftls-model=initial-exec -O2 -w -std=gnu99 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 \
             -D_BSD_SOURCE -D_DEFAULT_SOURCE -D__PROSPERO__=1 \
             '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1
NSRF_SYS = $(OOPS_POSIX_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)
NSRF_LIB_INC := $(foreach l,$(NSRF_LIBS),-I$(NSRF_ALL)/$(l)/include) \
                -I$(NSRF_ALL)/libutf8proc/include/libutf8proc -I$(NSRF_GEN)/include
nsrf_lib_flags = $(NSRF_BASE) -I$(NSRF_ALL)/$(1)/src $(NSRF_LIB_INC) $(NSRF_SYS)

# `Makefile.defaults` and `frontends/framebuffer/Makefile`'s definitions for these switches.
NSRF_DEFS := -Dnsframebuffer -Dsmall -DWITH_CURL -DWITH_BMP -DWITH_GIF -DWITH_PNG \
             -DWITH_JPEG -DWITH_UTF8PROC -DLIBICONV_PLUG \
             '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' \
             '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE \
             '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' \
             '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' \
             '-DNETSURF_FB_RESPATH="/app0/res"' '-DNETSURF_FB_FONTPATH="/app0/res/fonts"'
NSRF_FLAGS = $(NSRF_BASE) $(NSRF_DEFS) -I$(CURDIR)/shim/include -I$(NS) -I$(NS)/include \
             -I$(NS)/frontends -I$(NS)/content/handlers $(NSRF_LIB_INC) $(OOPS_CURL_INCLUDE) \
             $(OOPS_MBEDTLS_INCLUDE) $(OOPS_PNG_INCLUDE) $(OOPS_JPEG_INCLUDE) \
             $(OOPS_ZLIB_INCLUDE) $(OOPS_SDL12_INCLUDE) $(NSRF_SYS)

CENSUS_SETS := netsurf $(NSRF_LIBS) zlib_gz
# zlib's file functions, which NetSurf's `Messages` loader reads a gzip file through.
# `Z_HAVE_UNISTD_H` is what zlib's own configure defines where `<unistd.h>` exists.
CENSUS_zlib_gz_SRCS := $(OOPS_ZLIB_GZ_SRCS)
CENSUS_zlib_gz_FLAGS = $(NSRF_BASE) -DZ_HAVE_UNISTD_H $(OOPS_ZLIB_INCLUDE) $(NSRF_SYS)
CENSUS_netsurf_SRCS := $(NSRF_SRCS) $(NSRF_FB_SRCS)
CENSUS_netsurf_FLAGS = $(NSRF_FLAGS) -I$(NSRF_FB_GEN) -I$(NS)/frontends/framebuffer
$(foreach l,$(NSRF_LIBS),$(eval CENSUS_$(l)_SRCS := $$(NSRF_LIB_$(l))))
$(foreach l,$(NSRF_LIBS),$(eval CENSUS_$(l)_FLAGS = $$(call nsrf_lib_flags,$(l))))
CENSUS_libhubbub_FLAGS = $(call nsrf_lib_flags,libhubbub) -I$(NSRF_GEN)/hubbub
CENSUS_libnsfb_FLAGS = $(call nsrf_lib_flags,libnsfb) -DNSFB_SDL_AVAILABLE $(OOPS_SDL12_INCLUDE)

include $(OOPS_APPS_ROOT)/common/deps.mk

ifeq ($(NSRF_ARMED),1)
# --- the payload ------------------------------------------------------------------------
#
# The browser and its libraries in one archive, linked whole: libnsfb registers its surfaces
# from constructors, which a plain archive link would drop along with the object.
NSRF_BUILD := build/nsrf

define nsrf_objs
NSRF_$(1)_OBJS := $$(call oops_objs,$$(NSRF_BUILD)/$(1),$$(CENSUS_$(1)_SRCS))
$$(call oops_obj_rules,$$(NSRF_BUILD)/$(1),TARGET_CC,CENSUS_$(1)_FLAGS,$$(CENSUS_$(1)_SRCS))
-include $$(NSRF_$(1)_OBJS:.o=.d)
endef
$(foreach s,$(CENSUS_SETS),$(eval $(call nsrf_objs,$(s))))

NSRF_LIB := $(NSRF_BUILD)/libnetsurf.a
NSRF_OBJS := $(foreach s,$(CENSUS_SETS),$(NSRF_$(s)_OBJS))
$(NSRF_LIB): $(NSRF_OBJS)
	@rm -f $@
	@a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $(NSRF_OBJS)
	@echo "netsurf: $@ ($(words $(NSRF_OBJS)) objects)"
# The generated font header, before the one source that includes it.
$(call oops_obj,$(NSRF_BUILD)/netsurf,$(NS)/frontends/framebuffer/font_internal.c): \
    $(NSRF_FB_GEN)/font-ns-sans.h

EXTRA_TARGET_CFLAGS += $(OOPS_POSIX_INCLUDE) -I$(OOPS_APPS_ROOT)/common \
                       -DOOPS_POSIX_HOME=\"/app0\"
PAYLOAD_SRCS := $(CURDIR)/shim/nsrf_start.c $(OOPS_APPS_ROOT)/common/crashtrace.c \
                $(OOPS_POSIX_SRCS)
EXTRA_TARGET_LDFLAGS += -Wl,--whole-archive $(NSRF_LIB) -Wl,--no-whole-archive \
                        $(OOPS_CURL_LDFLAGS) $(OOPS_MBEDTLS_LDFLAGS) $(OOPS_PNG_LDFLAGS) \
                        $(OOPS_JPEG_LDFLAGS) $(OOPS_ZLIB_LDFLAGS) $(OOPS_SDL12_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(NSRF_LIB) $(OOPS_CURL_LIB) $(OOPS_MBEDTLS_LIB) $(OOPS_PNG_LIB) \
                        $(OOPS_JPEG_LIB) $(OOPS_ZLIB_LIB) $(OOPS_SDL12_LIB) $(OOPS_SDL12_SDL2_LIB)
endif


include $(OOPS_APPS_ROOT)/common/app.mk
include $(OOPS_APPS_ROOT)/common/scaffold.mk
include $(OOPS_APPS_ROOT)/common/census.mk

# The internal font's header is generated, and one source includes it.
$(CENSUS_DIR)/netsurf$(NS)/frontends/framebuffer/font_internal.c.chk: $(NSRF_FB_GEN)/font-ns-sans.h

# --- the resources ----------------------------------------------------------------------
#
# What the framebuffer front end's `install` target puts in its resource directory
# (`frontends/framebuffer/Makefile:200-206`): the files under `res/`, with the links there
# resolved, and `Messages`, which `tools/split-messages` writes gzip-compressed from
# `resources/FatMessages` - a host program linking zlib, built here from the vendored sources.
# Some 60 files, so they ship as they are rather than in a tar.
#
# Two additions of the port's own: `ca-bundle`, NetSurf's own certificate list from
# `resources/`, and a `Choices` file naming it, because the framebuffer front end never sets the
# `ca_bundle` option and cURL here has no system store to fall back on.
NSRF_PKG_RES = build/title/$(TITLE_ID)/res

$(NSRF_HOST)/split-messages: $(NS)/tools/split-messages.c
	@mkdir -p $(@D)
	@clang -O1 -w -I$(NS) $(OOPS_ZLIB_INCLUDE) -DZ_HAVE_UNISTD_H -o $@ $< \
	    $(OOPS_ZLIB_SRCS) $(OOPS_ZLIB_GZ_SRCS)

.PHONY: package
package: title $(NSRF_HOST)/split-messages
	@rm -rf $(NSRF_PKG_RES) && mkdir -p $(NSRF_PKG_RES)
	@cd $(NSRF_FB_RES) && for f in *; do \
	    [ "$$f" = Messages ] || cp -rL "$$f" $(CURDIR)/$(NSRF_PKG_RES)/; done
	@$(NSRF_HOST)/split-messages -l en -p any -f messages -o $(NSRF_PKG_RES)/Messages -z \
	    $(NS)/resources/FatMessages
	@cp $(NS)/resources/ca-bundle $(NSRF_PKG_RES)/ca-bundle
	@printf 'ca_bundle:/app0/res/ca-bundle\n' > $(NSRF_PKG_RES)/Choices
	@echo "netsurf: $$(find $(NSRF_PKG_RES) -type f | wc -l) resource files in $(NSRF_PKG_RES)"
