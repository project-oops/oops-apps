/*
 * Directory enumeration, the POSIX way.
 *
 * **This header used to refuse `readdir` on principle, and the principle was wrong.** Extreme
 * Tux Racer only ever calls `opendir` to test whether a directory exists, so the first version
 * declared that pair and nothing else, and argued that declaring `readdir` would let a future
 * title link against nothing - true at the time, since `oops-sdk` had no enumeration.
 *
 * Neverball is that future title and it genuinely walks directories, to list levels, sets and
 * replays. The right answer was never to keep refusing: it was for the SDK to grow the call.
 * `SYS_getdents` had been sitting in `<oops/syscall.h>` the whole time, beside the `SYS_mkdir`
 * that `oops_fs_mkdir` already used. `oops_fs_opendir`/`readdir`/`closedir` now exist, and this
 * is the POSIX spelling over them.
 *
 * `d_name` is the only field here. POSIX guarantees no others, `d_ino` and `d_type` are
 * extensions, and a port reading them would be reading something this shim would have to invent.
 */
#ifndef OOPS_POSIX_DIRENT_H
#define OOPS_POSIX_DIRENT_H

typedef struct OOPS_DIR DIR;

struct dirent {
    char d_name[256];
};

#ifdef __cplusplus
extern "C" {
#endif
DIR *opendir(const char *path);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);

/*
 * **Always NULL, with `EBADF`, and this one cannot be made to work.**
 *
 * `fdopendir` turns an open *directory* descriptor into a `DIR *`. There are no directory
 * descriptors on this platform - `oops_fs_open` cannot open a directory at all, which is why
 * `openat`'s only usable anchor is `AT_FDCWD` - so there is no descriptor to hand it and nothing
 * for it to convert. Unlike the other refusals here, this is not a facility that could be added
 * behind the same signature; it would need directory descriptors first.
 *
 * libc++'s `src/filesystem/operations.cpp` calls it on the `openat` path when walking a tree and
 * reports the error to its caller.
 */
DIR *fdopendir(int fd);
#ifdef __cplusplus
}
#endif

#endif
