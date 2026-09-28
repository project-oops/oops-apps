# spdlog build integration.
#
#   OOPS_SPDLOG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/spdlog)
#   include $(OOPS_SPDLOG)/oops-spdlog.mk
#   EXTRA_TARGET_CFLAGS += $(OOPS_SPDLOG_INCLUDE)
#
# Header-only, so there is no archive and no `OOPS_SPDLOG_LIB`. The compiled form behind
# `SPDLOG_COMPILED_LIB` would be a `src/*.cpp` list and an `ar` rule shaped like `oops-libogg.mk`.
ifndef OOPS_SPDLOG_DIR
OOPS_SPDLOG_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_SPDLOG_UPSTREAM ?= $(OOPS_SPDLOG_DIR)/upstream

OOPS_SPDLOG_INCLUDE := -I$(OOPS_SPDLOG_UPSTREAM)/include

# Nothing to clean; the target exists so every dependency has a clean rule.
.PHONY: spdlog-clean
spdlog-clean:
	@:
