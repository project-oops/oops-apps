/*
 * `statvfs`, and it **always fails**.
 *
 * # Why the header exists at all
 *
 * Bugdom's Pomme bundles `ghc::filesystem`, a `std::filesystem` stand-in for compilers without
 * one. Its BSD branch includes this header unconditionally - `filesystem_implementation.hpp:175` -
 * so eight of Bugdom's C++ sources stop at the include whether or not anything calls the function.
 *
 * # Why the function fails
 *
 * There is nothing behind it. `oops/fs.h` answers existence, size and directory-ness; nothing in
 * the SDK reports a filesystem's capacity or its free blocks, and the console's storage is not a
 * mounted volume a payload can interrogate. Filling the structure with plausible numbers would be
 * inventing them, and the numbers are exactly the thing a caller asks this for.
 *
 * **The one caller handles it.** `ghc::filesystem`'s `space()` is the only user in this tree, and
 * a non-zero return there sets an error code and returns `-1` for all three figures - which is
 * what "this platform does not know" looks like in that interface. A program that asks how much
 * room is left is told it cannot find out, rather than being told a number.
 *
 * The field layout is FreeBSD's, because a program that computes `f_blocks * f_frsize` should be
 * reading the members it thinks it is.
 */
#ifndef OOPS_POSIX_SYS_STATVFS_H
#define OOPS_POSIX_SYS_STATVFS_H

#include <stdint.h>
#include <sys/types.h>

typedef uint64_t fsblkcnt_t;
typedef uint64_t fsfilcnt_t;

struct statvfs {
    unsigned long f_bavail;  /* free blocks available to a non-privileged caller */
    unsigned long f_bfree;   /* free blocks */
    unsigned long f_blocks;  /* total blocks */
    unsigned long f_favail;  /* free inodes available to a non-privileged caller */
    unsigned long f_ffree;   /* free inodes */
    unsigned long f_files;   /* total inodes */
    unsigned long f_bsize;   /* preferred I/O size */
    unsigned long f_flag;
    unsigned long f_frsize;  /* fundamental block size */
    unsigned long f_fsid;
    unsigned long f_namemax; /* longest component name */
};

/* `ST_RDONLY` and `ST_NOSUID` are the two `f_flag` bits POSIX defines. Named because a program
 * testing them should compile; `f_flag` is never filled, because the call never succeeds. */
#define ST_RDONLY 0x1
#define ST_NOSUID 0x2

#ifdef __cplusplus
extern "C" {
#endif

/* **Always -1, with `errno` set to `ENOSYS`.** See the note above: there is no capacity to report
 * and a made-up one would be worse than none. `*buf` is left untouched. */
int statvfs(const char *path, struct statvfs *buf);
int fstatvfs(int fd, struct statvfs *buf);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_SYS_STATVFS_H */
