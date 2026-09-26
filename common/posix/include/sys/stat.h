/*
 * `stat` for the one thing ETR asks it: how big is this file, and is it a directory.
 * `FileExists` calls it and reads `st_size`; the mode macros are here because the header that
 * declares `stat` is expected to carry them.
 */
#ifndef OOPS_ETR_SYS_STAT_H
#define OOPS_ETR_SYS_STAT_H
#include <sys/types.h>
#include <time.h> /* time_t, for the timestamps below */

#define S_IFMT   0170000u
#define S_IFDIR  0040000u
#define S_IFREG  0100000u
#define S_IFLNK  0120000u
#define S_IFIFO  0010000u
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
/* Never true, for the same reason `S_ISLNK` never is: `stat` below reports a file or a directory
 * and nothing else. Named because ioquake3's `sys_unix.c:320` refuses to treat a FIFO as a game
 * file, and has to be able to ask. */
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
/* Never true: `stat` below reports a file or a directory and nothing else. Named because PhysFS
 * asks it of every path it stats. */
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)

/* **The permission bits, which this platform does not enforce but programs still name.** A
 * caller passes them to `open` as the mode for a file it may create - StormLib's
 * `FileStream.cpp:117` is the first here - and the call has to compile whether or not anything
 * downstream honours them. The values are the universal octal ones. */
#define S_IRUSR 0000400u
#define S_IWUSR 0000200u
#define S_IXUSR 0000100u
#define S_IRWXU 0000700u
#define S_IRGRP 0000040u
#define S_IWGRP 0000020u
#define S_IXGRP 0000010u
#define S_IRWXG 0000070u
#define S_IROTH 0000004u
#define S_IWOTH 0000002u
#define S_IXOTH 0000001u
#define S_IRWXO 0000007u

struct stat {
    mode_t st_mode;
    off_t  st_size;
    /* **The timestamps, and they are always zero.** The SDK's filesystem answers existence and
     * size and has no call for a modification time, so there is nothing truthful to put here.
     * They are present because programs name them - StormLib reads `st_mtime` at
     * `FileStream.cpp:190` to stamp an archive - and a field that does not exist is a build
     * failure where a zero is a date of 1970. Neither is right; the zero is the one that lets a
     * port run, and it is documented rather than hidden so that a caller depending on file times
     * knows to look here first. */
    time_t st_mtime;
    time_t st_atime;
    time_t st_ctime;
};

#ifdef __cplusplus
extern "C" {
#endif
int stat(const char *path, struct stat *out);
/* **`stat` itself**, because there are no symbolic links on the console's filesystem for the
 * difference to be about. PhysFS calls it when told not to follow links. */
int lstat(const char *path, struct stat *out);
/* By descriptor. Only `st_size` and `st_mode` are filled, which is what `stat` above answers too
 * - see `posix.c` for how the size is taken without disturbing the file pointer. */
int fstat(int fd, struct stat *out);
int mkdir(const char *path, mode_t mode);
/* **Always fails with `ENOSYS`.** There are no FIFOs on this filesystem, which is also why
 * `S_ISFIFO` above is never true. ioquake3's `sys_unix.c` creates one for a pipe-based console it
 * does not use here; it checks the return. */
int mkfifo(const char *path, mode_t mode);
/* **Always fails with `ENOSYS`.** There are no file permissions on this platform to change - see
 * `posix.c`, and `ftruncate` in `unistd.h` for the same reasoning. */
int chmod(const char *path, mode_t mode);
#ifdef __cplusplus
}
#endif

#endif
