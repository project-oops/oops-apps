/*
 * `sys/sysctl.h`, for the FreeBSD branch portable code takes on this target.
 *
 * clang defines `__FreeBSD__` for `x86_64-unknown-freebsd`, so a program's `#ifdef __FreeBSD__`
 * arm compiles here - OpenAL Soft's `core/helpers.cpp` is the first, asking `kern.proc.pathname`
 * for the path of its own executable. `sysctl` is defined in `posix.c` and always fails with
 * `ENOENT`; the MIB constants are FreeBSD's, so a caller's array is built exactly as it would be
 * there.
 */
#ifndef OOPS_POSIX_SYS_SYSCTL_H
#define OOPS_POSIX_SYS_SYSCTL_H

#include <stddef.h>

#define CTL_KERN 1
#define KERN_PROC 14
#define KERN_PROC_PATHNAME 12

#ifdef __cplusplus
extern "C" {
#endif

int sysctl(const int *name, unsigned int namelen, void *oldp, size_t *oldlenp,
           const void *newp, size_t newlen);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_SYS_SYSCTL_H */
