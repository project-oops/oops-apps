# bshoshany/thread-pool build integration.
#
#   OOPS_THREADPOOL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/thread-pool)
#   include $(OOPS_THREADPOOL)/oops-thread-pool.mk
#   EXTRA_TARGET_CFLAGS += $(OOPS_THREADPOOL_INCLUDE)
#
# Header-only, so no archive.
#
# **This one is the proof that libc++'s threading works.** `BS::thread_pool` constructs real
# `std::thread`s, holds a `std::mutex` and waits on a `std::condition_variable` - it is not a
# library that merely mentions them. It compiled first try after
# `../libcxx/include/__external_threading` went in, which is a better test of that header than
# anything written to test it directly.
ifndef OOPS_THREADPOOL_DIR
OOPS_THREADPOOL_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_THREADPOOL_UPSTREAM ?= $(OOPS_THREADPOOL_DIR)/upstream
OOPS_THREADPOOL_INCLUDE := -I$(OOPS_THREADPOOL_UPSTREAM)/include

.PHONY: thread-pool-clean
thread-pool-clean:
	@:
