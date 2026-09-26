# glm, the header-only vector and matrix library. Include from a title's Makefile:
#
#   OOPS_GLM ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/glm)
#   include $(OOPS_GLM)/oops-glm.mk
#
#   OOPS_CXX_INCLUDE += $(OOPS_GLM_INCLUDE)
#
# glm is templates in headers, so this is an include path and one define, with no archive.
#
# `GLM_ENABLE_EXPERIMENTAL` is the define upstream requires before a `gtx/` extension is used;
# glm 1.0 makes its absence an `#error`. SuperTux's CMake sets it too.
ifndef OOPS_GLM_DIR
OOPS_GLM_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif

OOPS_GLM_UPSTREAM ?= $(OOPS_GLM_DIR)/upstream

OOPS_GLM_INCLUDE := -I$(OOPS_GLM_UPSTREAM) -DGLM_ENABLE_EXPERIMENTAL

.PHONY: glm-clean
glm-clean:
	@:
