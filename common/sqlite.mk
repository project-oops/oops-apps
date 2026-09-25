# SQLite's platform layer, for titles that vendor SQLite. Include from a title's Makefile:
#
#   include $(OOPS_APPS_ROOT)/common/sqlite.mk
#
#   EXTRA_TARGET_CFLAGS += $(OOPS_SQLITE_INCLUDE) $(OOPS_SQLITE_DEFS) \
#                          -DOOPS_SQLITE_TEMP_DIR=\"/data/my-title\"
#   PAYLOAD_SRCS        += $(OOPS_SQLITE_SRCS)
#
# **The title vendors SQLite itself; this is only the platform layer.** SQLite ships as a single
# amalgamated `sqlite3.c` that programs bundle rather than link against, and the two titles that
# might want it would not agree on the version. So `sqlite3.c` stays where the port put it, and
# this supplies the part that is about *this machine*: a VFS over `oops/fs.h`, a mutex
# implementation over `oops/thread.h`, and the `sqlite3_os_init` hook that registers them.
#
# `common/sqlite/sqlite_oops.c` says why the unix backend is removed rather than shimmed, and
# which three operations this platform cannot perform.

ifndef OOPS_SQLITE_DIR
OOPS_SQLITE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))/sqlite
endif

OOPS_SQLITE_INCLUDE := -I$(OOPS_SQLITE_DIR)/include
OOPS_SQLITE_SRCS := $(OOPS_SQLITE_DIR)/sqlite_oops.c

# **The defines are part of the contract, not tuning.** Each one is what makes the platform layer
# next to it the whole truth about this machine:
#
#   SQLITE_OS_OTHER=1     removes the unix backend, so `sqlite3_os_init` here is the only one.
#                         Without it SQLite compiles its own and wants `mmap`, `fchown`, POSIX
#                         record locks and a `struct stat` with meaningful `st_ino`/`st_nlink`.
#   SQLITE_THREADSAFE=1   serialised. A port driving one connection from two threads needs it, and
#                         the mutex methods in the platform layer are real.
#   SQLITE_OMIT_WAL       write-ahead logging needs shared memory between processes, which is what
#                         `xShmMap` is for and what this VFS does not implement. Omitting it means
#                         asking for `journal_mode=WAL` is refused rather than half-working.
#   SQLITE_OMIT_LOAD_EXTENSION   there is no dynamic loading here; the VFS's `xDlOpen` is NULL.
#   SQLITE_OMIT_DATETIME_FUNCS   `oops-sdk`'s `<time.h>` deliberately has no `localtime`, because
#                         a port formatting a date with this clock "will print nonsense, so it
#                         should not". SQLite's date functions want it. Omitting them is the
#                         honest switch; the alternative is a `localtime` that lies.
#   SQLITE_TEMP_STORE=2   temporary tables in memory by default. The VFS can make a temporary file
#                         if asked, but memory is faster and this avoids needing a writable
#                         scratch directory for the common case.
OOPS_SQLITE_DEFS := -DSQLITE_OS_OTHER=1 \
                    -DSQLITE_THREADSAFE=1 \
                    -DSQLITE_OMIT_WAL \
                    -DSQLITE_OMIT_LOAD_EXTENSION \
                    -DSQLITE_OMIT_DATETIME_FUNCS \
                    -DSQLITE_TEMP_STORE=2

# Where a temporary file goes, and what a relative path is resolved against. A title sets it; the
# default names no title, which is the intended nudge.
OOPS_SQLITE_TEMP_DIR ?= /data
OOPS_SQLITE_DEFS += -DOOPS_SQLITE_TEMP_DIR=\"$(OOPS_SQLITE_TEMP_DIR)\"
