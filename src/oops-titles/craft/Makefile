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

# Craft draws everything through four shader pairs and has no fixed-function path.
OOPS_RENDERER = gl2

# Included first so that `upstream/` exists before the wildcard below expands. It is
# guarded, so `common/app.mk`'s own include of it costs nothing.
include $(OOPS_APPS_ROOT)/common/upstream.mk

FORMATS = elf

# The POSIX layer above oops-sdk's libc - `stat`, `opendir`, `getcwd` - which Craft
# reaches through SQLite. Shared with other ports, so it lives in `common/`.
include $(OOPS_APPS_ROOT)/common/posix.mk

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

# Craft's own sources except `auth.c`, the HTTPS login and the only user of libcurl. The
# port is single-player; `shim/craft_offline.c` supplies the symbol `main.c` calls.
# `client.c` is compiled and stays inert until `client_enable()` is called.
CRAFT_UPSTREAM_SRCS := $(filter-out upstream/src/auth.c,$(wildcard upstream/src/*.c))

# The vendored libraries Craft carries. sqlite and tinycthread need a platform
# underneath them; lodepng and noise are pure C.
CRAFT_DEP_SRCS := upstream/deps/lodepng/lodepng.c \
                  upstream/deps/noise/noise.c \
                  upstream/deps/sqlite/sqlite3.c \
                  upstream/deps/tinycthread/tinycthread.c

# `-Ishim/include` first, then upstream's include directories in its CMakeLists order.
#
# `CRAFT_DATA_DIR` is the install directory rather than `/app0`, because a homebrew
# title launched from `/data/homebrew/<id>` cannot open `/app0` or anything under it.
# The id is written out because `$(TITLE_ID)` comes from `common/app.mk`, included
# later.
CRAFT_CFLAGS := -Ishim/include \
                -Iupstream/src \
                -Iupstream/deps/lodepng \
                -Iupstream/deps/noise \
                -Iupstream/deps/sqlite \
                -Iupstream/deps/tinycthread \
                -DOOPS_POSIX_HOME=\"/app0\" \
                -DCRAFT_DATA_DIR=\"/app0\" \
                -DDB_PATH=\"/app0/craft.db\" \
                $(OOPS_POSIX_INCLUDE) $(OOPS_SQLITE_INCLUDE) $(OOPS_SQLITE_DEFS)

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
                           -Wno-unused-function

EXTRA_TARGET_CFLAGS += $(CRAFT_CFLAGS) $(CRAFT_UPSTREAM_WARNINGS)

# Everything the payload links. `CRAFT_SHIM_SRCS` carries `common/sqlite`'s VFS along
# with the GLFW shim.
CRAFT_PAYLOAD_SRCS := $(CRAFT_SHIM_SRCS) $(CRAFT_UPSTREAM_SRCS) $(CRAFT_DEP_SRCS) \
                      $(OOPS_POSIX_SRCS)
PAYLOAD_SRCS := $(CRAFT_PAYLOAD_SRCS)

# The SDK subsystems this title uses, declared as capabilities via OOPS_FEATURES.
OOPS_FEATURES := display input keyboard mouse thread net

# ---------------------------------------------------------------------------
# `make check`: every shader Craft ships, compiled for the console.
#
# oops-gl's console back end compiles a fragment shader to gfx1030 instructions or
# refuses it, and a refused shader fails its draw with `GL_INVALID_OPERATION`. The
# checker reads `upstream/shaders/*.glsl` directly and prints each shader's cost.
# ---------------------------------------------------------------------------
# system.c, fs.c and heap.c travel together: the disk sink reaches fs.c, fs.c reaches
# the allocator.
HOST_TEST_SRCS = tools/shadercheck.c \
                 $(OOPS_GL_SRCS) \
                 $(OOPS_SDK_DIR)/src/system/system.c \
                 $(OOPS_SDK_DIR)/src/system/fs.c \
                 $(OOPS_SDK_DIR)/src/memory/heap.c \
                 $(OOPS_SDK_DIR)/src/system/freestd.c
HOST_TEST_LIBS = -lm

include $(OOPS_APPS_ROOT)/common/app.mk

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
