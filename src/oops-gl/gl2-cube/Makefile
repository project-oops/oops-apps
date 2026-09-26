# gl2-cube - the GL 2.0 cube, drawn by shaders.
#
#   make check      compile, link and draw the cube through oops-gl's software reference
#   make eboot      signed eboot.bin container via selfish
#   make title      complete title package (.zip) for homebrew installation
#   make dist       stages conforming release artifacts under dist/
#
# Draws a shader program through oops-gl's own draw path. `gl1-cube` is the fixed-function
# half of the pair: the two draw the same cube in the same place at the same angles, so any
# difference between them is fixed function against a shader program.
#
# No `elf` in FORMATS: this collection installs native titles, so the eboot and the title
# package are staged. The `.elf` is still built as the intermediate both are made from,
# linked against `selfish/link/native_eboot.ld`.

OOPS_APPS_ROOT ?= $(abspath ../../..)
EXTRA_CFLAGS += -I$(OOPS_APPS_ROOT)/common
EXTRA_TARGET_CFLAGS += -I$(OOPS_APPS_ROOT)/common

# The whole GL layer, from the one list in common/app.mk - the check draws, so the shader front
# end and interpreter need the context, the rasteriser and the texture code behind them. The
# self-test supplies its own display and allocator, since the SDK's are direct memory.
# system.c, fs.c and heap.c travel together: the disk sink reaches fs.c, fs.c reaches the
# allocator.
HOST_TEST_SRCS = gl2_cube_selftest.c \
                 $(OOPS_GL_SRCS) \
                 $(OOPS_SDK_DIR)/src/system/system.c \
                 $(OOPS_SDK_DIR)/src/system/fs.c \
                 $(OOPS_SDK_DIR)/src/memory/heap.c \
                 $(OOPS_SDK_DIR)/src/system/freestd.c
HOST_TEST_LIBS = -lm

OOPS_RENDERER = gl2
OOPS_FEATURES := display input draw

PAYLOAD_SRCS = gl2_cube_main.c

ENTRY_POINT := gl2_cube_start

include $(OOPS_APPS_ROOT)/common/app.mk
