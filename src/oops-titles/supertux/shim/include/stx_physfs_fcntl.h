/*
 * The two `fcntl` names PhysFS's `__PHYSFS_platformFlush` uses, forced into the title's C
 * objects.
 *
 * Both are in `common/posix/include/fcntl.h`, and `fcntl` is defined in `common/posix/posix.c`.
 * The trouble is only which `<fcntl.h>` is found: oops-sdk's libc now has one too, earlier on the
 * path, declaring `open` and not `fcntl`. Putting the POSIX shim first instead was tried and is
 * worse - it also puts the shim's minimal `<sys/types.h>` ahead of the SDK's and loses
 * `useconds_t`. So the two names are supplied here and the path is left as `app.mk` has it.
 *
 * Retire this when one `<fcntl.h>` carries both.
 */
#ifndef STX_PHYSFS_FCNTL_H
#define STX_PHYSFS_FCNTL_H

#ifndef F_GETFL
#define F_GETFL 3
#endif

int fcntl(int fd, int cmd, ...);

#endif
