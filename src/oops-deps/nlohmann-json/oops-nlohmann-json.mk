# nlohmann/json build integration.
#
#   OOPS_JSON ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/nlohmann-json)
#   include $(OOPS_JSON)/oops-nlohmann-json.mk
#   EXTRA_TARGET_CFLAGS += $(OOPS_JSON_INCLUDE)
#
# Header-only, so there is no archive and no `OOPS_JSON_LIB`. Consumers use the amalgamated
# header in `single_include`, as upstream directs. It compiles unpatched against this
# collection's libc++.
ifndef OOPS_JSON_DIR
OOPS_JSON_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_JSON_UPSTREAM ?= $(OOPS_JSON_DIR)/upstream
OOPS_JSON_INCLUDE := -I$(OOPS_JSON_UPSTREAM)/single_include

.PHONY: nlohmann-json-clean
nlohmann-json-clean:
	@:
