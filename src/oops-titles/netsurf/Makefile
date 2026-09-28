# NetSurf, through oops-sdk.
#
#   make           the survey - what stands between this title and a link
#   make title     the payload, packaged - once NSRF_ARMED=1 links
#
# The browser (`upstream/`) with its framebuffer front end (`frontends/framebuffer/`), which
# draws every pixel itself into a surface libnsfb presents. Nothing is proprietary, so the player
# supplies nothing.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# The browser makes no GL calls. libnsfb's `sdl` surface is SDL 1.2, which `sdl12-compat` puts on
# SDL2, whose OpenGL renderer presents it on fixed-function GL - as `../extreme-tux-racer` does.
OOPS_RENDERER = gl1

include $(OOPS_APPS_ROOT)/common/upstream.mk

# NetSurf's own libraries, each a separate repository, vendored as the port reaches them:
# `libwapcaplet` `libparserutils` `libhubbub` `libdom` `libcss` (the parsers), `libnsgif`
# `libnsbmp` `libsvgtiny` (images), `libnsutils` `libutf8proc` `libnspsl` `libnslog`, `libnsfb`
# (the surface), and `nsgenbind`, a host tool that generates the JavaScript bindings. The
# JavaScript engine, Duktape, is bundled in the tree and interprets; there is no JIT.
# cURL fetches, and a TLS library under it is what makes most of the web load at all.
SCAFFOLD_ARM    := NSRF_ARMED
SCAFFOLD_DEPS   := sdl2 sdl12-compat freetype libpng libjpeg-turbo zlib curl mbedtls \
                   libwapcaplet libparserutils libhubbub libdom libcss libnsgif libnsbmp \
                   libsvgtiny libnsutils libutf8proc libnspsl libnslog libnsfb
SCAFFOLD_SUPPLY := nothing - the browser and its resources are free and ship in the package

OOPS_FEATURES := display input keyboard mouse audio suspend thread draw dialog net

# Arming is where the browser's source list is transcribed from its Makefiles, with
# `NETSURF_FB_FRONTEND=sdl`, `NETSURF_FB_RESPATH=/app0/res/`, `shim/nsrf_start.c` and
# `common/crashtrace.c`, and a `package` target that copies `frontends/framebuffer/res`. Until
# then an armed build says so rather than linking a payload of two files.
ifeq ($(NSRF_ARMED),1)
$(error netsurf: the source lists are not transcribed yet - `make survey` says what is next)
endif

include $(OOPS_APPS_ROOT)/common/app.mk
include $(OOPS_APPS_ROOT)/common/scaffold.mk
