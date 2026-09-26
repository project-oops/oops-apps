/*
 * Name resolution: `gethostbyname` and `getaddrinfo`, both over `oops_net_resolve`.
 *
 * # Two interfaces, because two ports asked
 *
 * `getaddrinfo` replaced `gethostbyname` decades ago and is what new code should use. Craft's
 * `client.c:218` calls the old one; ioquake3's `net_ip.c:287` calls the new one. A port's job here
 * is to compile the port rather than to modernise it, so both exist, and both answer from the same
 * single-address resolver underneath.
 *
 * # What the old interface obliges
 *
 * `gethostbyname` returns a pointer to storage the *library* owns, which the caller reads and does
 * not free, and which the next call may overwrite. That is why it was replaced. The storage here
 * is one static `struct hostent` with one address, so:
 *
 *   - **It is not thread-safe**, exactly as the original is not. Two threads resolving at once
 *     will tread on each other. Craft resolves once, on the main thread, before starting its
 *     worker.
 *   - **Only the first address is returned.** `h_addr_list` has one entry and a NULL terminator,
 *     because `oops_net_resolve` answers one address. A caller walking the list for a fallback
 *     finds nothing to fall back to, which is a real difference from a desktop.
 */
#ifndef OOPS_POSIX_NETDB_H
#define OOPS_POSIX_NETDB_H

#include <netinet/in.h>
#include <sys/socket.h>

struct hostent {
    char  *h_name;
    char **h_aliases;
    int    h_addrtype;
    int    h_length;
    char **h_addr_list;
};

/* The BSD spelling some code still uses for `h_addr_list[0]`. */
#define h_addr h_addr_list[0]

/* `h_errno` values. Set by `gethostbyname` below. */
#define HOST_NOT_FOUND 1
#define TRY_AGAIN      2
#define NO_RECOVERY    3
#define NO_DATA        4

#ifdef __cplusplus
extern "C" {
#endif

extern int h_errno;

/* NULL on failure, with `h_errno` set. See the note above about the storage being static. */
struct hostent *gethostbyname(const char *name);

#ifdef __cplusplus
}
#endif

/*
 * # `getaddrinfo`, and the four ways it is narrower than the real one
 *
 * It resolves, and the result is a correctly-formed list a caller can walk and free. The
 * differences from a desktop's are all consequences of what the SDK answers:
 *
 *   1. **One address, never a list.** `oops_net_resolve` returns a single IPv4 address, so
 *      `ai_next` is always NULL. A caller walking the list looking for a second address to try
 *      finds none - the same limitation `gethostbyname` has above, for the same reason.
 *   2. **`AF_INET6` and `AF_UNSPEC` both give IPv4.** `AF_UNSPEC` means "whatever you have", and
 *      IPv4 is what there is, so that one is honest. An explicit `ai_family = AF_INET6` fails with
 *      `EAI_FAMILY` rather than quietly returning an IPv4 address a caller would then hand to an
 *      IPv6 socket.
 *   3. **A service must be numeric or absent.** There is no `/etc/services` on this filesystem to
 *      turn "http" into 80, and a built-in table of guesses would be a different program's port
 *      numbers. A named service fails with `EAI_SERVICE`.
 *   4. **`ai_canonname` is never filled**, not even with `AI_CANONNAME`. The resolver returns an
 *      address and no canonical name, and echoing the caller's own query string back as the
 *      "canonical" name would be inventing an answer. It stays NULL, which `AI_CANONNAME`'s callers
 *      must already handle.
 *
 * A numeric address is recognised without resolving, so `getaddrinfo("10.0.0.1", ...)` works with no
 * network at all - and `AI_NUMERICHOST` is honoured, meaning a name fails rather than being looked
 * up.
 */

struct addrinfo {
    int              ai_flags;
    int              ai_family;
    int              ai_socktype;
    int              ai_protocol;
    socklen_t        ai_addrlen;
    char            *ai_canonname; /* always NULL here - see above */
    struct sockaddr *ai_addr;
    struct addrinfo *ai_next;      /* always NULL here - see above */
};

/* FreeBSD's flag values. `AI_ADDRCONFIG` is accepted and changes nothing: it asks for families this
 * machine has addresses for, and there is one family. */
#define AI_PASSIVE     0x00000001
#define AI_CANONNAME   0x00000002
#define AI_NUMERICHOST 0x00000004
#define AI_NUMERICSERV 0x00001000
#define AI_ADDRCONFIG  0x00000400

/* `getnameinfo` flags, FreeBSD's values. Only `NI_NAMEREQD` changes what happens - see below. */
#define NI_NOFQDN      0x00000001
#define NI_NUMERICHOST 0x00000002
#define NI_NAMEREQD    0x00000004
#define NI_NUMERICSERV 0x00000008
#define NI_DGRAM       0x00000010

#define NI_MAXHOST 1025
#define NI_MAXSERV   32

/* `getaddrinfo` return codes - FreeBSD's, and negative-or-zero is not the convention: 0 is success
 * and everything else is one of these positive values, which is why the caller passes the result to
 * `gai_strerror` rather than `strerror`. */
#define EAI_AGAIN     2
#define EAI_BADFLAGS  3
#define EAI_FAIL      4
#define EAI_FAMILY    5
#define EAI_MEMORY    6
#define EAI_NONAME    8
#define EAI_SERVICE   9
#define EAI_SOCKTYPE 10
#define EAI_SYSTEM   11
#define EAI_OVERFLOW 14

#ifdef __cplusplus
extern "C" {
#endif

/* 0, or one of the `EAI_*` above. `*res` is set only on success and must be given to
 * `freeaddrinfo`; the list is one heap block per entry, so freeing it any other way corrupts the
 * heap. */
int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *ai);

/*
 * The address as text.
 *
 * **It is always the numeric form, whether or not `NI_NUMERICHOST` was asked for.** There is no
 * reverse resolver in the SDK, and POSIX already allows returning the numeric form when a name
 * cannot be determined - so this is within the interface rather than a compromise. The one flag that
 * matters is `NI_NAMEREQD`, which says a numeric answer is not acceptable: that fails with
 * `EAI_NONAME`, truthfully.
 *
 * `serv` is filled with the port in decimal when a buffer is given. `AF_INET` only; `AF_INET6` fails
 * with `EAI_FAMILY`.
 */
int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host,
                socklen_t hostlen, char *serv, socklen_t servlen, int flags);

const char *gai_strerror(int ecode);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_NETDB_H */
