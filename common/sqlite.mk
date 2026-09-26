# SQLite's platform layer, for titles that vendor SQLite. Include from a title's Makefile:
#
#   include $(OOPS_APPS_ROOT)/common/sqlite.mk
#
#   EXTRA_TARGET_CFLAGS += $(OOPS_SQLITE_INCLUDE) $(OOPS_SQLITE_DEFS) \
#                          -DOOPS_SQLITE_TEMP_DIR=\"/data/my-title\"
#   PAYLOAD_SRCS        += $(OOPS_SQLITE_SRCS)
#
# The title vendors its own `sqlite3.c`; this supplies a VFS over `oops/fs.h`, mutexes over
# `oops/thread.h`, and the `sqlite3_os_init` that registers them (`common/sqlite/sqlite_oops.c`).

ifndef OOPS_SQLITE_DIR
OOPS_SQLITE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))/sqlite
endif

OOPS_SQLITE_INCLUDE := -I$(OOPS_SQLITE_DIR)/include
OOPS_SQLITE_SRCS := $(OOPS_SQLITE_DIR)/sqlite_oops.c

# The defines the platform layer depends on:
#   SQLITE_OS_OTHER=1     no unix backend; `sqlite3_os_init` here is the only one.
#   SQLITE_THREADSAFE=1   serialised; the platform layer's mutexes are real.
#   SQLITE_OMIT_WAL       the VFS has no `xShmMap`, so `journal_mode=WAL` is refused.
#   SQLITE_OMIT_LOAD_EXTENSION   no dynamic loading; the VFS's `xDlOpen` is NULL.
#   SQLITE_OMIT_DATETIME_FUNCS   oops-sdk's `<time.h>` has no `localtime`.
#   SQLITE_TEMP_STORE=2   temporary tables in memory, so no scratch directory is needed.
OOPS_SQLITE_DEFS := -DSQLITE_OS_OTHER=1 \
                    -DSQLITE_THREADSAFE=1 \
                    -DSQLITE_OMIT_WAL \
                    -DSQLITE_OMIT_LOAD_EXTENSION \
                    -DSQLITE_OMIT_DATETIME_FUNCS \
                    -DSQLITE_TEMP_STORE=2

# Where a temporary file goes, and what a relative path resolves against. A title sets it to
# its own data directory.
OOPS_SQLITE_TEMP_DIR ?= /data
OOPS_SQLITE_DEFS += -DOOPS_SQLITE_TEMP_DIR=\"$(OOPS_SQLITE_TEMP_DIR)\"
