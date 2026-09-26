# Craft, through oops-sdk.
#
#   make check               compile every shader Craft ships, then run the census
#   make elf                 build the payload
#   make title stage-data    package the title with its textures and shaders

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Defined here because the `:=` lists below name files under it before `app.mk` is
# included. The `?=` matches app.mk's own default, so including app.mk changes nothing.
OOPS_SDK ?= $(abspath $(OOPS_APPS_ROOT)/../oops-sdk)
OOPS_SDK_DIR := $(abspath $(OOPS_SDK))

# Craft draws everything through four shader pairs and has no fixed-function path, and
# it submits a world's worth of geometry: 90000 triangles a frame once the chunks load.
# That is upstream Mesa's radeonsi, which runs the vertex stage on the GPU. oops-gl's
# GL 2.0 path transforms every vertex on the CPU, which measured at 150us a triangle.
OOPS_RENDERER = mesa

# Included first so that `upstream/` exists before the wildcard below expands. It is
# guarded, so `common/app.mk`'s own include of it costs nothing.
include $(OOPS_APPS_ROOT)/common/upstream.mk

FORMATS = elf

# No `common/posix.mk` here. That layer stands in for a libc a freestanding payload has
# not got; `OOPS_RENDERER = mesa` is a hosted build with oops-mesa's sysroot, which has
# the real headers, and defining `mode_t` twice is a build error rather than a choice.

# SQLite's platform layer: a VFS over `oops/fs.h`, a mutex implementation over
# `oops/thread.h`, and the defines that remove SQLite's unix backend. The mutexes are
# real because `db.c` drives one connection from a writer thread and the main thread.
# `/app0`, not `/data`: `/data` is outside a title's sandbox - `common/posix/posix.c` records the
# measurement - and `/app0` is inside it and writable. What a title writes to `/app0` is lost on a
# redeploy, which for a world file is a real cost and still better than a path that cannot be
# opened at all. Real persistence wants savedata mounted, which this title does not do yet.
OOPS_SQLITE_TEMP_DIR := /app0
include $(OOPS_APPS_ROOT)/common/sqlite.mk

# GLFW and GLEW are not compiled: `shim/` answers every `glfw*` and GLEW entry point
# Craft's own sources call, over oops-sdk's display, input and clock. `shim/include`
# goes first on the include path so that `#include <GLFW/glfw3.h>` finds the shim.
CRAFT_SHIM_SRCS := shim/glfw_shim.c shim/craft_offline.c shim/craft_start.c $(OOPS_SQLITE_SRCS)

# Craft's own sources except the multiplayer half: `auth.c` is the HTTPS login and the
# only user of libcurl, and `client.c` talks to a server over BSD sockets, which
# oops-mesa's sysroot has no headers for. The port is single-player, so
# `shim/craft_offline.c` supplies both files' symbols and answers "not connected".
CRAFT_UPSTREAM_SRCS := $(filter-out upstream/src/auth.c upstream/src/client.c,\
                                    $(wildcard upstream/src/*.c))

# The vendored libraries Craft carries. sqlite needs a platform underneath it; lodepng
# and noise are pure C. `tinycthread.c` is left out: it is C11 `<threads.h>` for
# platforms without one, and oops-mesa's runtime defines `mtx_*`, `cnd_*` and `thrd_*`
# already, so compiling it duplicates every symbol. Its header still resolves the
# declarations Craft includes.
CRAFT_DEP_SRCS := upstream/deps/lodepng/lodepng.c \
                  upstream/deps/noise/noise.c \
                  upstream/deps/sqlite/sqlite3.c

# `-Ishim/include` first, then upstream's include directories in its CMakeLists order.
#
# `CRAFT_DATA_DIR` roots every asset path, `OOPS_POSIX_HOME` every relative `open`, and
# `DB_PATH` the world. All three are `/app0`, the package mount: assets ship inside it
# and it is writable, so the world file lands beside them.
CRAFT_CFLAGS := -Ishim/include \
                -Iupstream/src \
                -Iupstream/deps/lodepng \
                -Iupstream/deps/noise \
                -Iupstream/deps/sqlite \
                -Iupstream/deps/tinycthread \
                -DOOPS_POSIX_HOME=\"/app0\" \
                -DCRAFT_DATA_DIR=\"/app0\" \
                -DDB_PATH=\"/app0/craft.db\" \
                -DSHOW_CONTROLS_TEXT=1 \
                $(OOPS_SQLITE_INCLUDE) $(OOPS_SQLITE_DEFS)

# Upstream's code under upstream's warnings. `common/app.mk` compiles at `-Werror` with
# warnings stricter than Craft was written for; they are switched off rather than
# patched, since a patch would be a rebase burden with no behaviour change.
# `-Wno-conversion` is deliberately the wide flag. These apply to `shim/` as well.
CRAFT_UPSTREAM_WARNINGS := -Wno-strict-prototypes -Wno-missing-prototypes -Wno-conversion \
                           -Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-comment \
                           -Wno-unused-parameter -Wno-unused-but-set-variable \
                           -Wno-shadow -Wno-sign-compare -Wno-char-subscripts \
                           -Wno-string-concatenation -Wno-cast-function-type-mismatch \
                           -Wno-null-pointer-subtraction -Wno-unused-const-variable \
                           -Wno-fortify-source \
                           -Wno-unused-function

EXTRA_TARGET_CFLAGS += $(CRAFT_CFLAGS) $(CRAFT_UPSTREAM_WARNINGS)

# Everything the payload links. `CRAFT_SHIM_SRCS` carries `common/sqlite`'s VFS along
# with the GLFW shim.
CRAFT_PAYLOAD_SRCS := $(CRAFT_SHIM_SRCS) $(CRAFT_UPSTREAM_SRCS) $(CRAFT_DEP_SRCS)
PAYLOAD_SRCS := $(CRAFT_PAYLOAD_SRCS)

# Mesa needs a PT_TLS segment and the `.init_array` bounds `oops_mesa_run_init_array`
# walks; `selfish`'s default script emits neither.
TARGET_LD_SCRIPT := $(abspath $(OOPS_APPS_ROOT)/common/mesa_title.ld)

# A Mesa title imports more libc than `common/symbols.txt` names, so the manifest is
# generated from the linked payload. Without `build/imports.txt` mkmodule builds no
# vendor segment and the loader refuses the module. `make imports` after a link, then
# `make title`.
OOPS_MESA ?= $(abspath $(OOPS_APPS_ROOT)/../oops-mesa)
SYMBOLS_FILE := $(abspath build/imports.txt)

# The SDK subsystems this title uses, declared as capabilities via OOPS_FEATURES.
OOPS_FEATURES := display input keyboard mouse thread net

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

# ---------------------------------------------------------------------------
# `make stage-data`: the assets, into the packaged title
#
#   make title stage-data
#
# Craft does not start without these: `main` loads four textures and four shader pairs
# before drawing, and `load_png_texture` calls `exit(1)` when one is missing. A separate
# target because `title` belongs to `common/app.mk` and has no post-staging hook.
# ---------------------------------------------------------------------------
CRAFT_TITLE_DIR = $(BUILD)/title/$(TITLE_ID)

.PHONY: stage-data
stage-data:
	@test -d $(CRAFT_TITLE_DIR) || { \
	    echo "stage-data: no packaged title at $(CRAFT_TITLE_DIR) - run 'make title' first"; \
	    exit 1; \
	}
	@mkdir -p $(CRAFT_TITLE_DIR)/textures $(CRAFT_TITLE_DIR)/shaders
	@cp upstream/textures/*.png $(CRAFT_TITLE_DIR)/textures/
	@cp upstream/shaders/*.glsl $(CRAFT_TITLE_DIR)/shaders/
	@t=$$(ls $(CRAFT_TITLE_DIR)/textures | wc -l); s=$$(ls $(CRAFT_TITLE_DIR)/shaders | wc -l); \
	 printf 'stage-data: %s textures, %s shaders -> %s\n' "$$t" "$$s" "$(CRAFT_TITLE_DIR)"; \
	 test "$$t" -eq 4 || { echo "stage-data: expected 4 textures"; exit 1; }; \
	 test "$$s" -eq 8 || { echo "stage-data: expected 8 shaders"; exit 1; }

# ---------------------------------------------------------------------------
# `make census`: which of this title's sources compile for the target
# ---------------------------------------------------------------------------
#
# Compiles every source, plus the excluded `auth.c`, to `/dev/null` under the real
# target flags and prints each one's first error. It reports and does not fail the
# build. `check` gains it as a prerequisite; the recipe stays in `common/app.mk`.
.PHONY: census
check: census
census:
	@echo "craft census: $(words $(CRAFT_PAYLOAD_SRCS)) sources, under the target's own flags"
	@ok=0; bad=0; \
	for f in $(CRAFT_UPSTREAM_SRCS) upstream/src/auth.c $(CRAFT_DEP_SRCS) $(CRAFT_SHIM_SRCS); do \
	    err=$$($(TARGET_CC) $(TARGET_CFLAGS) -c -o /dev/null "$$f" 2>&1 | \
	           grep -m1 -E 'error:' | sed 's/.*error: //'); \
	    if [ -z "$$err" ]; then \
	        printf '  %-40s ok\n' "$$f"; ok=$$((ok+1)); \
	    else \
	        printf '  %-40s FAIL  %s\n' "$$f" "$$err"; bad=$$((bad+1)); \
	    fi; \
	done; \
	echo "craft census: $$ok compile, $$bad do not"
