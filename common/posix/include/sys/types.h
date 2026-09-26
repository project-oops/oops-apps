#ifndef OOPS_ETR_SYS_TYPES_H
#define OOPS_ETR_SYS_TYPES_H
#include <stddef.h>
#include <stdint.h>
typedef int32_t  uid_t;
typedef int32_t  gid_t;
typedef int64_t  off_t;
/* What `read` and `write` answer: a count, or -1. Signed and pointer-width, which on this LP64
 * target is 64 bits - the same width `oops_fs_read` returns, so the shim's conversion is a
 * rename rather than a narrowing. */
#ifndef OOPS_HAVE_SSIZE_T
#define OOPS_HAVE_SSIZE_T 1
typedef long ssize_t;
#endif
typedef uint32_t mode_t;
typedef int64_t  time_t_etr;

/*
 * **The same three `oops-sdk/include/libc/sys/types.h` declares, under the same guards.**
 *
 * There are two `<sys/types.h>` on a title's include path and which one a given compile reaches
 * is not worth relying on: the SDK's is meant to win - it sits at include position 24 against this
 * file's 37 - and Neverball reached *this* one, which is how the three names came to be missing
 * after they were added to the other. Both now say the same thing, and the `_*_DECLARED` guards
 * mean whichever arrives second is a no-op rather than a redefinition.
 *
 * The widths are FreeBSD's. `sys/stat.h` here explains why all three are always zero.
 */
#ifndef _DEV_T_DECLARED
typedef uint64_t dev_t;
#define _DEV_T_DECLARED
#endif

#ifndef _INO_T_DECLARED
typedef uint64_t ino_t;
#define _INO_T_DECLARED
#endif

#ifndef _NLINK_T_DECLARED
typedef uint64_t nlink_t;
#define _NLINK_T_DECLARED
#endif

/* `usleep`'s argument, which `unistd.h` here declares with. 32-bit on FreeBSD and Linux, and not
 * the unsigned twin of `suseconds_t`. */
#ifndef _USECONDS_T_DECLARED
typedef unsigned int useconds_t;
#define _USECONDS_T_DECLARED
#endif

/* The remaining three the SDK's copy has, at its widths. A port reaching this file rather than that
 * one gets the same set either way, which is the point of the guards. */
#ifndef _PID_T_DECLARED
typedef int32_t pid_t;
#define _PID_T_DECLARED
#endif

#ifndef _TIME_T_DECLARED
typedef int64_t time_t;
#define _TIME_T_DECLARED
#endif

#ifndef _SUSECONDS_T_DECLARED
typedef int64_t suseconds_t;
#define _SUSECONDS_T_DECLARED
#endif

#endif
