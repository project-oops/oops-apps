/*
 * `fcntl.h`, enough of it for a port that opens files.
 *
 * Added for spdlog, whose `details/os-inl.h` includes it unconditionally on every non-Windows
 * platform. The flags below are FreeBSD's values, because that is the kernel underneath, and
 * `oops/fs.h` already takes the same numbers for its own `open`.
 *
 * **`open` and `fcntl` are declared, not defined here.** A port that only needs the header to
 * exist - which is spdlog when its file sinks are unused - links nothing extra; one that really
 * opens a file gets an undefined symbol at link, which is the honest failure. A payload link
 * ignores unresolved symbols, so the alternative - declaring them and quietly having no
 * definition - would fault on the console instead, which is the trade `oops-sdk/AGENTS.md`
 * warns about.
 *
 * `O_CLOEXEC` is here because spdlog names it. There is no `exec` on this platform for a
 * descriptor to survive, so it is zero rather than a flag the kernel would reject.
 */
#ifndef OOPS_POSIX_FCNTL_H
#define OOPS_POSIX_FCNTL_H

#include <sys/types.h>

#define O_RDONLY   0x0000
#define O_WRONLY   0x0001
#define O_RDWR     0x0002
#define O_ACCMODE  0x0003
#define O_NONBLOCK 0x0004
#define O_APPEND   0x0008
#define O_CREAT    0x0200
#define O_TRUNC    0x0400
#define O_EXCL     0x0800
/* No exec on this platform, so nothing to close across one. */
#define O_CLOEXEC  0x0000
/* **A Linux-ism, and zero is its correct value here.** `O_LARGEFILE` asks a 32-bit `off_t`
 * platform to allow files above 2 GB; FreeBSD's `off_t` is 64-bit and always has been, so the
 * flag does not exist there and asking for it is already the default. StormLib names it
 * unconditionally on every non-Windows platform (`FileStream.cpp:117`). */
#ifndef O_LARGEFILE
#define O_LARGEFILE 0x0000
#endif

#define F_GETFL 3
#define F_SETFL 4
#define F_GETFD 1
#define F_SETFD 2

#ifdef __cplusplus
extern "C" {
#endif

int open(const char *path, int flags, ...);
int fcntl(int fd, int cmd, ...);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_FCNTL_H */
