# nlohmann/json build integration.
#
#   OOPS_JSON ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/nlohmann-json)
#   include $(OOPS_JSON)/oops-nlohmann-json.mk
#   EXTRA_TARGET_CFLAGS += $(OOPS_JSON_INCLUDE)
#
# **Header-only, so there is no archive and no `OOPS_JSON_LIB`.** The amalgamated header in
# `single_include` is the one upstream tells consumers to use; `include/` carries the split form
# it is generated from and is fetched only because the sparse checkout takes both.
#
# Compiles as it stands against this collection's freestanding libc++ - no patch, no define. That
# is worth stating because it was not a given: it needs `<string>`, `<vector>`, `<map>` and
# exceptions, and it was the threading work done for spdlog that made the standard library
# complete enough for any of that to be true.
ifndef OOPS_JSON_DIR
OOPS_JSON_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_JSON_UPSTREAM ?= $(OOPS_JSON_DIR)/upstream
OOPS_JSON_INCLUDE := -I$(OOPS_JSON_UPSTREAM)/single_include

.PHONY: nlohmann-json-clean
nlohmann-json-clean:
	@:
