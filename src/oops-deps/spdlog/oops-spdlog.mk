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

# `FMT_USE_LOCALE=0` is required: the bundled fmt's `thousands_sep_impl` and `decimal_point_impl`
# use `std::numpunct<wchar_t>`, which libc++ built with `_LIBCPP_HAS_WIDE_CHARACTERS 0` lacks.
# The remaining wide-character use, `utf8_to_utf16::str()`, has no switch and is `patches/0001`.
OOPS_SPDLOG_INCLUDE := -I$(OOPS_SPDLOG_UPSTREAM)/include -DFMT_USE_LOCALE=0

# Nothing to clean; the target exists so every dependency has a clean rule.
.PHONY: spdlog-clean
spdlog-clean:
	@:
