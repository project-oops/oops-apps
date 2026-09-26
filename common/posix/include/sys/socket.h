/*
 * BSD sockets, over `oops/net.h`.
 *
 * # What is here
 *
 * A TCP client - `socket`, `connect`, `send`, `recv` - for Craft's `client.c`, and a UDP endpoint -
 * `bind`, `sendto`, `recvfrom`, `setsockopt` - for ioquake3's `net_ip.c`, which is a game protocol
 * rather than a stream.
 *
 * `listen` and `accept` are still absent. The SDK has `oops_listen`/`oops_accept` and they would be
 * a short shim, but nothing in this tree runs a TCP server and an untested shim is worse than a
 * missing one.
 *
 * # `AF_INET6` is declared and always refused
 *
 * `netinet/in.h` carries the IPv6 structures and explains why; the short version is that a program
 * supporting both families names both, and the SDK has one. `socket(PF_INET6, ...)` fails here,
 * which is the outcome such a program is already written to handle.
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
/* **28, which is FreeBSD's** - Linux uses 10. Every call here refuses it; see the header note. */
#define AF_INET6  28
#define PF_INET   AF_INET
#define PF_INET6  AF_INET6

#define SOCK_STREAM 1
#define SOCK_DGRAM  2

/*
 * `send`/`recv` flags.
 *
 * **`MSG_PEEK` and `MSG_DONTWAIT` are real**, and they are the two that `select` is built out of -
 * `oops/net.h` records both as measured on this console, and `sys/select.h` explains what it does
 * with them. The rest are defined because callers name them and pass 0; nothing honours them.
 */
#define MSG_OOB       0x1
#define MSG_PEEK      0x2
#define MSG_DONTROUTE 0x4
#define MSG_WAITALL   0x40
#define MSG_DONTWAIT  0x80

/*
 * Socket options. The values are FreeBSD's, and they reach the platform unchanged - `setsockopt`
 * below hands `level` and `optname` straight to `oops_setsockopt`, which passes them to the kernel.
 * So an option this header spells wrongly is not a missing feature, it is a *different* option
 * being set, which is why these are transcribed rather than invented.
 */
#define SOL_SOCKET    0xffff
#define SO_REUSEADDR  0x0004
#define SO_KEEPALIVE  0x0008
#define SO_BROADCAST  0x0020
#define SO_SNDTIMEO   0x1005
#define SO_RCVTIMEO   0x1006
#define SO_ERROR      0x1007

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

/*
 * `AF_INET` only, and the address goes through a string on the way down - the same round trip
 * `connect` makes, for the same reason. An `INADDR_ANY` bind passes an empty string, which is what
 * `oops_bind` documents as meaning "any".
 */
int bind(int sock, const struct sockaddr *addr, socklen_t addrlen);

/*
 * Datagrams.
 *
 * `sendto` with a NULL `dest_addr` is a plain `send`, as POSIX says, so a connected datagram socket
 * works. `recvfrom` fills `src_addr` when it is given one, and `*addrlen` is set to the size of what
 * was written - `sizeof(struct sockaddr_in)`, always, because every address this platform can
 * report is IPv4.
 *
 * **`recvfrom` truncates silently when the buffer is short**, exactly as the real one does for a
 * datagram socket: the rest of the datagram is discarded and there is no `MSG_TRUNC` to say so.
 * ioquake3 passes a `MAX_MSGLEN + 1` buffer and checks for the `+ 1` being used, which is the
 * portable way to notice.
 */
ssize_t sendto(int sock, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t recvfrom(int sock, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);

/*
 * Straight through to `oops_setsockopt`, which is why the constants above are FreeBSD's real values
 * rather than tokens this shim interprets.
 *
 * `getsockopt` is absent: the SDK has no counterpart, and nothing in this tree reads an option back.
 */
int setsockopt(int sock, int level, int optname, const void *optval,
               socklen_t optlen);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_SYS_SOCKET_H */
