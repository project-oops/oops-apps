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
/* Named by PhysFS, which asks `access(dir, W_OK)` of a write directory and `X_OK` of a path it
 * might execute. `access` answers existence for every mode - see `posix.c`. */
#define W_OK 2
#define X_OK 1

/* The three descriptors every process starts with. They are not fictional here - `oops-sdk`'s
 * `write` to 1 and 2 reaches the kernel log - but there is nothing on 0 to read, which is why
 * `isatty` below answers 0 and a program's interactive-console branch never runs. */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

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
 * honest way to report otherwise - see `posix.c`. `truncate` is the same answer by path; libc++'s
 * `src/filesystem/operations.cpp` calls it from `resize_file()`, and reports the error. */
int ftruncate(int fd, off_t length);
int truncate(const char *path, off_t length);

/*
 * **Always fails with `ENOSYS`, which is what a kernel without the call returns** - and that is
 * the whole point of providing it.
 *
 * `copy_file_range` is a Linux 4.5 / FreeBSD 13 syscall that copies between descriptors without
 * going through userspace. libc++'s `src/filesystem/operations.cpp:52` enables it for any
 * `__FreeBSD__` target, which this is, so its `copy_file_impl` calls it - and at line 341 falls
 * through to a portable read-and-write copy when it fails. Refusing therefore costs a copy no
 * speed it could have had, and the alternative was patching libc++'s platform detection.
 */
ssize_t copy_file_range(int infd, off_t *inoffp, int outfd, off_t *outoffp,
                        size_t len, unsigned int flags);

/* One process, so a constant - see the definition in `posix.c` for why that is the truth here
 * rather than a stand-in. */
int getpid(void);
/* Always 0: a payload's output is the kernel log, never a terminal, so a library asking this
 * before emitting colour escapes gets the answer that keeps them out of the log. */
int isatty(int fd);
/* Nothing is buffered behind a descriptor here, so there is nothing to force out. */
int fsync(int fd);
/* **Always fails with `EINVAL`**, which is POSIX's answer for "not a symbolic link" - and there
 * are none on this filesystem. PhysFS and OpenAL Soft both try `/proc/self/exe` to find their own
 * binary, and both fall back when this fails. */
ssize_t readlink(const char *path, char *buf, size_t size);

/*
 * **Hard links and symbolic links, both always failing with `ENOSYS`.**
 *
 * There are neither on this filesystem, which is the same finding `readlink` above and
 * `sys/stat.h`'s `S_ISLNK` record from the other side. A caller is told so rather than being given
 * a copy of the file under the second name, which is what a "helpful" implementation would do and
 * which diverges the moment either name is written to.
 */
int link(const char *oldpath, const char *newpath);
int symlink(const char *target, const char *linkpath);

/*
 * The `*at()` form of `unlink`. `AT_FDCWD` is the only anchor this platform has - `fcntl.h` says
 * why - and for it this is `unlink`. `AT_REMOVEDIR` asks it to remove a *directory* instead, and
 * that fails with `ENOSYS`: the SDK has `oops_fs_unlink` and no `rmdir`, the same gap
 * `SDL_SYS_RemovePath` records.
 */
int unlinkat(int dirfd, const char *path, int flags);

/*
 * **Configurable limits, and only `_PC_PATH_MAX` has an answer.**
 *
 * `pathconf` asks a *path* what its filesystem permits. This one permits what `sys/param.h` says -
 * 1024, FreeBSD's `MAXPATHLEN` - and has nothing to say about the rest: no name-length limit it
 * publishes, no link maximum worth reporting when there are no links.
 *
 * -1 **without setting `errno`** is POSIX's way of saying "this limit is indeterminate", which is
 * different from -1 with `errno` set, meaning the call failed. `ghc::filesystem` asks
 * `_PC_PATH_MAX` and falls back to a built-in when told nothing, so either answer works for it -
 * but the distinction is the interface's and is kept.
 */
#define _PC_LINK_MAX          1
#define _PC_NAME_MAX          4
#define _PC_PATH_MAX          5
#define _PC_PIPE_BUF          6
#define _PC_NO_TRUNC          8
long pathconf(const char *path, int name);
long fpathconf(int fd, int name);

/*
 * **Both always fail, and that is the useful answer.** A payload is one process: there is no second
 * one for `fork` to produce and no program image for `execvp` to replace it with. Failing is not a
 * shortfall here, it is the truth, and the callers are written for it - ioquake3's `Sys_Exec` reads
 * `if (pid < 0) return -1`, which is how it declines to put up the `zenity` error dialog it would
 * otherwise have spawned.
 *
 * `errno` is `ENOSYS`. `fork` returning 0 - the child's answer - is the one thing that must never
 * happen, because the caller would then run the child branch in the only process there is.
 */
pid_t fork(void);
int execvp(const char *file, char *const argv[]);

/*
 * **The console's own IP address, in dotted-quad form, or a failure.**
 *
 * This platform has no hostname. There is no `/etc/hostname`, no `sysctl kern.hostname` this shim
 * can reach, and `oops/netctl.h` - which is where the network identity lives - reports an address, a
 * netmask, a gateway and a MAC, and no name.
 *
 * So rather than invent one, this answers with the address, because **the thing a caller does with
 * the result is hand it to a resolver.** ioquake3's `NET_GetLocalAddress` is the case in point: it
 * calls `gethostname`, feeds the answer to `getaddrinfo`, and keeps the addresses that come back as
 * "this machine's". Given the address it gets that right. Given an invented name like "ps5" it would
 * get `EAI_NONAME` and conclude the machine has no addresses at all.
 *
 * The compromise, stated plainly: a caller that *prints* this, or compares it to a configured name,
 * sees an address where it expected a name. Nothing in this tree does either.
 *
 * -1 with `ENOSYS` when `oops_net_ctl_get_info` cannot answer - no network, no address, and no
 * pretending otherwise. `ENAMETOOLONG` if the buffer is too small, as POSIX says.
 */
int gethostname(char *name, size_t len);
#ifdef __cplusplus
}
#endif

#endif
