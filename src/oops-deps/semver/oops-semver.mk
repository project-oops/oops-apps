# semver for a ported title. Include it from a title's Makefile:
#
#   OOPS_SEMVER ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/semver)
#   include $(OOPS_SEMVER)/oops-semver.mk
#   OOPS_CXX_INCLUDE += $(OOPS_SEMVER_INCLUDE)
#
# Header-only, so there is no archive and nothing to link. Consumers write `#include "semver.hpp"`,
# so the include points at the directory holding it rather than its parent.
ifndef OOPS_SEMVER_MK
OOPS_SEMVER_MK := 1

OOPS_SEMVER_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
OOPS_SEMVER_INCLUDE := -I$(OOPS_SEMVER_DIR)/upstream/include

endif
