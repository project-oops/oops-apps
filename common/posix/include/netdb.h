/*
 * Name resolution: gethostbyname and getaddrinfo, both over oops_net_resolve.
 *
 * Both interfaces exist because ported code calls both; they answer from the same
 * single-address resolver.
 *
 * gethostbyname returns library-owned storage, here one static struct hostent, so it is
 * not thread-safe and the next call overwrites it. h_addr_list holds one address and a
 * NULL terminator, so a caller walking it for a fallback finds none.
 */
#ifndef OOPS_POSIX_NETDB_H
#define OOPS_POSIX_NETDB_H

#include <netinet/in.h>
#include <sys/socket.h>

struct hostent {
    char *h_name;
    char **h_aliases;
    int h_addrtype;
    int h_length;
    char **h_addr_list;
};

/* The BSD spelling some code still uses for `h_addr_list[0]`. */
#define h_addr h_addr_list[0]

/* `h_errno` values. Set by `gethostbyname` below. */
#define HOST_NOT_FOUND 1
#define TRY_AGAIN 2
#define NO_RECOVERY 3
#define NO_DATA 4

#ifdef __cplusplus
extern "C" {
#endif

extern int h_errno;

/* NULL on failure, with `h_errno` set. See the note above about the storage being
 * static. */
struct hostent *gethostbyname(const char *name);

#ifdef __cplusplus
}
#endif

/*
 * getaddrinfo returns a walkable, freeable list, narrower than a desktop's in four ways
 * that follow from the resolver answering one IPv4 address:
 *
 *   1. ai_next is always NULL, so a caller looking for a second address to try finds
 * none.
 *   2. AF_UNSPEC gives IPv4; an explicit AF_INET6 fails with EAI_FAMILY rather than
 * handing an IPv4 address to an IPv6 socket.
 *   3. A service must be numeric or absent - there is no /etc/services - else
 * EAI_SERVICE.
 *   4. ai_canonname stays NULL even with AI_CANONNAME; the resolver returns no
 * canonical name and echoing the query back would invent one.
 *
 * A numeric address resolves with no network, and AI_NUMERICHOST makes a name fail.
 */

struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    socklen_t ai_addrlen;
    char *ai_canonname; /* always NULL here - see above */
    struct sockaddr *ai_addr;
    struct addrinfo *ai_next; /* always NULL here - see above */
};

/* FreeBSD's flag values. `AI_ADDRCONFIG` is accepted and changes nothing: it asks for
 * families this machine has addresses for, and there is one family. */
#define AI_PASSIVE 0x00000001
#define AI_CANONNAME 0x00000002
#define AI_NUMERICHOST 0x00000004
#define AI_NUMERICSERV 0x00001000
#define AI_ADDRCONFIG 0x00000400

/* `getnameinfo` flags, FreeBSD's values. Only `NI_NAMEREQD` changes what happens - see
 * below. */
#define NI_NOFQDN 0x00000001
#define NI_NUMERICHOST 0x00000002
#define NI_NAMEREQD 0x00000004
#define NI_NUMERICSERV 0x00000008
#define NI_DGRAM 0x00000010

#define NI_MAXHOST 1025
#define NI_MAXSERV 32

/* `getaddrinfo` return codes - FreeBSD's, and negative-or-zero is not the convention: 0
 * is success and everything else is one of these positive values, which is why the
 * caller passes the result to `gai_strerror` rather than `strerror`. */
#define EAI_AGAIN 2
#define EAI_BADFLAGS 3
#define EAI_FAIL 4
#define EAI_FAMILY 5
#define EAI_MEMORY 6
#define EAI_NONAME 8
#define EAI_SERVICE 9
#define EAI_SOCKTYPE 10
#define EAI_SYSTEM 11
#define EAI_OVERFLOW 14

#ifdef __cplusplus
extern "C" {
#endif

/* 0, or one of the `EAI_*` above. `*res` is set only on success and must be given to
 * `freeaddrinfo`; the list is one heap block per entry, so freeing it any other way
 * corrupts the heap. */
int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints,
                struct addrinfo **res);
void freeaddrinfo(struct addrinfo *ai);

/*
 * The address as text.
 *
 * **It is always the numeric form, whether or not `NI_NUMERICHOST` was asked for.**
 * There is no reverse resolver in the SDK, and POSIX already allows returning the
 * numeric form when a name cannot be determined - so this is within the interface
 * rather than a compromise. The one flag that matters is `NI_NAMEREQD`, which says a
 * numeric answer is not acceptable: that fails with `EAI_NONAME`, truthfully.
 *
 * `serv` is filled with the port in decimal when a buffer is given. `AF_INET` only;
 * `AF_INET6` fails with `EAI_FAMILY`.
 */
int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host,
                socklen_t hostlen, char *serv, socklen_t servlen, int flags);

const char *gai_strerror(int ecode);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_NETDB_H */
