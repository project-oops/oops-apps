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
#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#include "oops/net.h"       /* the DNS query and parser, oops_net_inet_pton/ntop */
#include "oops/netctl.h"    /* the network's DNS servers */
#include "oops/system.h"    /* oops_log_* */
#include "oops/sysmodule.h" /* oops_sysmodule_load */

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

/*
 * `arc4random` and its siblings, over the platform's random source.
 *
 * The platform's own `arc4random` family is in libScePosixForWebKit, the system
 * browser's support library, which is not loaded into a title: SuperTuxKart's
 * `std::random_device` (libc++ takes `arc4random` on FreeBSD) trapped with
 * PRX_NOT_RESOLVED_FUNCTION. Two other routes were measured and fail for a title: the
 * kernel's `getrandom` (syscall 563) answers -1, and `sceRandomGetRandomNumber` bound to
 * libSceNet traps unresolved. This binds it to libSceRandom (the corpus lists both;
 * `common/symbols.txt` places it) and loads that module first. At most 64 bytes a call.
 * The family has no failure return, so a refusal aborts rather than handing back
 * predictable bytes.
 */
#define HOSTED_RANDOM_CHUNK 64u
int sceRandomGetRandomNumber(void *buf, size_t size); /* libSceRandom */

void arc4random_buf(void *buf, size_t n) {
    static int loaded;
    unsigned char *p = buf;

    if (!loaded) {
        const int rc = oops_sysmodule_load(OOPS_SYSMODULE_RANDOM);
        if (rc != 0) {
            fprintf(stderr, "arc4random: libSceRandom did not load (0x%x)\n", (unsigned)rc);
            abort();
        }
        loaded = 1;
    }
    while (n > 0) {
        const size_t want = n < HOSTED_RANDOM_CHUNK ? n : HOSTED_RANDOM_CHUNK;
        const int rc = sceRandomGetRandomNumber(p, want);
        if (rc != 0) {
            fprintf(stderr, "arc4random: sceRandomGetRandomNumber refused (0x%x)\n",
                    (unsigned)rc);
            abort();
        }
        p += want;
        n -= want;
    }
}

uint32_t arc4random(void) {
    uint32_t v;
    arc4random_buf(&v, sizeof(v));
    return v;
}

/* Rejection sampling, as FreeBSD's: every result in [0, upper_bound) equally likely. */
uint32_t arc4random_uniform(uint32_t upper_bound) {
    uint32_t min;
    uint32_t r;
    if (upper_bound < 2u) {
        return 0u;
    }
    min = (0u - upper_bound) % upper_bound;
    do {
        r = arc4random();
    } while (r < min);
    return r % upper_bound;
}

/* Kernel-seeded bytes, as FreeBSD's `getentropy`: at most 256 per call, EIO beyond.
   Mbed TLS seeds its DRBG with it. */
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

/* `getcwd` and `chdir` are oops-mesa's (`src/runtime/libc_absent.c`), with the working
 * directory the rest of the runtime resolves relative paths against. */

/*
 * `lstat` and `access`, which libkernel exports and refuses: measured on hardware from
 * SuperTuxKart, against a file that is there (`/app0/data/supertuxkart.1.5`), both answer
 * EPERM, while `fopen` and the SDK's `oops_fs_exists` (SYS_open) see it. Irrlicht tests
 * for every file with `access`, so a title that could open its data could not find it.
 *
 * Both go to `stat`, which oops-mesa's runtime answers over the same SDK calls. There are
 * no symbolic links in a title's sandbox, so `lstat` is `stat`.
 */
int lstat(const char *restrict path, struct stat *restrict out) {
    return stat(path, out);
}

/* Existence only, as for a freestanding title: the platform has no permission model to
 * consult, and a title that can see a file can read it. */
int access(const char *path, int mode) {
    struct stat sb;

    (void)mode;
    return stat(path, &sb);
}

/*
 * `realpath`, lexically.
 *
 * libSceLibcInternal's `realpath` walks the path with the `lstat` and `getcwd` a title
 * cannot use (see above and oops-mesa's runtime), and Irrlicht opens every file through
 * it (`CFileSystem::getAbsolutePath`).
 *
 * There are no symbolic links in a title's sandbox, so the canonical path is the lexical
 * one: oops-mesa's `oops_mesa_absolute_path` makes a relative path absolute against the
 * working directory and removes `.`, `..` and repeated separators. As POSIX asks, the
 * result must exist (ENOENT otherwise), and a NULL `resolved` is allocated for the
 * caller.
 */
const char *oops_mesa_absolute_path(const char *path, char *buf, size_t max); /* oops-mesa */

char *realpath(const char *restrict path, char *restrict resolved) {
    char tmp[PATH_MAX];
    const char *canon;
    struct stat sb;
    size_t len;
    char *out;

    if (path == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if (path[0] == '\0') {
        errno = ENOENT;
        return NULL;
    }
    canon = oops_mesa_absolute_path(path, tmp, sizeof(tmp));
    len = strlen(canon);
    if (len >= PATH_MAX) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    if (stat(canon, &sb) != 0) {
        return NULL; /* stat set ENOENT */
    }
    out = resolved ? resolved : malloc(PATH_MAX);
    if (out == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    memcpy(out, canon, len + 1);
    return out;
}

/*
 * Name resolution: `getaddrinfo` and its companions.
 *
 * The platform's are in libScePosixForWebKit, the system browser's support library,
 * which is not loaded into a title: SuperTuxKart's first HTTP request (cURL resolving
 * its news server) trapped with PRX_NOT_RESOLVED_FUNCTION. These are the answers
 * `common/posix/posix.c` gives a freestanding title, over oops-sdk's resolver
 * (`oops_net_resolve`), and they are narrower than a desktop's in the same four ways:
 * IPv4 only, one address per name, no canonical name, and numeric services only - there
 * is no services database, so `getservbyname` finds nothing.
 */
struct hosted_addrinfo_block {
    struct addrinfo ai;
    struct sockaddr_in sa;
};

/*
 * A name to a dotted quad: the SDK's DNS query and answer parser, carried over this
 * C library's UDP socket.
 *
 * Not `oops_net_resolve`, although it is the same client. A title built hosted compiles
 * the SDK's socket layer as its host-build stubs (`oops-sdk/src/net/net.c`, the
 * `__STDC_HOSTED__` branch), so its query is never sent and every name fails. cURL's own
 * connections use these same C-library sockets. The network's DNS servers first, then
 * public resolvers, as the SDK orders them; 1.5 s each.
 */
static int hosted_dns_resolve(const char *name, char *out_ip, size_t out_len) {
    enum { tx_id = 0x5053 };
    uint8_t query[512], answer[512];
    char servers[6][16];
    int nservers = 0;
    oops_net_info_t info;

    const int qlen = oops_dns_build_query(name, tx_id, query, sizeof(query));
    if (qlen <= 0) {
        return -1;
    }
    if (oops_net_ctl_get_info(&info) == 0) {
        if (info.primary_dns[0] != '\0') {
            snprintf(servers[nservers++], sizeof(servers[0]), "%s", info.primary_dns);
        }
        if (info.secondary_dns[0] != '\0') {
            snprintf(servers[nservers++], sizeof(servers[0]), "%s", info.secondary_dns);
        }
    }
    static const char *const fallback[] = {"1.1.1.1", "8.8.8.8", "1.0.0.1", "8.8.4.4"};
    for (size_t i = 0; i < sizeof(fallback) / sizeof(fallback[0]); i++) {
        snprintf(servers[nservers++], sizeof(servers[0]), "%s", fallback[i]);
    }

    for (int s = 0; s < nservers; s++) {
        struct sockaddr_in to;
        uint32_t addr = 0;
        if (oops_net_inet_pton(servers[s], &addr) != 0) {
            continue;
        }
        memset(&to, 0, sizeof(to));
        to.sin_len = (uint8_t)sizeof(to);
        to.sin_family = AF_INET;
        to.sin_port = htons(53);
        to.sin_addr.s_addr = addr;

        const int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (fd < 0) {
            oops_log_warn("DNS", "socket: errno %d", errno);
            return -1; /* no socket for one server is no socket for any */
        }
        int ok = 0;
        if (sendto(fd, query, (size_t)qlen, 0, (const struct sockaddr *)(const void *)&to,
                   (socklen_t)sizeof(to)) == (ssize_t)qlen) {
            struct pollfd pfd = {.fd = fd, .events = POLLIN, .revents = 0};
            if (poll(&pfd, 1, 1500) == 1) {
                const ssize_t n = recv(fd, answer, sizeof(answer), 0);
                ok = n > 0 && oops_dns_parse_response(answer, (size_t)n, tx_id, out_ip,
                                                      out_len) == 0;
            }
        } else {
            oops_log_warn("DNS", "sendto %s: errno %d", servers[s], errno);
        }
        close(fd);
        if (ok) {
            oops_log_info("DNS", "'%s' -> %s (via %s)", name, out_ip, servers[s]);
            return 0;
        }
    }
    oops_log_warn("DNS", "'%s' did not resolve at any of %d servers", name, nservers);
    return -1;
}

static int hosted_service_port(const char *service, uint16_t *out) {
    unsigned long v = 0;

    *out = 0;
    if (service == NULL || *service == '\0') {
        return 0;
    }
    for (const char *p = service; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return EAI_SERVICE;
        }
        v = v * 10u + (unsigned long)(*p - '0');
        if (v > 65535u) {
            return EAI_SERVICE;
        }
    }
    *out = (uint16_t)v;
    return 0;
}

int getaddrinfo(const char *restrict node, const char *restrict service,
                const struct addrinfo *restrict hints, struct addrinfo **restrict res) {
    int family = AF_UNSPEC, socktype = 0, protocol = 0, flags = 0, rc;
    uint16_t port = 0;
    uint32_t packed = 0;
    struct hosted_addrinfo_block *block;

    if (res == NULL) {
        return EAI_SYSTEM;
    }
    *res = NULL;
    if (hints != NULL) {
        family = hints->ai_family;
        socktype = hints->ai_socktype;
        protocol = hints->ai_protocol;
        flags = hints->ai_flags;
    }
    if (family != AF_UNSPEC && family != AF_INET) {
        return EAI_FAMILY; /* an IPv4 answer handed to an IPv6 socket would be worse */
    }
    rc = hosted_service_port(service, &port);
    if (rc != 0) {
        return rc;
    }
    if (node == NULL) {
        packed = (flags & AI_PASSIVE) ? htonl(INADDR_ANY) : htonl(INADDR_LOOPBACK);
    } else if (oops_net_inet_pton(node, &packed) == 0) {
        /* numeric: no resolver involved */
    } else if (flags & AI_NUMERICHOST) {
        return EAI_NONAME;
    } else {
        char ip[INET_ADDRSTRLEN];
        if (hosted_dns_resolve(node, ip, sizeof(ip)) != 0) {
            return EAI_NONAME;
        }
        if (oops_net_inet_pton(ip, &packed) != 0) {
            return EAI_FAIL;
        }
    }
    block = calloc(1, sizeof(*block));
    if (block == NULL) {
        return EAI_MEMORY;
    }
    block->sa.sin_len = (uint8_t)sizeof(block->sa);
    block->sa.sin_family = AF_INET;
    block->sa.sin_port = htons(port);
    block->sa.sin_addr.s_addr = packed;
    block->ai.ai_flags = flags;
    block->ai.ai_family = AF_INET;
    block->ai.ai_socktype = socktype;
    block->ai.ai_protocol = protocol;
    block->ai.ai_addrlen = (socklen_t)sizeof(block->sa);
    block->ai.ai_addr = (struct sockaddr *)(void *)&block->sa;
    *res = &block->ai;
    return 0;
}

void freeaddrinfo(struct addrinfo *ai) {
    while (ai != NULL) {
        struct addrinfo *next = ai->ai_next;
        free(ai); /* the sockaddr is inside the same block */
        ai = next;
    }
}

int getnameinfo(const struct sockaddr *restrict sa, socklen_t salen, char *restrict host,
                size_t hostlen, char *restrict serv, size_t servlen, int flags) {
    const struct sockaddr_in *in = (const struct sockaddr_in *)(const void *)sa;

    if (sa == NULL || salen < (socklen_t)sizeof(*in) || in->sin_family != AF_INET) {
        return EAI_FAMILY;
    }
    if (flags & NI_NAMEREQD) {
        return EAI_NONAME; /* no reverse resolver, and a number is what this flag forbids */
    }
    if (host != NULL && hostlen > 0) {
        if (hostlen < INET_ADDRSTRLEN) {
            return EAI_OVERFLOW;
        }
        if (oops_net_inet_ntop(in->sin_addr.s_addr, host, hostlen) != 0) {
            return EAI_FAIL;
        }
    }
    if (serv != NULL && servlen > 0) {
        if ((size_t)snprintf(serv, servlen, "%u", (unsigned)ntohs(in->sin_port)) >= servlen) {
            return EAI_OVERFLOW;
        }
    }
    return 0;
}

const char *gai_strerror(int ecode) {
    switch (ecode) {
    case 0:
        return "no error";
    case EAI_AGAIN:
        return "temporary failure in name resolution";
    case EAI_BADFLAGS:
        return "invalid flags";
    case EAI_FAIL:
        return "non-recoverable failure in name resolution";
    case EAI_FAMILY:
        return "address family not supported";
    case EAI_MEMORY:
        return "memory allocation failure";
    case EAI_NONAME:
        return "name or service not known";
    case EAI_SERVICE:
        return "service not supported for this socket type";
    case EAI_SOCKTYPE:
        return "socket type not supported";
    case EAI_SYSTEM:
        return "system error";
    case EAI_OVERFLOW:
        return "argument buffer overflow";
    default:
        return "unknown error";
    }
}

/* No services database on this platform, so no service has an entry. */
struct servent *getservbyname(const char *name, const char *proto) {
    (void)name;
    (void)proto;
    return NULL;
}
