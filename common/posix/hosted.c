/*
 * The POSIX calls a hosted title's C library declares and the platform does not
 * provide.
 *
 * A hosted title takes its C library from the Mesa sysroot, so it needs almost nothing
 * from `posix.c` beside this: `stat`, `opendir` and their kin are already there, and
 * this directory's headers would shadow the sysroot's own. What the sysroot declares
 * but no platform library exports is the whole gap, and the import manifest names it -
 * a title fails to package rather than failing on the console.
 *
 * Refusing is the right answer for `copy_file_range` and `utimensat`. libc++'s
 * `<filesystem>` selects its copy implementation by platform test and treats ENOSYS,
 * EINVAL and EOPNOTSUPP as "not available here", falling through to the `fstream` path;
 * a wrong answer would be a silently truncated file. Set `errno` and return -1.
 *
 * The rest are real implementations over what the platform does export (obSCEne's
 * corpus, `data/mined-names.txt`): `getentropy` over `arc4random_buf`, the
 * length-bounded conversions over `mbrtowc` and `wcrtomb`, and `gmtime_r` as
 * arithmetic, which needs nothing.
 */
#include <errno.h>
#include <limits.h>
#include <net/if.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

/* Declared in the sysroot's <unistd.h> under __BSD_VISIBLE. FreeBSD 13 has the syscall;
   this platform exports no such symbol. */
ssize_t copy_file_range(int infd, off_t *inoffp, int outfd, off_t *outoffp, size_t len,
                        unsigned int flags) {
    (void)inoffp;
    (void)outoffp;
    (void)len;
    (void)flags;
    if (infd < 0 || outfd < 0) {
        errno = EBADF;
        return -1;
    }
    errno = ENOSYS;
    return -1;
}

/* `<filesystem>`'s last_write_time setter. The platform exports no timestamp call that
   takes a descriptor-relative path, and AT_FDCWD is the only anchor a title has, so
   refusing keeps the error where a caller can see it. */
int utimensat(int dirfd, const char *path, const struct timespec times[2], int flag) {
    (void)dirfd;
    (void)times;
    (void)flag;
    if (path == NULL) {
        errno = EFAULT;
        return -1;
    }
    errno = ENOSYS;
    return -1;
}

/* `MB_CUR_MAX` expands to this in the sysroot's <stdlib.h>. The platform exports its
   own as data (`__mb_cur_max`), of a width the corpus does not record, so this answers
   the upper bound instead: every caller sizes a buffer from it, and too large costs
   only space. libc++'s locale reaches it. */
int ___mb_cur_max(void) {
    return MB_LEN_MAX;
}

/* `mbsrtowcs` bounded to `nms` source bytes, over `mbrtowc`. libc++'s codecvt converts
   a buffer at a time with it. As FreeBSD's: `dst == NULL` counts without storing or
   advancing
   `*src`, a completed NUL ends the string and sets `*src` to NULL, and an incomplete
   character at the end of the bound is consumed into `ps` and stops the conversion. */
size_t mbsnrtowcs(wchar_t *restrict dst, const char **restrict src, size_t nms,
                  size_t len, mbstate_t *restrict ps) {
    static mbstate_t own;
    const char *s = *src;
    size_t n = 0;

    if (ps == NULL) {
        ps = &own;
    }
    while (dst == NULL || n < len) {
        wchar_t wc;
        const size_t r = mbrtowc(&wc, s, nms, ps);

        if (r == (size_t)-1) {
            if (dst != NULL) {
                *src = s;
            }
            return (size_t)-1;
        }
        if (r == (size_t)-2) {
            /* The bound ends inside a character; `ps` holds what was read of it. */
            s += nms;
            break;
        }
        if (r == 0) {
            if (dst != NULL) {
                dst[n] = L'\0';
                *src = NULL;
            }
            return n;
        }
        if (dst != NULL) {
            dst[n] = wc;
        }
        s += r;
        nms -= r;
        n++;
    }
    if (dst != NULL) {
        *src = s;
    }
    return n;
}

/* `wcsrtombs` bounded to `nwc` source characters, over `wcrtomb`. A character is stored
   only when all of it fits in the `len` bytes left; one that does not stops the
   conversion before it, with `*src` pointing at it. */
size_t wcsnrtombs(char *restrict dst, const wchar_t **restrict src, size_t nwc,
                  size_t len, mbstate_t *restrict ps) {
    static mbstate_t own;
    const wchar_t *s = *src;
    size_t n = 0;

    if (ps == NULL) {
        ps = &own;
    }
    for (; nwc > 0; nwc--, s++) {
        char buf[MB_LEN_MAX];
        mbstate_t before = *ps;
        const size_t r = wcrtomb(buf, *s, ps);

        if (r == (size_t)-1) {
            if (dst != NULL) {
                *src = s;
            }
            return (size_t)-1;
        }
        if (dst != NULL) {
            if (n + r > len) {
                *ps = before;
                break;
            }
            memcpy(dst + n, buf, r);
        }
        if (*s == L'\0') {
            /* The NUL is stored but not counted. */
            if (dst != NULL) {
                *src = NULL;
            }
            return n + r - 1;
        }
        n += r;
    }
    if (dst != NULL) {
        *src = s;
    }
    return n;
}

