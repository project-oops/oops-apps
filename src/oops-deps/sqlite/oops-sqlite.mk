# SQLite build integration.
#
#   OOPS_SQLITE ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sqlite)
#   include $(OOPS_SQLITE)/oops-sqlite.mk
#   EXTRA_TARGET_LDFLAGS += $(OOPS_SQLITE_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_SQLITE_LIB)
#
# The amalgamation, and `oops_vfs.c`: SQLite built with `SQLITE_OS_OTHER`, its own switch for a
# platform it has no layer for, and the VFS and mutexes this platform answers with. The header of
# `oops_vfs.c` says what that layer does and why locking is a no-op here.
#
# The options are SQLite's documented compile-time switches:
#   SQLITE_OS_OTHER, SQLITE_MUTEX_APPDEF   the platform layer is `oops_vfs.c`
#   SQLITE_THREADSAFE=1                    serialized: a title may use it from several threads
#   SQLITE_OMIT_WAL                        WAL needs shared memory the VFS does not offer
#   SQLITE_OMIT_LOAD_EXTENSION             no `dlopen`
#   SQLITE_DEFAULT_MEMSTATUS=0             no global allocation counters
#   SQLITE_TEMP_STORE=2                    temporary tables in memory unless asked otherwise
ifndef OOPS_SQLITE_MK
OOPS_SQLITE_MK := 1

ifndef OOPS_SQLITE_DIR
OOPS_SQLITE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_SQLITE_UPSTREAM ?= $(OOPS_SQLITE_DIR)/upstream/sqlite-amalgamation-3530400
OOPS_SQLITE_BUILD ?= $(OOPS_SQLITE_DIR)/build
OOPS_SQLITE_INCLUDE := -I$(OOPS_SQLITE_UPSTREAM)
OOPS_SQLITE_LIB := $(OOPS_SQLITE_BUILD)/libsqlite3.a
OOPS_SQLITE_LDFLAGS := $(OOPS_SQLITE_LIB)
OOPS_SQLITE_SRCS := $(OOPS_SQLITE_UPSTREAM)/sqlite3.c $(OOPS_SQLITE_DIR)/oops_vfs.c
OOPS_SQLITE_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                     -nostdlibinc -fPIC -O2 -w \
                     -DSQLITE_OS_OTHER=1 -DSQLITE_MUTEX_APPDEF=1 -DSQLITE_THREADSAFE=1 \
                     -DSQLITE_OMIT_WAL=1 -DSQLITE_OMIT_LOAD_EXTENSION=1 \
                     -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_TEMP_STORE=2 \
                     $(OOPS_SQLITE_INCLUDE) $(OOPS_POSIX_INCLUDE) \
                     $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

$(OOPS_SQLITE_LIB): $(OOPS_SQLITE_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_SQLITE_BUILD)
	@rm -f $@
	@n=0; objs=""; for s in $(OOPS_SQLITE_SRCS); do n=$$((n+1)); o=$(OOPS_SQLITE_BUILD)/sq$$n.o; \
	   $(TARGET_CC) $(OOPS_SQLITE_CFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 echo "sqlite: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs
	@echo "sqlite: $@"

.PHONY: sqlite-clean
sqlite-clean:
	@rm -rf $(OOPS_SQLITE_BUILD)

endif
