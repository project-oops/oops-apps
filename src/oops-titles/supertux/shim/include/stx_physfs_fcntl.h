/*
 * The two `fcntl` names PhysFS's `__PHYSFS_platformFlush` uses, forced into the title's
 * C objects.
 *
 * Both are in `common/posix/include/fcntl.h`, and `fcntl` is defined in
 * `common/posix/posix.c`, but oops-sdk's libc `<fcntl.h>` comes first on the path and
 * declares `open` without `fcntl`. Putting the POSIX shim first would also put its
 * minimal `<sys/types.h>` ahead of the SDK's, which loses `useconds_t`.
 */
#ifndef STX_PHYSFS_FCNTL_H
#define STX_PHYSFS_FCNTL_H

#ifndef F_GETFL
#define F_GETFL 3
#endif

int fcntl(int fd, int cmd, ...);

#endif
