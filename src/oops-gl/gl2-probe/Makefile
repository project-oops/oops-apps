# gl2-probe - OpenGL 2.0 measured against the specification.
#
#   make check      run the suite against oops-gl's software reference
#   make eboot      the artifact that deploys to a console
#   make title      the installable package
#
# No `elf` in FORMATS: this collection installs native titles, so the eboot and the title
# package are staged. The `.elf` is still built as the intermediate both are made from,
# linked against `selfish/link/native_eboot.ld`.
#
# The suite in `gl2_probe.c` is shared: `gl2_probe_selftest.c` is the host's runner and
# `gl2_probe_main.c` the console's.

OOPS_APPS_ROOT ?= $(abspath ../../..)
EXTRA_CFLAGS += -I$(OOPS_APPS_ROOT)/common
EXTRA_TARGET_CFLAGS += -I$(OOPS_APPS_ROOT)/common

FORMATS = eboot title

# The whole GL layer, from the one list in common/app.mk. The self-test supplies its own display
# and allocator, since the SDK's are direct memory.
# `fs.c` because `system.c` reaches into it for the log's disk sink - `oops_log_enable_disk_sink`
# calls `oops_fs_get_storage_dir`.
HOST_TEST_SRCS = gl2_probe_selftest.c \
                 gl2_probe.c \
                 $(OOPS_GL_SRCS) \
                 $(OOPS_SDK_DIR)/src/system/system.c \
                 $(OOPS_SDK_DIR)/src/system/fs.c \
                 $(OOPS_SDK_DIR)/src/memory/heap.c \
                 $(OOPS_SDK_DIR)/src/system/freestd.c
HOST_TEST_LIBS = -lm

OOPS_RENDERER = gl2
OOPS_FEATURES := display input draw

PAYLOAD_SRCS = gl2_probe_main.c \
               gl2_probe.c

include $(OOPS_APPS_ROOT)/common/app.mk
