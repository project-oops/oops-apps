#ifndef OOPS_ETR_SYS_TIME_H
#define OOPS_ETR_SYS_TIME_H
#include <sys/types.h>
/* **Guarded against `oops-sdk/include/libc/sys/time.h`**, which declares the same structure and
 * is the copy a compile normally reaches. The two guards differ, so without this a translation
 * unit that saw both would define `struct timeval` twice. See the note in the SDK's copy. */
#ifndef _TIMEVAL_DECLARED
#define _TIMEVAL_DECLARED
struct timeval { long tv_sec; long tv_usec; };
#endif
#ifdef __cplusplus
extern "C" {
#endif
int gettimeofday(struct timeval *tv, void *tz);

/*
 * **Setting a file's times, and it always fails.**
 *
 * The SDK's filesystem has no call for it - which is the same absence `sys/stat.h` records from
 * the reading side, where all three timestamps are zero. A `utimes` that returned success would
 * claim the time had been set on a file whose time cannot even be read back, so the failure is
 * the only answer that does not mislead.
 *
 * libc++'s `src/filesystem/operations.cpp` is the caller: `last_write_time(p, t)` is how a
 * program sets one, and it propagates the error to its own caller. Declared here because POSIX
 * puts it in this header and because, without the declaration, three of libc++'s filesystem
 * sources do not compile at all - which is a worse failure than one that reports itself.
 *
 * `errno` is `ENOSYS`.
 */
int utimes(const char *path, const struct timeval times[2]);
#ifdef __cplusplus
}
#endif
#endif
