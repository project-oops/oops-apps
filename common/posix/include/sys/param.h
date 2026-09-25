/*
 * `sys/param.h`, which BSD code includes for its path-length limit. PhysFS's
 * `physfs_platform_unix.c` is the first here, on the `__FreeBSD__` branch clang selects for this
 * target.
 *
 * 1024 is FreeBSD's `MAXPATHLEN`, and `PATH_MAX` is the same number there.
 */
#ifndef OOPS_POSIX_SYS_PARAM_H
#define OOPS_POSIX_SYS_PARAM_H

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif
#ifndef PATH_MAX
#define PATH_MAX MAXPATHLEN
#endif

#endif /* OOPS_POSIX_SYS_PARAM_H */
