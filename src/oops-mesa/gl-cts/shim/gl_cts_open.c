/*
 * `open()`, routed through the SDK's file layer so that `fopen` and everything above it works.
 *
 * dEQP writes its result log with `fopen`/`fprintf`/`fflush`/`fclose` (`qphelper/qpTestLog.c`),
 * and on this platform that produced a 0-byte file: the descriptor libc's `open()` returns
 * accepts writes, reports success at every step - `fwrite` full count, `fflush` 0, `fclose` 0,
 * `fsync` 0, errno never set - and stores nothing. Measured on hardware 2026-09-25, with the
 * read-back done in the same process at the same path, so it is the descriptor and not a
 * namespace.
 *
 * `oops_fs_open` opens the same path with a direct `SYS_open` and that descriptor works;
 * `oops_fs_write` then writes to it with plain libc `write`. So the write path was never the
 * problem and there is nothing to replace above this - one function is the whole fix, and the C
 * library's buffering, formatting and `FILE` handling all keep working on top of a descriptor
 * that stores what it is given.
 *
 * This is the SDK doing its job rather than a workaround: `oops/fs.h` is the file API for this
 * platform, and the only reason dEQP does not call it is that dEQP is somebody else's program.
 * Defining `open` here is how somebody else's program reaches it without being modified.
 *
 * Filed as `oops-sdk REQ-20260925T1936Z-6c8d`, because a silent-success write path catches every
 * ported program that saves a file and `fs_open_raw`'s reason for avoiding libc was undocumented.
 * If the SDK takes the shim, this file goes.
 */
#include <fcntl.h>
#include <stdarg.h>

#include "oops/fs.h"

/*
 * FreeBSD's `<fcntl.h>` values, which are what a caller in this title passes, mapped to the
 * SDK's. They agree numerically today - both are the BSD set - but they are separate constants
 * and translating is what makes that a fact rather than an assumption.
 */
int open(const char *path, int flags, ...)
{
    int oflags = 0;

    switch (flags & O_ACCMODE) {
    case O_WRONLY:
        oflags |= OOPS_O_WRONLY;
        break;
    case O_RDWR:
        oflags |= OOPS_O_RDWR;
        break;
    default:
        oflags |= OOPS_O_RDONLY;
        break;
    }

    if (flags & O_CREAT)
        oflags |= OOPS_O_CREAT;
    if (flags & O_TRUNC)
        oflags |= OOPS_O_TRUNC;
    if (flags & O_APPEND)
        oflags |= OOPS_O_APPEND;

    int mode = 0666;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }

    return oops_fs_open(path, oflags, mode);
}
