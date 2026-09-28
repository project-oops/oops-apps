/*
 * `iconv.h` - character-set conversion between the Unicode encodings, and nothing else.
 *
 * Luanti converts between UTF-8 and `wchar_t` through `iconv` on every POSIX system
 * (`src/util/string.cpp`), asking for "UTF-32LE" (its `wchar_t` is four bytes) and
 * "UTF-8". Those, UTF-16LE and "WCHAR_T" (UTF-32LE here) are what this converts, exactly:
 * a malformed sequence stops with `EILSEQ`, a truncated one with `EINVAL`, and a full
 * output buffer with `E2BIG`, leaving the pointers at the first unconverted byte as POSIX
 * has it. `iconv_open` refuses every other encoding with `EINVAL`, so a program wanting a
 * legacy code page is told so rather than handed UTF-8 under another name.
 */
#ifndef OOPS_POSIX_ICONV_H
#define OOPS_POSIX_ICONV_H

#include <stddef.h>

typedef void *iconv_t;

#ifdef __cplusplus
extern "C" {
#endif

iconv_t iconv_open(const char *tocode, const char *fromcode);
size_t iconv(iconv_t cd, char **inbuf, size_t *inbytesleft, char **outbuf,
             size_t *outbytesleft);
int iconv_close(iconv_t cd);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_ICONV_H */
