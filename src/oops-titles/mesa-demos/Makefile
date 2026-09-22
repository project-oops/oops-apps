# mesa-demos - upstream Mesa's own feature demos, built against upstream Mesa.
#
# The bring-up slot from `src/oops-titles/README.md`. One demo per build:
#
#   make title              # the default, `gears`
#   make title DEMO=engine  # any other name from upstream/src/demos/
#
# # Why one demo per build rather than a menu
#
# A menu would be our code, and every line of ours between the console and the demo is a line
# that can be blamed when something does not draw. The value of this title is that the program is
# *not ours*: if `gears` is wrong here and right on a desktop, the difference is oops-mesa. A
# picker would put a frame around that, and the thing being measured is the picture inside it.
#
# It also keeps the title honest about what it is. `DEMO=` in the build is a measurement
# parameter, the way a probe's arm is, and the title id stays the same because the answer being
# recorded is "does this console run mesa-demos", not "does it run gears".

OOPS_APPS_ROOT ?= $(abspath ../../..)

# **This is the line the whole title exists for.** `oops-sdk`'s GLUT names no backend - it goes
# through `oops/gfx.h` - so this one word is the difference between these demos running on
# oops-gl's own OpenGL and on upstream Mesa with radeonsi and ACO. (oops-mesa#D013, oops-sdk#D012)
OOPS_RENDERER = mesa

DEMO ?= gears

DEMO_SRC := upstream/src/demos/$(DEMO).c

# `src/util` is header-only for the simpler demos: `glut_wrap.h` is an `#include <GL/glut.h>` with
# two `#ifdef`s around it, and it finds oops-sdk's GLUT header without knowing anything about it.
# Demos that need `readtex.c`, `shaderutil.c` or `matrix.c` will fail to link and name what is
# missing, and this list grows from that rather than from `ls src/util` - the same discipline
# `src/oops-deps/libcxx/oops-libcxx.mk` states for its four-of-forty-five source list.
#
# `shim/include` comes first so `glad/glad.h` resolves to ours. mesa-demos 9.0.0 loads GL through
# glad, which 27 of the 56 demos include; that header is a loader this static link does not need,
# but its *reporting* half is real and is answered from the driver. See the header for why those
# flags are queried rather than defined to 1.
MESA_DEMOS_CFLAGS := -Ishim/include -Iupstream/src/util -Iupstream/src

# The demo's name, for the two log lines the shim prints. Stringified here rather than in the
# shim because the shim does not know which demo it was built with.
MESA_DEMOS_CFLAGS += -DOOPS_DEMOS_NAME=\"$(DEMO)\"

# Where the demos look for their textures and models. Upstream defaults this to `../data/`
# relative to the build tree (`upstream/meson.build:30`) and the demos concatenate it directly -
# `#define TEXTURE_FILE DEMOS_DATA_DIR "reflect.rgb"` - so the trailing slash is load-bearing.
#
# `/app0/` is the title's own directory on the console. **The files are not there yet**: they
# live in `upstream/src/data` (14 files, 2.1 MB) and `make stage-data` copies them into the
# packaged title. Defining this makes the eight data-using demos *compile*; it does not make
# them find anything at run time until that step has run, and the README says so rather than
# letting a build success imply an asset.
MESA_DEMOS_DATA_DIR ?= /app0/data/
MESA_DEMOS_CFLAGS += -DDEMOS_DATA_DIR=\"$(MESA_DEMOS_DATA_DIR)\"

# The proportional GLUT font names, mapped to the face oops-sdk has. Force-included rather than
# patched into `glut_wrap.h`, because `upstream/` is fetched output and is never edited - the
# patch rule in `src/oops-titles/README.md` says a thing that can be answered from outside the
# tree must be. The header explains why the mapping lives in this title and not in the SDK.
MESA_DEMOS_CFLAGS += -include $(abspath shim/include/glut_proportional_fonts.h)

# Upstream's warnings are upstream's. These are the same suppressions Craft's port carries, for
# the same reason: this repository's `-Wall -Wextra -Werror` is a statement about code we write,
# and turning it on somebody else's twenty-year-old demo produces a patch queue that says nothing
# about the console. `-Wno-missing-prototypes` in particular is unavoidable - a demo's `main` and
# its callbacks are declared nowhere because nothing else calls them.
MESA_DEMOS_WARNINGS := -Wno-strict-prototypes -Wno-missing-prototypes -Wno-conversion \
                       -Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-comment \
                       -Wno-unused-parameter -Wno-unused-but-set-variable \
                       -Wno-shadow -Wno-sign-compare -Wno-char-subscripts \
                       -Wno-string-concatenation -Wno-cast-function-type-mismatch \
                       -Wno-missing-variable-declarations -Wno-double-promotion

EXTRA_TARGET_CFLAGS += $(MESA_DEMOS_CFLAGS) $(MESA_DEMOS_WARNINGS)

# The SDK this title needs, listed rather than linked from an archive - the arrangement every
# hosted title here uses, and `OOPS_RENDERER = mesa` adds none of it (it sets `USE_MESA` and
# nothing else, because oops-gl's sources would collide with Mesa's own GL).
#
# **`src/gl/glut.c` is the interesting entry and the reason this title works at all.** It is in
# oops-sdk's `gl/` directory but it is not part of oops-gl: it names no backend, calls
# `oops_gfx_create`/`_present`/`_extent` and otherwise plain GL, so linked beside Mesa it drives
# Mesa. None of the rest of `src/gl/` comes with it, which is what keeps the two OpenGL
# implementations out of the same link.
#
# The list below is what `gears` resolves against and no more. A demo that needs something else
# will fail `make imports` by name - that check reads the ELF's undefined symbols and refuses to
# write a manifest with a gap in it - and this grows from that rather than from `ls src/`.
# Deferred `=`, not `:=`, and it matters: `OOPS_SDK_DIR` is set by `common/app.mk`, which is
# included at the bottom of this file. An immediate assignment expands it to empty here and make
# reports `No rule to make target '/src/gl/glut.c'` - a path with the root missing.
PAYLOAD_SRCS = shim/mesa_demos_entry.c shim/glu_maths_from_libm.c shim/glu_absent_on_mesa.c \
                shim/glad_from_driver.c shim/glu_matrix_on_mesa.c \
                $(DEMO_SRC) \
                upstream/src/util/readtex.c \
                $(OOPS_SDK_DIR)/src/gl/glut.c \
                $(OOPS_SDK_DIR)/src/gl/glut_font.c \
                $(OOPS_SDK_DIR)/src/display.c \
                $(OOPS_SDK_DIR)/src/agc/agc_display.c \
                $(OOPS_SDK_DIR)/src/agc/agc_tiler.c \
                $(OOPS_SDK_DIR)/src/memory/memory.c \
                $(OOPS_SDK_DIR)/src/thread/thread.c \
                $(OOPS_SDK_DIR)/src/time/time.c \
                $(OOPS_SDK_DIR)/src/system/system.c \
                $(OOPS_SDK_DIR)/src/system/freestd.c \
                $(OOPS_SDK_DIR)/src/system/syscall.c \
                $(OOPS_SDK_DIR)/src/system/sysmodule.c \
                $(OOPS_SDK_DIR)/src/system/fs.c \
                $(OOPS_SDK_DIR)/src/math/math.c \
                $(OOPS_SDK_DIR)/src/input/input.c \
                $(OOPS_SDK_DIR)/src/input/keyboard.c \
                $(OOPS_SDK_DIR)/src/input/mouse.c \
                $(OOPS_SDK_DIR)/src/agc/agc_compute.c

# The same local link script the Mesa probes and `mesa-cube` carry, taken verbatim. SELFish's
# `native_eboot.ld` emits no `PT_TLS` segment, Mesa's GL dispatch is thread-local by construction,
# and the link fails with `has an STT_TLS symbol but doesn't have a PT_TLS segment` without it -
# which is exactly how this title first failed. It also carries the `.init_array` output sections
# with `PROVIDE_HIDDEN` bounds, which the shim's `oops_mesa_run_init_array()` walks. The copy goes
# when `REQ-20260915T0001Z-8d72` on the SELFish bus resolves and the shared script grows both.
TARGET_LD_SCRIPT := $(abspath local_tls.ld)

# The import manifest, generated rather than written - the arrangement every Mesa-linked title
# here uses, because such a title imports far more than `common/symbols.txt` carries and every
# attribution comes from obSCEne's mined corpus rather than anyone's judgement.
#
# Run `make imports` after a link, then `make title`. Not optional: without `build/imports.txt`
# mkmodule builds no vendor segment and the container is refused when it loads.
OOPS_MESA ?= $(abspath $(OOPS_APPS_ROOT)/../oops-mesa)
SYMBOLS_FILE := $(abspath build/imports.txt)

include $(OOPS_APPS_ROOT)/common/app.mk

$(SYMBOLS_FILE): $(BUILD)/$(APP_NAME).elf
	@if [ ! -f "$@" ]; then \
	  mkdir -p $(BUILD); \
	  OOPS_APPS_SYMBOLS=$(OOPS_APPS_ROOT)/common/symbols.txt \
	    bash $(OOPS_MESA)/tools/generate-imports.sh $< $@; \
	fi

.PHONY: imports
imports: $(BUILD)/$(APP_NAME).elf
	@mkdir -p $(BUILD)
	@OOPS_APPS_SYMBOLS=$(OOPS_APPS_ROOT)/common/symbols.txt \
	  bash $(OOPS_MESA)/tools/generate-imports.sh $< $(SYMBOLS_FILE)

# Stage the demos' textures and models into the packaged title.
#
# Separate from `title` on purpose: `make title` is what the sweep and every other consumer runs,
# and it must not depend on 2.1 MB of assets that only eight of the 56 demos read. Run this after
# `make title` when packaging one of those eight for hardware.
#
# `/app0/` is the title directory on the console, so this lands at the path
# `MESA_DEMOS_DATA_DIR` names.
.PHONY: stage-data
stage-data:
	@test -d $(BUILD)/title/$(TITLE_ID) || { \
	    echo "stage-data: no packaged title at $(BUILD)/title/$(TITLE_ID) - run 'make title' first"; \
	    exit 1; \
	}
	@mkdir -p $(BUILD)/title/$(TITLE_ID)/data
	@n=0; for f in upstream/src/data/*; do \
	    case "$$f" in *meson.build) continue ;; esac; \
	    cp "$$f" $(BUILD)/title/$(TITLE_ID)/data/ && n=$$((n+1)); \
	done; \
	echo "stage-data: $$n files -> $(BUILD)/title/$(TITLE_ID)/data (read as $(MESA_DEMOS_DATA_DIR))"
