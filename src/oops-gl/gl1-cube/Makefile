# gl-cube - Clean-room OpenGL 3D Rotating Cube Demo for Prospero / Trinity.
#
#   make elf        the plain-ELF payload a homebrew loader runs
#   make eboot      signed eboot.bin container via selfish
#   make title      complete title package (.zip) for homebrew installation
#   make dist       stages conforming release artifacts under dist/

OOPS_APPS_ROOT ?= $(abspath ../../..)
EXTRA_TARGET_CFLAGS += -I$(OOPS_APPS_ROOT)/common
EXTRA_CFLAGS += -I$(OOPS_APPS_ROOT)/common

# The renderer this title links (common/app.mk): fixed-function oops-gl. This pulls in
# $(OOPS_GL_SRCS) for the payload, so the list below no longer names it. The host self-test still
# lists it in HOST_TEST_SRCS, which the flag does not touch.
OOPS_RENDERER = gl1

# oops-mesa's generated preamble (roadmap unit 2) is picked up from the sibling checkout when it
# is there; gl1_cube_main.c tests for the header and builds without the control files otherwise.
OOPS_MESA ?= $(abspath $(OOPS_APPS_ROOT)/../oops-mesa)
EXTRA_TARGET_CFLAGS += -I$(OOPS_MESA)/tools/preamble-dump

# Host test sources
#
# obj_loader.c and math.c are here so the torus and sphere the payload can switch to are built
# and rasterised on the host too; the self-test supplies its own oops_mem_alloc, since the SDK's
# allocator is direct memory and has nothing to talk to on a build machine.
# system.c, fs.c and heap.c travel together on a host link. `oops_log_enable_disk_sink` calls
# `oops_fs_get_storage_dir`, and fs.c's directory walk calls `oops_malloc`/`oops_free` - so one
# without the next two does not resolve. gl-replay and seashell already list the trio; this did
# not, and the gate could not say so while it was stopping on the wrong clang. `oops_mem_alloc`
# stays the self-test's own, below: heap.c is the allocator, that is the direct memory under it.
# `gl_glu.c` is linked here as well as in PAYLOAD_SRCS below: the self-test projects with
# `gluPerspective`, and GLU is a separate library from GL that `$(OOPS_GL_SRCS)` does not carry.
HOST_TEST_SRCS = gl1_cube_selftest.c \
                 $(OOPS_APPS_ROOT)/common/obj_loader.c \
                 $(OOPS_GL_SRCS) \
                 $(OOPS_SDK_DIR)/src/gl/gl_glu.c \
                 $(OOPS_SDK_DIR)/src/system/system.c \
                 $(OOPS_SDK_DIR)/src/system/fs.c \
                 $(OOPS_SDK_DIR)/src/memory/heap.c \
                 $(OOPS_SDK_DIR)/src/system/freestd.c
HOST_TEST_LIBS = -lm

OOPS_FEATURES := display hud glu input keyboard mouse audio audiodec videodec draw

# Target payload sources
PAYLOAD_SRCS = gl1_cube_main.c \
               $(OOPS_APPS_ROOT)/common/obj_loader.c \
               $(OOPS_APPS_ROOT)/common/cube_hud.c

ENTRY_POINT := gl1_cube_start

include $(OOPS_APPS_ROOT)/common/app.mk
