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
#endif
