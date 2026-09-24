# gl2-cube - the GL 2.0 cube, drawn by shaders.
#
#   make check      compile, link and draw the cube through oops-gl's software reference
#   make eboot      signed eboot.bin container via selfish
#   make title      complete title package (.zip) for homebrew installation
#   make dist       stages conforming release artifacts under dist/
#
# **This is the title that closes the last gap in the GL 2.0 path.** obSCEne has measured every
# piece of the console back end - a generated pixel shader retires (`REQ-...-4e77`), its
# parameters arrive, its uniform block loads (`REQ-...-6c0d`), it can sample a texture
# (`REQ-...-2a45`) - and every one of those bound a shader from obSCEne's own fixture. What has
# never run is **oops-gl's draw path** binding one. That is what this packages.
#
# `gl1-cube` is the fixed-function half of the pair and is pinned to a measured console frame.
# The two draw the same cube, in the same place, at the same angles, so the only difference
# between them is fixed function against a shader program - which is what makes a difference in
# what they draw mean something.
#
# **No `elf` in FORMATS**, unlike gl1-cube and its neighbours. That format stages the bare ELF
# for a homebrew loader; this collection installs native titles, so the artifacts worth staging
# are the eboot and the title package. The `.elf` is still built - it is the intermediate both
# of those are made from, linked against `selfish/link/native_eboot.ld`.

OOPS_APPS_ROOT ?= $(abspath ../../..)
EXTRA_CFLAGS += -I$(OOPS_APPS_ROOT)/common
EXTRA_TARGET_CFLAGS += -I$(OOPS_APPS_ROOT)/common

# The whole GL layer, from the one list in common/app.mk - the check draws, so the shader front
# end and interpreter need the context, the rasteriser and the texture code behind them. The
# self-test supplies its own display and allocator, since the SDK's are direct memory and have
# nothing to talk to on a build machine.
# system.c, fs.c and heap.c travel together: the disk sink reaches fs.c, fs.c reaches the
# allocator.
HOST_TEST_SRCS = gl2_cube_selftest.c \
                 $(OOPS_GL_SRCS) \
                 $(OOPS_SDK_DIR)/src/system/system.c \
                 $(OOPS_SDK_DIR)/src/system/fs.c \
                 $(OOPS_SDK_DIR)/src/memory/heap.c \
                 $(OOPS_SDK_DIR)/src/system/freestd.c
HOST_TEST_LIBS = -lm

# The payload. The same list gl1-cube carries, minus the pieces this title has no use for:
# `obj_loader.c` (no torus or sphere - one cube, so the difference from gl1-cube stays one
# thing), and the audio and video decoders, which neither title uses and which gl1-cube links
# out of habit.
PAYLOAD_SRCS = gl2_cube_main.c \
               $(OOPS_SDK_DIR)/src/display.c \
               $(OOPS_GL_SRCS) \
               $(OOPS_SDK_DIR)/src/agc/agc_display.c \
               $(OOPS_SDK_DIR)/src/agc/agc_tiler.c \
               $(OOPS_SDK_DIR)/src/agc/agc_compute.c \
               $(OOPS_SDK_DIR)/src/gnm/gnm_display.c \
               $(OOPS_SDK_DIR)/src/input/input.c \
               $(OOPS_SDK_DIR)/src/memory/memory.c \
               $(OOPS_SDK_DIR)/src/memory/heap.c \
               $(OOPS_SDK_DIR)/src/time/time.c \
               $(OOPS_SDK_DIR)/src/system/system.c \
               $(OOPS_SDK_DIR)/src/system/freestd.c \
               $(OOPS_SDK_DIR)/src/system/syscall.c \
               $(OOPS_SDK_DIR)/src/system/sysmodule.c \
               $(OOPS_SDK_DIR)/src/system/krw.c \
               $(OOPS_SDK_DIR)/src/system/fs.c \
               $(OOPS_SDK_DIR)/src/draw/draw.c

ENTRY_POINT := gl2_cube_start

include $(OOPS_APPS_ROOT)/common/app.mk
