# toml++ for a ported title. Include it from a title's Makefile:
#
#   OOPS_TOMLPP ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/tomlplusplus)
#   include $(OOPS_TOMLPP)/oops-tomlplusplus.mk
#   OOPS_CXX_INCLUDE += $(OOPS_TOMLPP_INCLUDE)
#
# Header-only in its default configuration, so there is no archive and nothing to link: consumers
# write `#include <toml++/toml.hpp>` and the code compiles in each translation unit that does.
ifndef OOPS_TOMLPP_MK
OOPS_TOMLPP_MK := 1

OOPS_TOMLPP_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
OOPS_TOMLPP_INCLUDE := -I$(OOPS_TOMLPP_DIR)/upstream/include

endif
