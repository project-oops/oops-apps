# mesa-demos - upstream Mesa's own feature demos, built against upstream Mesa.
#
# The bring-up slot from `src/oops-titles/README.md`. One demo per build:
#
#   make title              # the default, `gears`
#   make title DEMO=engine  # any other name from upstream/src/demos/
#
# No menu: none of the code between the console and the demo is ours, so a demo that draws
# wrong here and right on a desktop points at oops-mesa.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# oops-sdk's GLUT names no backend (it goes through `oops/gfx.h`), so this selects upstream
# Mesa with radeonsi and ACO instead of oops-gl (oops-mesa#D013).
OOPS_RENDERER = mesa

DEMO ?= gears

DEMO_SRC := upstream/src/demos/$(DEMO).c

# `upstream/src/util`'s `glut_wrap.h` includes <GL/glut.h>, which resolves to oops-sdk's.
# `shim/include` comes first so `glad/glad.h` resolves to the shim, which answers glad's
# version and extension flags from the driver.
MESA_DEMOS_CFLAGS := -Ishim/include -Iupstream/src/util -Iupstream/src

# The demo's name, for the log lines the shim prints.
MESA_DEMOS_CFLAGS += -DOOPS_DEMOS_NAME=\"$(DEMO)\"

# `DEMO=` changes a `-D`, not a file, so a stamp holding the demo name is rewritten when it
# differs and the object carrying `OOPS_DEMOS_NAME` depends on it.
DEMO_STAMP := build/.demo-name
$(shell mkdir -p build 2>/dev/null; \
        [ "$$(cat $(DEMO_STAMP) 2>/dev/null)" = "$(DEMO)" ] || printf '%s' "$(DEMO)" > $(DEMO_STAMP))

# Where the demos look for textures and models. The demos concatenate it directly
# (`DEMOS_DATA_DIR "reflect.rgb"`, `upstream/meson.build:30`), so it ends in a slash.
# `/app0/` is the title's directory on the console; `make stage-data` copies the files there.
MESA_DEMOS_DATA_DIR ?= /app0/data/
MESA_DEMOS_CFLAGS += -DDEMOS_DATA_DIR=\"$(MESA_DEMOS_DATA_DIR)\"

# The proportional GLUT font names, mapped to the face oops-sdk has. Force-included because
# `upstream/` is fetched output and is never edited.
MESA_DEMOS_CFLAGS += -include $(abspath shim/include/glut_proportional_fonts.h)

# This repository's `-Wall -Wextra -Werror` covers code written here, not upstream's demos.
MESA_DEMOS_WARNINGS := -Wno-strict-prototypes -Wno-missing-prototypes -Wno-conversion \
                       -Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-comment \
                       -Wno-unused-parameter -Wno-unused-but-set-variable \
                       -Wno-shadow -Wno-sign-compare -Wno-char-subscripts \
                       -Wno-string-concatenation -Wno-cast-function-type-mismatch \
                       -Wno-missing-variable-declarations -Wno-double-promotion

EXTRA_TARGET_CFLAGS += $(MESA_DEMOS_CFLAGS) $(MESA_DEMOS_WARNINGS)

# The oops-sdk features this title links. `OOPS_RENDERER = mesa` adds none, so oops-gl stays
# out of the link; oops-sdk's GLUT calls `oops_gfx_*` and plain GL, so beside Mesa it drives
# Mesa. `make imports` names any symbol a demo needs that is missing.
OOPS_FEATURES := glu glut display thread suspend input keyboard mouse

PAYLOAD_SRCS = shim/mesa_demos_entry.c shim/glad_from_driver.c \
               $(DEMO_SRC) \
               upstream/src/util/readtex.c \
               upstream/src/util/showbuffer.c

# SELFish's `native_eboot.ld` plus a PT_TLS segment, which Mesa's thread-local GL dispatch
# needs, and `.init_array` bounds for `oops_mesa_run_init_array()`.
TARGET_LD_SCRIPT := $(abspath local_tls.ld)

# The import manifest is generated from obSCEne's corpus; a Mesa title imports more than
# `common/symbols.txt` carries. Run `make imports` after a link, then `make title`: without
# `build/imports.txt` mkmodule builds no vendor segment and the loader refuses the module.
OOPS_MESA ?= $(abspath $(OOPS_APPS_ROOT)/../oops-mesa)
SYMBOLS_FILE := $(abspath build/imports.txt)

include $(OOPS_APPS_ROOT)/common/app.mk

# The one object with `OOPS_DEMOS_NAME` compiled into it; after the include for `$(BUILD)`.
$(BUILD)/obj/app/shim/mesa_demos_entry.o: $(DEMO_STAMP)

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

# Stage the demos' textures and models into the packaged title, at MESA_DEMOS_DATA_DIR.
# Separate from `title`, which does not depend on assets most demos never read.
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
