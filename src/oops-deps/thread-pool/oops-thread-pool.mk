# bshoshany/thread-pool build integration.
#
#   OOPS_THREADPOOL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/thread-pool)
#   include $(OOPS_THREADPOOL)/oops-thread-pool.mk
#   EXTRA_TARGET_CFLAGS += $(OOPS_THREADPOOL_INCLUDE)
#
# Header-only, so no archive. `BS::thread_pool` uses `std::thread`, `std::mutex` and
# `std::condition_variable`, provided through `../libcxx/include/__external_threading`.
ifndef OOPS_THREADPOOL_DIR
OOPS_THREADPOOL_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_THREADPOOL_UPSTREAM ?= $(OOPS_THREADPOOL_DIR)/upstream
OOPS_THREADPOOL_INCLUDE := -I$(OOPS_THREADPOOL_UPSTREAM)/include

.PHONY: thread-pool-clean
thread-pool-clean:
	@:
