/*
 * `strings.h` - the case-insensitive comparisons, which live here rather than in `<string.h>`.
 *
 * POSIX puts `strcasecmp` and `strncasecmp` in this header and not the C one, so a portable
 * program includes it by name; StormLib's `StormPort.h:376` is the first here to do so. The
 * split is historical and every platform keeps it.
 *
 * Defined in `common/posix/posix.c`. The SDK's `<string.h>` has neither, and adding them there
 * instead would put a POSIX function in the C library's header - which is where the next reader
 * would not look for it.
 */
#ifndef OOPS_POSIX_STRINGS_H
#define OOPS_POSIX_STRINGS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int strcasecmp(const char *a, const char *b);
int strncasecmp(const char *a, const char *b, size_t n);

/* The pre-POSIX pair, which `<strings.h>` is their home for. A decompiled N64 codebase uses them
 * throughout: the original compiler had them and the decompilation kept the calls. `bcopy` takes its
 * arguments in the opposite order to `memcpy` and is defined to handle overlap. */
void bcopy(const void *src, void *dst, size_t n);
void bzero(void *dst, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_STRINGS_H */
