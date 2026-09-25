# glm, the header-only vector and matrix library. Include from a title's Makefile:
#
#   OOPS_GLM ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/glm)
#   include $(OOPS_GLM)/oops-glm.mk
#
#   OOPS_CXX_INCLUDE += $(OOPS_GLM_INCLUDE)
#
# **Nothing to build.** glm is templates in headers, so this file is an include path and one
# define, and has no archive for a title to link.
#
# `GLM_ENABLE_EXPERIMENTAL` is the define upstream asks a program to set before it uses a `gtx/`
# extension. At 0.9.9.8 its absence costs only a `#pragma message`, and only when
# `GLM_MESSAGES` is on; glm 1.0 turns it into an `#error`. It is set here, once, so that a bump
# to 1.0 is not the day every title using `gtx/` stops compiling. SuperTux's CMake sets it too.
ifndef OOPS_GLM_DIR
OOPS_GLM_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif

OOPS_GLM_UPSTREAM ?= $(OOPS_GLM_DIR)/upstream

OOPS_GLM_INCLUDE := -I$(OOPS_GLM_UPSTREAM) -DGLM_ENABLE_EXPERIMENTAL

.PHONY: glm-clean
glm-clean:
	@:
