# gl-replay - replay a captured GL frame against the software reference, on a build machine.
#
#   make check                              the in-process round trip, as a build gate
#   ./build/gl-replay_selftest F.oglcap OUT.ppm   draw a console capture here
#
# Host only: a frame captured on the console is replayed here against the reference
# rasteriser, and the difference between the two images is the defect. There is no
# `PAYLOAD_SRCS` and no `app.env`; `common/app.mk` builds only the self-test binary when a
# Makefile lists `HOST_TEST_SRCS` and nothing else.
OOPS_APPS_ROOT ?= $(abspath ../../..)
EXTRA_CFLAGS += -I$(OOPS_APPS_ROOT)/common

APP_NAME = gl-replay

# The whole GL layer: a capture can contain any call the recorder hooks, so the replay needs
# every entry point behind them - the rasteriser, the texture code, the shader front end. The
# display and allocator are the tool's own, since the SDK's talk to hardware that is not here.
# `fs.c` and `heap.c` because `system.c` reaches into them for the log's disk sink.
HOST_TEST_SRCS = gl_replay_main.c \
                 $(OOPS_GL_SRCS) \
                 $(OOPS_SDK_DIR)/src/system/system.c \
                 $(OOPS_SDK_DIR)/src/system/fs.c \
                 $(OOPS_SDK_DIR)/src/memory/heap.c \
                 $(OOPS_SDK_DIR)/src/system/freestd.c
HOST_TEST_LIBS = -lm

include $(OOPS_APPS_ROOT)/common/app.mk
