# gl-replay - replay a captured GL frame against the software reference, on a build machine.
#
#   make check                              the in-process round trip, as a build gate
#   ./build/gl-replay_selftest F.oglcap OUT.ppm   draw a console capture here
#
# **Host only, deliberately.** Everything else under `src/oops-gl/` packages a payload, because
# the question it answers is what the console does. This one's whole purpose is to answer the
# same question *without* the console: a frame captured there is replayed here against the
# reference rasteriser, and the difference between the two images is the defect. A payload would
# make it possible to run the tool on the very machine it exists to take out of the loop.
#
# So there is no `PAYLOAD_SRCS` and no `app.env`. `common/app.mk` builds only the self-test
# binary when a Makefile lists `HOST_TEST_SRCS` and nothing else, which is the shape `gl2-cube`
# already relies on for its check half.
OOPS_APPS_ROOT ?= $(abspath ../../..)
EXTRA_CFLAGS += -I$(OOPS_APPS_ROOT)/common

APP_NAME = gl-replay

# The whole GL layer: a capture can contain any call the recorder hooks, so the replay needs
# every entry point behind them - the rasteriser, the texture code, the shader front end. The
# display and allocator are the tool's own, since the SDK's talk to hardware that is not here.
# `fs.c` and `heap.c` because `system.c` reaches into them for the log's disk sink; the payload
# link has always had both, and only the host binaries had to be told.
HOST_TEST_SRCS = gl_replay_main.c \
                 $(OOPS_GL_SRCS) \
                 $(OOPS_SDK_DIR)/src/system/system.c \
                 $(OOPS_SDK_DIR)/src/system/fs.c \
                 $(OOPS_SDK_DIR)/src/memory/heap.c \
                 $(OOPS_SDK_DIR)/src/system/freestd.c
HOST_TEST_LIBS = -lm

include $(OOPS_APPS_ROOT)/common/app.mk
