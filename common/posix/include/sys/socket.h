/*
 * BSD sockets, over `oops/net.h`.
 *
 * # What is here
 *
 * A TCP client: `socket`, `connect`, `send`, `recv`, and the constants that go with them. Craft's
 * `client.c` is the caller. Servers (`bind`, `listen`, `accept`) and datagrams are absent - the
 * SDK has `oops_sendto`/`oops_recvfrom` and could support both, but nothing here has asked and an
 * untested shim is worse than a missing one.
 *
 * # `connect` takes a `sockaddr` and the SDK takes a string, and that conversion is the shim
 *
 * `oops_connect(sock, "1.2.3.4", port)` names the peer as dotted-quad text. A POSIX caller hands
 * over a `struct sockaddr_in` holding a 32-bit address in network byte order, so this formats it
 * back into text. That is a round trip through a string for something that was already a number,
 * and it is what the SDK's interface asks for - noted here rather than hidden, because the first
 * person to add `bind` will wonder.
 *
 * Only `AF_INET` is accepted; anything else fails rather than being silently treated as IPv4.
 */
#ifndef OOPS_POSIX_SYS_SOCKET_H
#define OOPS_POSIX_SYS_SOCKET_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef uint8_t  sa_family_t;
typedef uint32_t socklen_t;

struct sockaddr {
    uint8_t     sa_len;
    sa_family_t sa_family;
    char        sa_data[14];
};

/*
 * **Big enough for any address family, and aligned like one.** `sockaddr_storage` exists so a
 * caller can hold an address without knowing its family - ioquake3's `net_ip.c:141` keeps one per
 * local interface. The two properties that matter are the size (128 bytes, which is FreeBSD's and
 * every other platform's) and the alignment, which is why the `__ss_align` member is `int64_t`
 * rather than the padding being a bare array: a `sockaddr_in6` cast onto a misaligned buffer is a
 * fault on some targets and silently slow on this one.
 */
#define _SS_MAXSIZE   128u
#define _SS_ALIGNSIZE (sizeof(int64_t))
#define _SS_PAD1SIZE  (_SS_ALIGNSIZE - sizeof(uint8_t) - sizeof(sa_family_t))
#define _SS_PAD2SIZE  (_SS_MAXSIZE - sizeof(uint8_t) - sizeof(sa_family_t) \
                       - _SS_PAD1SIZE - _SS_ALIGNSIZE)

struct sockaddr_storage {
    uint8_t     ss_len;
    sa_family_t ss_family;
    char        __ss_pad1[_SS_PAD1SIZE];
    int64_t     __ss_align;
    char        __ss_pad2[_SS_PAD2SIZE];
};

#define AF_UNSPEC 0
#define AF_INET   2
#define PF_INET   AF_INET

#define SOCK_STREAM 1
#define SOCK_DGRAM  2

/* `send`/`recv` flags. Nothing here honours any of them; they are defined because callers name
 * them and pass 0. */
#define MSG_OOB       0x1
#define MSG_PEEK      0x2
#define MSG_DONTROUTE 0x4
#define MSG_WAITALL   0x40

#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2

#ifdef __cplusplus
extern "C" {
#endif

int socket(int domain, int type, int protocol);
/* `AF_INET` only; see the note at the top about the address becoming a string. */
int connect(int sock, const struct sockaddr *addr, socklen_t addrlen);
ssize_t send(int sock, const void *buf, size_t len, int flags);
ssize_t recv(int sock, void *buf, size_t len, int flags);
int shutdown(int sock, int how);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_SYS_SOCKET_H */
