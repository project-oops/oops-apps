/*
 * `utime`, the older form of `utimes` in `<sys/time.h>`, and it fails the same way: the SDK's
 * filesystem cannot set a file's times, so the answer is -1 with `ENOSYS` rather than a success
 * nothing could read back. Torch's `libmio0` calls it to touch a file and ignores the result.
 */
#ifndef OOPS_POSIX_UTIME_H
#define OOPS_POSIX_UTIME_H

#include <sys/types.h>

struct utimbuf {
    time_t actime;
    time_t modtime;
};

#ifdef __cplusplus
extern "C" {
#endif
int utime(const char *path, const struct utimbuf *times);
#ifdef __cplusplus
}
#endif

#endif
