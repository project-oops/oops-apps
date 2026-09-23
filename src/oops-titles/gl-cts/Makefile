# The OpenGL CTS, built for the console.
#
# `upstream.lock` says what is fetched and why. This file is the port, and today it builds the
# **framework only** - the part that has to compile and run before any test can. See
# `oops-mesa`'s roadmap row 8 for what the finished thing has to deliver.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# The fetch has to happen while this file is being read, because the source lists below wildcard
# directories that do not exist in a fresh checkout. `common/upstream.mk` explains the ordering
# trap in full; the short version is that a title which lists `upstream/` sources and includes
# `app.mk` last builds wrong once and right afterwards.
include $(OOPS_APPS_ROOT)/common/upstream.mk

OOPS_RENDERER = mesa

UP := upstream/framework

# # The build order is the dependency order, and it is upstream's
#
# `delibs` is the bottom: C base libraries with no C++ and no knowledge of GL. `qphelper` writes
# the `.qpa` result log. `common` is `tcu`, the test-case framework proper. `opengl` is `glu`,
# the GL object wrappers. `referencerenderer` is `rr`, a software rasteriser the image-comparison
# tests draw the expected answer with.
#
# Each list is a wildcard rather than an enumeration, because upstream owns the file set and a
# missing file should be a link error naming a symbol, not a silent omission. The exclusions
# below are the exception and each one says why.
DELIBS_SRCS := \
    $(wildcard $(UP)/delibs/debase/*.c) \
    $(wildcard $(UP)/delibs/depool/*.c) \
    $(wildcard $(UP)/delibs/deutil/*.c) \
    $(wildcard $(UP)/delibs/dethread/*.c) \
    $(wildcard $(UP)/delibs/decpp/*.cpp) \
    $(wildcard $(UP)/delibs/deimage/*.c)

QPHELPER_SRCS := $(wildcard $(UP)/qphelper/*.c) $(wildcard $(UP)/qphelper/*.cpp)
TCU_SRCS      := $(wildcard $(UP)/common/*.cpp)
GLU_SRCS      := $(wildcard $(UP)/opengl/*.cpp)
RR_SRCS       := $(wildcard $(UP)/referencerenderer/*.cpp)

CTS_INCLUDES := \
    -I$(UP)/delibs/debase -I$(UP)/delibs/depool -I$(UP)/delibs/deutil \
    -I$(UP)/delibs/dethread -I$(UP)/delibs/decpp -I$(UP)/delibs/deimage \
    -I$(UP)/qphelper -I$(UP)/common -I$(UP)/opengl -I$(UP)/referencerenderer \
    -Ishim/include

.PHONY: survey
# Compile every framework source one at a time and report what each failure is waiting for. This
# is the same shape as the libc++ survey behind `oops-apps#D007`, and for the same reason: a port
# this size needs a cause histogram, not a first error.
survey:
	@tools/survey.sh
