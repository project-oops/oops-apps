# gl1-cube - a rotating cube, torus and sphere through oops-gl's OpenGL 1.x path.
#
#   make check      host self-test: the scene, lit, through the software rasteriser
#   make elf        the plain-ELF payload a homebrew loader runs
#   make eboot      signed eboot.bin container via selfish
#   make title      complete title package (.zip) for homebrew installation
#   make dist       stages conforming release artifacts under dist/

OOPS_APPS_ROOT ?= $(abspath ../../..)
EXTRA_TARGET_CFLAGS += -I$(OOPS_APPS_ROOT)/common
EXTRA_CFLAGS += -I$(OOPS_APPS_ROOT)/common

# The renderer this title links (common/app.mk): fixed-function oops-gl, which adds
# $(OOPS_GL_SRCS) to the payload. The host self-test lists them itself.
OOPS_RENDERER = gl1

# obj_loader.c builds the torus and sphere on the host too; the self-test supplies its own
# oops_mem_alloc. system.c, fs.c and heap.c link together: the disk sink reaches fs.c and
# fs.c reaches the allocator. gl_glu.c is GLU, which $(OOPS_GL_SRCS) does not carry.
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

PAYLOAD_SRCS = gl1_cube_main.c \
               $(OOPS_APPS_ROOT)/common/obj_loader.c \
               $(OOPS_APPS_ROOT)/common/cube_hud.c

ENTRY_POINT := gl1_cube_start

include $(OOPS_APPS_ROOT)/common/app.mk