/* Clears memory in a way the compiler may not remove as a dead store. Mbed TLS and cURL
   wipe keys with it. */
void explicit_bzero(void *p, size_t n) {
    void *(*volatile set)(void *, int, size_t) = memset;
    set(p, 0, n);
}

/* Kernel-seeded bytes, as FreeBSD's `getentropy`: at most 256 per call, EIO beyond. The
   platform has no `getentropy` or `getrandom`, but it exports `arc4random_buf`
   (libScePosixForWebKit), whose stream is seeded from the kernel. Mbed TLS seeds its
   DRBG with it. */
int getentropy(void *buf, size_t n) {
    if (n > 256) {
        errno = EIO;
        return -1;
    }
    arc4random_buf(buf, n);
    return 0;
}

/* UTC broken-down time, by arithmetic (the civil-from-days algorithm), so it is
   reentrant without borrowing the platform's static `gmtime` buffer. cURL and Mbed TLS
   format certificate and header dates with it. */
struct tm *gmtime_r(const time_t *t, struct tm *tm) {
    long long secs = (long long)*t;
    long long days = secs / 86400;
    long long rem = secs % 86400;

    if (rem < 0) {
        rem += 86400;
        days--;
    }
    tm->tm_hour = (int)(rem / 3600);
    tm->tm_min = (int)(rem % 3600 / 60);
    tm->tm_sec = (int)(rem % 60);
    /* 1970-01-01 was a Thursday. */
    tm->tm_wday = (int)((days % 7 + 11) % 7);

    const long long z = days + 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const long long doe = z - era * 146097;
    const long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long long mp = (5 * doy + 2) / 153;
    const long long mday = doy - (153 * mp + 2) / 5 + 1;
    const long long mon = mp < 10 ? mp + 3 : mp - 9;
    const long long year = yoe + era * 400 + (mon <= 2);
    const int leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    static const short before[12] = {0,   31,  59,  90,  120, 151,
                                     181, 212, 243, 273, 304, 334};

    if (year - 1900 > INT_MAX || year - 1900 < INT_MIN) {
        errno = EOVERFLOW;
        return NULL;
    }
    tm->tm_year = (int)(year - 1900);
    tm->tm_mon = (int)(mon - 1);
    tm->tm_mday = (int)mday;
    tm->tm_yday = before[mon - 1] + (int)mday - 1 + (leap && mon > 2);
    tm->tm_isdst = 0;
    tm->tm_gmtoff = 0;
    static char utc[] = "UTC";
    tm->tm_zone = utc;
    return tm;
}

/* The corpus records this name without a library that exports it, so it answers "no
   such interface", as the call does for a name it cannot resolve. SuperTuxKart's LAN
   discovery asks it for an IPv6 scope id and carries on without one. */
unsigned int if_nametoindex(const char *name) {
    (void)name;
    errno = ENXIO;
    return 0;
}

/* FreeBSD reads `/etc/protocols`, which this platform has not got, and exports no such
   call. The numbers are IANA's and fixed, so a table of the ones a socket program names
   is the file's answer for them; any other number is unknown, as it would be to a file
   without that line. DNS-C (SuperTuxKart) resolves a service's protocol name through
   it. */
struct protoent *getprotobynumber(int proto) {
    static char ip[] = "ip", icmp[] = "icmp", tcp[] = "tcp", udp[] = "udp",
                icmp6[] = "ipv6-icmp";
    static char ip_alias[] = "IP", icmp_alias[] = "ICMP", tcp_alias[] = "TCP",
                udp_alias[] = "UDP", icmp6_alias[] = "IPv6-ICMP";
    static char *ip_aliases[] = {ip_alias, NULL}, *icmp_aliases[] = {icmp_alias, NULL},
                *tcp_aliases[] = {tcp_alias, NULL}, *udp_aliases[] = {udp_alias, NULL},
                *icmp6_aliases[] = {icmp6_alias, NULL};
    static struct protoent table[] = {
        {ip, ip_aliases, 0},    {icmp, icmp_aliases, 1},    {tcp, tcp_aliases, 6},
        {udp, udp_aliases, 17}, {icmp6, icmp6_aliases, 58},
    };

    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (table[i].p_proto == proto) {
            return &table[i];
        }
    }
    return NULL;
}
