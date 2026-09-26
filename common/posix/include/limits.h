/*
 * `<limits.h>`, for the POSIX limits the compiler's own copy does not carry.
 *
 * # Why this exists beside clang's
 *
 * `limits.h` is one of the few headers a *compiler* provides: clang ships one in its resource
 * directory with `INT_MAX`, `CHAR_BIT` and the rest, and that copy is found even under
 * `-nostdlibinc` because the resource directory is not a standard library directory. What it does
 * not have is `PATH_MAX`, `NAME_MAX` and their kin - those are the C library's, and on a real
 * system clang's header reaches them by including the system's.
 *
 * So this defines the POSIX half and hands off to clang's for the C half with `#include_next`.
 * Anything else - redefining `INT_MAX` here, or leaving the include out - would either collide
 * with the compiler or lose every limit it does carry.
 *
 * # `PATH_MAX`
 *
 * 1024, which is FreeBSD's, and the same number `sys/param.h` gives as `MAXPATHLEN`. It is here as
 * well as there because POSIX puts it in this header and `sys/param.h` is the BSD spelling: libc++'s
 * `src/filesystem/operations.cpp` asks for it by the POSIX name, and asked before this file
 * existed.
 */
#ifndef OOPS_POSIX_LIMITS_H
#define OOPS_POSIX_LIMITS_H

/* The compiler's, for `INT_MAX` and everything else that is C's rather than POSIX's. */
#include_next <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

/* A single component's length, which is the kernel's own 255 - the same number
 * `oops_dirent_t.name` is sized against in `oops/fs.h`, where the comment says so. */
#ifndef NAME_MAX
#define NAME_MAX 255
#endif

/* **The number of links a file may have, and here it is 1.** There are no hard links on this
 * filesystem - `unistd.h`'s `link` refuses - so a name is a file and there is never a second one.
 * A program sizing a loop by this gets 1 rather than a desktop's 32767. */
#ifndef LINK_MAX
#define LINK_MAX 1
#endif

#endif /* OOPS_POSIX_LIMITS_H */
