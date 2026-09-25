# spdlog build integration.
#
#   OOPS_SPDLOG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/spdlog)
#   include $(OOPS_SPDLOG)/oops-spdlog.mk
#   EXTRA_TARGET_CFLAGS += $(OOPS_SPDLOG_INCLUDE)
#
# **Header-only, so there is no archive and no `OOPS_SPDLOG_LIB`.** spdlog offers a compiled form
# behind `SPDLOG_COMPILED_LIB`, which would be the faster choice for a tree that includes it in
# 123 files - Ship of Harkinian's does. It is not taken yet because header-only is what has been
# measured to compile, and a build file should describe what was tried rather than what ought to
# work. Switching is a `src/*.cpp` list and an `ar` rule in the shape `oops-libogg.mk` has.
ifndef OOPS_SPDLOG_DIR
OOPS_SPDLOG_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_SPDLOG_UPSTREAM ?= $(OOPS_SPDLOG_DIR)/upstream

# `FMT_USE_LOCALE=0` is **not** a preference, it is what makes this compile.
#
# spdlog bundles fmt, and fmt's `thousands_sep_impl` and `decimal_point_impl` reach for
# `std::numpunct<wchar_t>` to find a locale's grouping character. libc++ here is built with
# `_LIBCPP_HAS_WIDE_CHARACTERS 0`, so that specialisation does not exist and the templates fail
# when instantiated. Turning fmt's locale support off is upstream's own switch for a target
# without one, and it costs a thousands separator nothing here uses - the alternative was a second
# patch against fmt, and a flag upstream provides beats a diff we would have to rebase.
#
# The remaining wide-character use, `utf8_to_utf16::str()`, has no such switch and is
# `patches/0001`.
OOPS_SPDLOG_INCLUDE := -I$(OOPS_SPDLOG_UPSTREAM)/include -DFMT_USE_LOCALE=0

# Nothing to build, so nothing to clean - the target exists so that a caller sweeping every
# dependency's clean rule does not have to special-case this one.
.PHONY: spdlog-clean
spdlog-clean:
	@:
