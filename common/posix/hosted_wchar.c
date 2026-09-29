/*
 * The wide-character C library, at this toolchain's width.
 *
 * A hosted title is compiled against FreeBSD's headers, where `wchar_t` is 4 bytes
 * (UTF-32). The platform's libSceLibcInternal is built for a 2-byte `wchar_t`
 * (UTF-16), so every function of its that takes a wide string reads and writes the
 * wrong width. Measured on hardware: SuperTuxKart's Irrlicht counts a wide string with
 * `do ++len; while (*p++)`, which clang turns into a call to `wcslen`; the platform's
 * `wcslen` stopped at the first two zero bytes of the 4-byte `'c'`, returned 1, and the
 * element `config` was copied out as `co` with no terminator - "no config node".
 *
 * So every function that takes a `wchar_t` array, or writes a `wchar_t` through a
 * pointer, is defined here and the platform's is never bound. The multibyte encoding
 * is UTF-8, the encoding titles' text is in. Functions that take one character by
 * value as `wint_t` (`iswalpha`, `towlower`, ...) are the same width on both sides and
 * stay the platform's.
 *
 * The functions a compiler emits on its own from an idiom (`wcslen`, `wmemcpy`,
 * `wmemset`, ...) are here as well as the ones a title names, since either binds to the
 * platform when it is not.
 */
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* --------------------------------------------------------------------- strings */

size_t wcslen(const wchar_t *s) {
    const wchar_t *p = s;
    while (*p) {
        p++;
    }
    return (size_t)(p - s);
}

size_t wcsnlen(const wchar_t *s, size_t max) {
    size_t n = 0;
    while (n < max && s[n]) {
        n++;
    }
    return n;
}

int wcscmp(const wchar_t *a, const wchar_t *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a < *b ? -1 : *a > *b;
}

int wcsncmp(const wchar_t *a, const wchar_t *b, size_t n) {
    for (; n > 0; n--, a++, b++) {
        if (*a != *b) {
            return *a < *b ? -1 : 1;
        }
        if (*a == 0) {
            break;
        }
    }
    return 0;
}

/* The C locale's collation is code-point order. */
int wcscoll(const wchar_t *a, const wchar_t *b) {
    return wcscmp(a, b);
}

size_t wcsxfrm(wchar_t *restrict dst, const wchar_t *restrict src, size_t n) {
    const size_t len = wcslen(src);
    if (n > 0) {
        const size_t copy = len < n - 1 ? len : n - 1;
        memcpy(dst, src, copy * sizeof(wchar_t));
        dst[copy] = 0;
    }
    return len;
}

wchar_t *wcscpy(wchar_t *restrict dst, const wchar_t *restrict src) {
    wchar_t *d = dst;
    while ((*d++ = *src++) != 0) {
    }
    return dst;
}

wchar_t *wcsncpy(wchar_t *restrict dst, const wchar_t *restrict src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) {
        dst[i] = src[i];
    }
    for (; i < n; i++) {
        dst[i] = 0;
    }
    return dst;
}

wchar_t *wcscat(wchar_t *restrict dst, const wchar_t *restrict src) {
    wcscpy(dst + wcslen(dst), src);
    return dst;
}

wchar_t *wcsncat(wchar_t *restrict dst, const wchar_t *restrict src, size_t n) {
    wchar_t *d = dst + wcslen(dst);
    while (n > 0 && *src) {
        *d++ = *src++;
        n--;
    }
    *d = 0;
    return dst;
}

wchar_t *wcschr(const wchar_t *s, wchar_t c) {
    for (;; s++) {
        if (*s == c) {
            return (wchar_t *)s;
        }
        if (*s == 0) {
            return NULL;
        }
    }
}

wchar_t *wcsrchr(const wchar_t *s, wchar_t c) {
    const wchar_t *last = NULL;
    for (;; s++) {
        if (*s == c) {
            last = s;
        }
        if (*s == 0) {
            return (wchar_t *)last;
        }
    }
}

wchar_t *wcsstr(const wchar_t *restrict hay, const wchar_t *restrict needle) {
    const size_t n = wcslen(needle);
    if (n == 0) {
        return (wchar_t *)hay;
    }
    for (; *hay; hay++) {
        if (*hay == *needle && wcsncmp(hay, needle, n) == 0) {
            return (wchar_t *)hay;
        }
    }
    return NULL;
}

wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n) {
    for (; n > 0; n--, s++) {
        if (*s == c) {
            return (wchar_t *)s;
        }
    }
    return NULL;
}

int wmemcmp(const wchar_t *a, const wchar_t *b, size_t n) {
    for (; n > 0; n--, a++, b++) {
        if (*a != *b) {
            return *a < *b ? -1 : 1;
        }
    }
    return 0;
}

wchar_t *wmemcpy(wchar_t *restrict dst, const wchar_t *restrict src, size_t n) {
    memcpy(dst, src, n * sizeof(wchar_t));
    return dst;
}

wchar_t *wmemmove(wchar_t *dst, const wchar_t *src, size_t n) {
    memmove(dst, src, n * sizeof(wchar_t));
    return dst;
}

wchar_t *wmemset(wchar_t *dst, wchar_t c, size_t n) {
    for (size_t i = 0; i < n; i++) {
        dst[i] = c;
    }
    return dst;
}

/* ----------------------------------------------------------- UTF-8 conversion */

/*
 * A conversion in progress lives in the caller's `mbstate_t`: how many continuation
 * bytes are still to come, the code point so far, and the smallest code point the
 * sequence may encode (an overlong encoding is invalid).
 */
struct utf8_state {
    uint32_t pending;
    uint32_t value;
    uint32_t min;
};

_Static_assert(sizeof(struct utf8_state) <= sizeof(mbstate_t), "mbstate_t holds the state");

static struct utf8_state *utf8_state(mbstate_t *ps) {
    return (struct utf8_state *)(void *)ps;
}

int mbsinit(const mbstate_t *ps) {
    return ps == NULL || ((const struct utf8_state *)(const void *)ps)->pending == 0;
}

size_t mbrtowc(wchar_t *restrict pwc, const char *restrict s, size_t n,
               mbstate_t *restrict ps) {
    static mbstate_t own;
    struct utf8_state *st;
    size_t used = 0;

    if (ps == NULL) {
        ps = &own;
    }
    st = utf8_state(ps);
    if (s == NULL) {
        /* Resets the state; an incomplete sequence left in it is an error. */
        const int bad = st->pending != 0;
        memset(ps, 0, sizeof(*ps));
        if (bad) {
            errno = EILSEQ;
            return (size_t)-1;
        }
        return 0;
    }
    if (n == 0) {
        return (size_t)-2;
    }
    if (st->pending == 0) {
        const unsigned char c = (unsigned char)s[0];
        used = 1;
        if (c < 0x80) {
            if (pwc) {
                *pwc = (wchar_t)c;
            }
            return c ? (size_t)1 : (size_t)0;
        } else if (c >= 0xc2 && c <= 0xdf) {
            st->pending = 1, st->value = c & 0x1fu, st->min = 0x80;
        } else if (c >= 0xe0 && c <= 0xef) {
            st->pending = 2, st->value = c & 0x0fu, st->min = 0x800;
        } else if (c >= 0xf0 && c <= 0xf4) {
            st->pending = 3, st->value = c & 0x07u, st->min = 0x10000;
        } else {
            errno = EILSEQ;
            return (size_t)-1;
        }
    }
    while (st->pending > 0) {
        unsigned char c;
        if (used == n) {
            return (size_t)-2; /* the rest has not arrived; kept in the state */
        }
        c = (unsigned char)s[used++];
        if ((c & 0xc0u) != 0x80u) {
            memset(ps, 0, sizeof(*ps));
            errno = EILSEQ;
            return (size_t)-1;
        }
        st->value = (st->value << 6) | (c & 0x3fu);
        st->pending--;
    }
    if (st->value < st->min || st->value > 0x10ffffu ||
        (st->value >= 0xd800u && st->value <= 0xdfffu)) {
        memset(ps, 0, sizeof(*ps));
        errno = EILSEQ;
        return (size_t)-1;
    }
    if (pwc) {
        *pwc = (wchar_t)st->value;
    }
    {
        const int was_nul = st->value == 0;
        memset(ps, 0, sizeof(*ps));
        return was_nul ? 0 : used;
    }
}

size_t mbrlen(const char *restrict s, size_t n, mbstate_t *restrict ps) {
    static mbstate_t own;
    return mbrtowc(NULL, s, n, ps ? ps : &own);
}

int mbtowc(wchar_t *restrict pwc, const char *restrict s, size_t n) {
    static mbstate_t own;
    size_t r;

    if (s == NULL) {
        memset(&own, 0, sizeof(own));
        return 0; /* UTF-8 has no shift state */
    }
    r = mbrtowc(pwc, s, n, &own);
    if (r == (size_t)-1 || r == (size_t)-2) {
        memset(&own, 0, sizeof(own));
        errno = EILSEQ;
        return -1;
    }
    return (int)r;
}

int mblen(const char *s, size_t n) {
    return mbtowc(NULL, s, n);
}

size_t wcrtomb(char *restrict s, wchar_t wc, mbstate_t *restrict ps) {
    const uint32_t c = (uint32_t)wc;

    if (ps != NULL) {
        memset(ps, 0, sizeof(*ps));
    }
    if (s == NULL) {
        return 1; /* wcrtomb(NULL, ...) is wcrtomb(buf, L'\0', ...) */
    }
    if (c < 0x80u) {
        s[0] = (char)c;
        return 1;
    }
    if (c < 0x800u) {
        s[0] = (char)(0xc0u | (c >> 6));
        s[1] = (char)(0x80u | (c & 0x3fu));
        return 2;
    }
    if (c < 0x10000u) {
        if (c >= 0xd800u && c <= 0xdfffu) {
            errno = EILSEQ;
            return (size_t)-1;
        }
        s[0] = (char)(0xe0u | (c >> 12));
        s[1] = (char)(0x80u | ((c >> 6) & 0x3fu));
        s[2] = (char)(0x80u | (c & 0x3fu));
        return 3;
    }
    if (c <= 0x10ffffu) {
        s[0] = (char)(0xf0u | (c >> 18));
        s[1] = (char)(0x80u | ((c >> 12) & 0x3fu));
        s[2] = (char)(0x80u | ((c >> 6) & 0x3fu));
        s[3] = (char)(0x80u | (c & 0x3fu));
        return 4;
    }
    errno = EILSEQ;
    return (size_t)-1;
}

int wctomb(char *s, wchar_t wc) {
    size_t r;
    if (s == NULL) {
        return 0;
    }
    r = wcrtomb(s, wc, NULL);
    return r == (size_t)-1 ? -1 : (int)r;
}

wint_t btowc(int c) {
    return (c >= 0 && c < 0x80) ? (wint_t)c : WEOF;
}

int wctob(wint_t c) {
    return ((int)c >= 0 && (int)c < 0x80) ? (int)c : EOF;
}

size_t mbsrtowcs(wchar_t *restrict dst, const char **restrict src, size_t len,
                 mbstate_t *restrict ps) {
    return mbsnrtowcs(dst, src, SIZE_MAX, len, ps);
}

size_t wcsrtombs(char *restrict dst, const wchar_t **restrict src, size_t len,
                 mbstate_t *restrict ps) {
    return wcsnrtombs(dst, src, SIZE_MAX, len, ps);
}

size_t mbstowcs(wchar_t *restrict dst, const char *restrict src, size_t len) {
    const char *p = src;
    mbstate_t st;
    memset(&st, 0, sizeof(st));
    return mbsrtowcs(dst, &p, len, &st);
}

size_t wcstombs(char *restrict dst, const wchar_t *restrict src, size_t len) {
    const wchar_t *p = src;
    mbstate_t st;
    memset(&st, 0, sizeof(st));
    return wcsrtombs(dst, &p, len, &st);
}

/* ------------------------------------------------------------- number parsing */

/*
 * The narrow parsers on an ASCII copy. Every character a number is written in is
 * ASCII, so the copy stops at the first that is not, which is where the parse would
 * stop anyway, and a consumed narrow character is one consumed wide character.
 */
#define WCSTO_MAX 512

static const char *wcsto_narrow(const wchar_t *s, char *buf) {
    size_t i = 0;
    for (; i < WCSTO_MAX - 1 && s[i] != 0 && (uint32_t)s[i] < 0x80u; i++) {
        buf[i] = (char)s[i];
    }
    buf[i] = '\0';
    return buf;
}

#define WCSTO_FLOAT(name, type, narrow)                                                \
    type name(const wchar_t *restrict s, wchar_t **restrict end) {                     \
        char buf[WCSTO_MAX];                                                           \
        char *e;                                                                       \
        const type v = narrow(wcsto_narrow(s, buf), &e);                               \
        if (end) {                                                                     \
            *end = (wchar_t *)s + (e - buf);                                           \
        }                                                                              \
        return v;                                                                      \
    }

#define WCSTO_INT(name, type, narrow)                                                  \
    type name(const wchar_t *restrict s, wchar_t **restrict end, int base) {           \
        char buf[WCSTO_MAX];                                                           \
        char *e;                                                                       \
        const type v = narrow(wcsto_narrow(s, buf), &e, base);                         \
        if (end) {                                                                     \
            *end = (wchar_t *)s + (e - buf);                                           \
        }                                                                              \
        return v;                                                                      \
    }

WCSTO_FLOAT(wcstod, double, strtod)
WCSTO_FLOAT(wcstof, float, strtof)
WCSTO_FLOAT(wcstold, long double, strtold)
WCSTO_INT(wcstol, long, strtol)
WCSTO_INT(wcstoll, long long, strtoll)
WCSTO_INT(wcstoul, unsigned long, strtoul)
WCSTO_INT(wcstoull, unsigned long long, strtoull)

/* ------------------------------------------------------------------- swprintf */

/*
 * Formatted output into a wide buffer. Each conversion is formatted by the narrow
 * `snprintf` and widened, except `%ls` and `%lc`, whose arguments are wide and are
 * converted here. The result is UTF-8-decoded into `dst`; like the standard's, it
 * returns -1 when the output does not fit in `n` characters.
 */
static int wide_put(wchar_t *dst, size_t n, size_t *at, const char *text, size_t len) {
    const char *p = text;
    const char *end = text + len;
    mbstate_t st;

    memset(&st, 0, sizeof(st));
    while (p < end) {
        wchar_t wc;
        size_t r = mbrtowc(&wc, p, (size_t)(end - p), &st);
        if (r == (size_t)-1 || r == (size_t)-2) {
            wc = (wchar_t)(unsigned char)*p; /* not UTF-8: one byte, one character */
            r = 1;
            memset(&st, 0, sizeof(st));
        } else if (r == 0) {
            r = 1;
        }
        if (*at + 1 >= n) {
            return -1;
        }
        dst[(*at)++] = wc;
        p += r;
    }
    return 0;
}

int vswprintf(wchar_t *restrict dst, size_t n, const wchar_t *restrict fmt, va_list ap) {
    size_t at = 0;
    char spec[64];
    char out[512];

    if (dst == NULL || n == 0) {
        errno = EOVERFLOW;
        return -1;
    }
    while (*fmt) {
        size_t k = 0;
        int lng = 0, llng = 0, big_l = 0, size_z = 0, size_j = 0, size_t_ = 0, half = 0;
        int len;

        if (*fmt != L'%') {
            char one[MB_LEN_MAX];
            const size_t m = wcrtomb(one, *fmt, NULL);
            if (m == (size_t)-1 || wide_put(dst, n, &at, one, m) != 0) {
                goto overflow;
            }
            fmt++;
            continue;
        }
        spec[k++] = '%';
        fmt++;
        if (*fmt == L'%') {
            if (wide_put(dst, n, &at, "%", 1) != 0) {
                goto overflow;
            }
            fmt++;
            continue;
        }
        /* Flags, width and precision, with `*` taken from the arguments. */
        while (*fmt && k < sizeof(spec) - 24 && wcschr(L"-+ #0", *fmt)) {
            spec[k++] = (char)*fmt++;
        }
        if (*fmt == L'*') {
            k += (size_t)snprintf(spec + k, sizeof(spec) - k, "%d", va_arg(ap, int));
            fmt++;
        } else {
            while (*fmt >= L'0' && *fmt <= L'9' && k < sizeof(spec) - 24) {
                spec[k++] = (char)*fmt++;
            }
        }
        if (*fmt == L'.') {
            spec[k++] = '.';
            fmt++;
            if (*fmt == L'*') {
                k += (size_t)snprintf(spec + k, sizeof(spec) - k, "%d", va_arg(ap, int));
                fmt++;
            } else {
                while (*fmt >= L'0' && *fmt <= L'9' && k < sizeof(spec) - 24) {
                    spec[k++] = (char)*fmt++;
                }
            }
        }
        /* Length. */
        if (*fmt == L'h') {
            half = 1;
            spec[k++] = 'h';
            fmt++;
            if (*fmt == L'h') {
                spec[k++] = 'h';
                fmt++;
            }
        } else if (*fmt == L'l') {
            lng = 1;
            fmt++;
            if (*fmt == L'l') {
                llng = 1;
                fmt++;
            }
        } else if (*fmt == L'L') {
            big_l = 1;
            spec[k++] = 'L';
            fmt++;
        } else if (*fmt == L'z') {
            size_z = 1;
            spec[k++] = 'z';
            fmt++;
        } else if (*fmt == L'j') {
            size_j = 1;
            spec[k++] = 'j';
            fmt++;
        } else if (*fmt == L't') {
            size_t_ = 1;
            spec[k++] = 't';
            fmt++;
        }
        (void)half;
        if (*fmt == 0) {
            break;
        }
        {
            const wchar_t conv = *fmt++;
            if ((conv == L's' && lng) || conv == L'S') {
                /* A wide string: its characters go straight in, the precision
                   limiting how many. Width is not applied to wide strings. */
                const wchar_t *ws = va_arg(ap, const wchar_t *);
                if (ws == NULL) {
                    ws = L"(null)";
                }
                for (; *ws; ws++) {
                    if (at + 1 >= n) {
                        goto overflow;
                    }
                    dst[at++] = *ws;
                }
                continue;
            }
            if ((conv == L'c' && lng) || conv == L'C') {
                const wint_t wc = va_arg(ap, wint_t);
                if (at + 1 >= n) {
                    goto overflow;
                }
                dst[at++] = (wchar_t)wc;
                continue;
            }
            if (lng) {
                spec[k++] = 'l';
                if (llng) {
                    spec[k++] = 'l';
                }
            }
            spec[k++] = (char)conv;
            spec[k] = '\0';
            switch (conv) {
            case L'd':
            case L'i':
            case L'o':
            case L'u':
            case L'x':
            case L'X':
                if (llng) {
                    len = snprintf(out, sizeof(out), spec, va_arg(ap, long long));
                } else if (lng || size_z || size_j || size_t_) {
                    len = snprintf(out, sizeof(out), spec, va_arg(ap, long));
                } else {
                    len = snprintf(out, sizeof(out), spec, va_arg(ap, int));
                }
                break;
            case L'f':
            case L'F':
            case L'e':
            case L'E':
            case L'g':
            case L'G':
            case L'a':
            case L'A':
                if (big_l) {
                    len = snprintf(out, sizeof(out), spec, va_arg(ap, long double));
                } else {
                    len = snprintf(out, sizeof(out), spec, va_arg(ap, double));
                }
                break;
            case L'c':
                len = snprintf(out, sizeof(out), spec, va_arg(ap, int));
                break;
            case L's':
                len = snprintf(out, sizeof(out), spec, va_arg(ap, const char *));
                break;
            case L'p':
                len = snprintf(out, sizeof(out), spec, va_arg(ap, void *));
                break;
            case L'n':
                *va_arg(ap, int *) = (int)at;
                len = 0;
                break;
            default:
                errno = EINVAL;
                return -1;
            }
        }
        if (len < 0) {
            return -1;
        }
        if ((size_t)len >= sizeof(out)) {
            len = (int)sizeof(out) - 1; /* one conversion past 511 bytes is cut */
        }
        if (wide_put(dst, n, &at, out, (size_t)len) != 0) {
            goto overflow;
        }
    }
    dst[at] = 0;
    return (int)at;

overflow:
    dst[n - 1] = 0;
    errno = EOVERFLOW;
    return -1;
}

int swprintf(wchar_t *restrict dst, size_t n, const wchar_t *restrict fmt, ...) {
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = vswprintf(dst, n, fmt, ap);
    va_end(ap);
    return r;
}

/* -------------------------------------------------------------- wide streams */

/*
 * One character at a time through the narrow stream, UTF-8 encoded. `ungetwc` keeps
 * one character per process: C guarantees one pushback, and a title reading wide
 * characters from more than one stream at once is not one this port has met.
 */
static FILE *s_unget_stream;
static wint_t s_unget_char = WEOF;

wint_t fgetwc(FILE *f) {
    char buf[MB_LEN_MAX];
    mbstate_t st;
    size_t n = 0;

    if (s_unget_stream == f && s_unget_char != WEOF) {
        const wint_t c = s_unget_char;
        s_unget_char = WEOF;
        s_unget_stream = NULL;
        return c;
    }
    memset(&st, 0, sizeof(st));
    for (;;) {
        wchar_t wc;
        size_t r;
        const int c = fgetc(f);
        if (c == EOF) {
            return WEOF;
        }
        buf[n++] = (char)c;
        r = mbrtowc(&wc, buf, n, &st);
        memset(&st, 0, sizeof(st)); /* each attempt starts from the first byte again */
        if (r == (size_t)-1) {
            errno = EILSEQ;
            return WEOF;
        }
        if (r != (size_t)-2) {
            return (wint_t)wc;
        }
        if (n == sizeof(buf)) {
            errno = EILSEQ;
            return WEOF;
        }
    }
}

wint_t fputwc(wchar_t wc, FILE *f) {
    char buf[MB_LEN_MAX];
    const size_t n = wcrtomb(buf, wc, NULL);
    if (n == (size_t)-1) {
        return WEOF;
    }
    return fwrite(buf, 1, n, f) == n ? (wint_t)wc : WEOF;
}

wint_t ungetwc(wint_t c, FILE *f) {
    if (c == WEOF) {
        return WEOF;
    }
    s_unget_stream = f;
    s_unget_char = c;
    return c;
}
