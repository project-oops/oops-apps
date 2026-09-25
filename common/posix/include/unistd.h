/*
 * The POSIX calls a port makes from <unistd.h>, over `oops/fs.h`.
 *
 * `getcwd`/`chdir` are a pair used by ETR's `DirExistsWin`, which tests a directory by trying to
 * enter it. A payload has one working directory and no way to change it, so `chdir` answers
 * whether the target exists and changes nothing - which is the only thing the caller does with
 * it.
 *
 * **This header shadows `oops-deps/sdl2/include/unistd.h` whenever a title includes both**, and
 * that has already cost one build: the SDL one exists solely to declare `_exit` for `SDL.c`, and
 * a title using this shim put `common/posix/include` on the command line ahead of it, so the
 * declaration disappeared. Nothing said so until the SDL archive was next rebuilt from scratch,
 * by which time the change that caused it was days old - and clang 21 turned what had been an
 * implicit-declaration warning into an error, which is the only reason it was caught rather than
 * linked. Whatever the SDL header declares, this one has to declare too.
 */
#ifndef OOPS_ETR_UNISTD_H
#define OOPS_ETR_UNISTD_H
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>

#define F_OK 0
#define R_OK 4

/* **Guarded, because `stdio.h` declares these too.** Both headers are expected to carry them and a
 * program may include either first; the values are universal, so a redefinition would be harmless
 * and a *conflicting* one impossible - but clang warns on the redefinition regardless, and a
 * warning here becomes an error under a port's own `-Werror`. */
#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

#ifdef __cplusplus
extern "C" {
#endif
char *getcwd(char *buf, size_t size);
int chdir(const char *path);
int access(const char *path, int mode);
uid_t getuid(void);
/* Also 0 - one identity here, so there is no real/effective distinction to report. */
uid_t geteuid(void);

/* `_exit` is `exit` here, and that is not an approximation: `oops-sdk`'s `exit` goes straight to
 * the platform's `SYS_exit` with no atexit list and no return, which is `_exit`'s contract. */
static inline void _exit(int status) { exit(status); }

/* Descriptor I/O, over the SDK's `oops_fs_*`. A count, or -1 with `errno` - which is `EIO` for
 * any platform refusal, because the SDK answers a sign rather than a reason and guessing between
 * `EBADF` and `ENOSPC` would be inventing detail. */
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);
/* The new offset, or -1. `off_t` is 64-bit here, so there is no `lseek64` to be a different
 * function - a port naming it wants a `#define` to this one, which is what StormLib's patch does. */
off_t lseek(int fd, off_t offset, int whence);

/* Always 0. Unlike POSIX's, this accepts a value of a million or more rather than failing with
 * `EINVAL` - see the definition in `posix.c` for why that rule does not apply here. */
int usleep(useconds_t microseconds);
/* Always 0: the return is "seconds left if a signal interrupted this", and nothing here can. */
unsigned int sleep(unsigned int seconds);
/* **Always fails with `ENOSYS`.** The SDK's filesystem cannot resize a file, and there is no
 * honest way to report otherwise - see `posix.c`. */
int ftruncate(int fd, off_t length);

/* One process, so a constant - see the definition in `posix.c` for why that is the truth here
 * rather than a stand-in. */
int getpid(void);
/* Always 0: a payload's output is the kernel log, never a terminal, so a library asking this
 * before emitting colour escapes gets the answer that keeps them out of the log. */
int isatty(int fd);
/* Nothing is buffered behind a descriptor here, so there is nothing to force out. */
int fsync(int fd);
#ifdef __cplusplus
}
#endif

#endif
