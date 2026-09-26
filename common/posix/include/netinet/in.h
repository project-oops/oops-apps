/*
 * IP address types. IPv4 works; the IPv6 declarations at the bottom exist so that a program
 * supporting both can compile, and every call behind them fails - see the long note down there.
 *
 * The layouts are the BSD ones because that is what the kernel underneath uses and what a program
 * casting between `sockaddr` and `sockaddr_in` assumes. In particular `sockaddr_in` starts with
 * `sin_len`/`sin_family` as two bytes, not a two-byte `sa_family_t` - that is the BSD shape, and
 * getting it wrong would put the port where the family goes.
 *
 * **Addresses are network byte order**, which on this little-endian machine means `htons` and
 * `htonl` really do swap. A shim that made them no-ops would connect to the wrong port in a way
 * that looks like a firewall.
 */
#ifndef OOPS_POSIX_NETINET_IN_H
#define OOPS_POSIX_NETINET_IN_H

#include <stdint.h>
#include <sys/socket.h>

typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

struct in_addr {
    in_addr_t s_addr; /* network byte order */
};

struct sockaddr_in {
    uint8_t        sin_len;
    sa_family_t    sin_family;
    in_port_t      sin_port; /* network byte order */
    struct in_addr sin_addr;
    char           sin_zero[8];
};

#define INADDR_ANY       ((in_addr_t)0x00000000)
#define INADDR_LOOPBACK  ((in_addr_t)0x7f000001)
#define INADDR_BROADCAST ((in_addr_t)0xffffffff)
#define INADDR_NONE      ((in_addr_t)0xffffffff)

#define IPPROTO_IP   0
#define IPPROTO_TCP  6
#define IPPROTO_UDP 17

/*
 * # IPv6: the types are here, and nothing behind them works
 *
 * **This is deliberate, and it is not the same thing as pretending.** The SDK's network layer is
 * IPv4 only - `oops/net.h` has one address family, `OOPS_AF_INET`, and every call that names an
 * address names it as a dotted quad. So no socket here can carry IPv6 traffic, and the shims in
 * `posix.c` refuse `AF_INET6` explicitly rather than mangling it into IPv4.
 *
 * The declarations exist because a program that supports both families **references both even when
 * it is going to use one**. ioquake3's `net_ip.c` names `struct sockaddr_in6` twenty times in code
 * that runs only after an IPv6 socket opened; a missing type is a compile error across the whole
 * file, including the IPv4 half. It then does exactly the right thing at runtime:
 *
 *     socket(PF_INET6, ...) fails  ->  ip6_socket = INVALID_SOCKET  ->  every IPv6 path skipped
 *
 * That is the shape to keep. A type that exists so a file can compile, with the call that would
 * use it failing at the first step, leaves a program on its own already-written fallback. A type
 * that exists beside a `bind` that *claims* to have bound an IPv6 socket would not.
 *
 * The layouts are FreeBSD's, so a program computing `sizeof` or casting through `sockaddr` agrees
 * with the kernel underneath - which matters the day the SDK grows a second family, because then
 * these structures start being passed to it rather than only around inside the caller.
 */
struct in6_addr {
    union {
        uint8_t  __u6_addr8[16];
        uint16_t __u6_addr16[8];
        uint32_t __u6_addr32[4];
    } __u6_addr;
};
#define s6_addr   __u6_addr.__u6_addr8
#define s6_addr16 __u6_addr.__u6_addr16
#define s6_addr32 __u6_addr.__u6_addr32

struct sockaddr_in6 {
    uint8_t         sin6_len;
    sa_family_t     sin6_family;
    in_port_t       sin6_port;     /* network byte order */
    uint32_t        sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t        sin6_scope_id;
};

/* Multicast group membership, for `setsockopt(IPPROTO_IPV6, IPV6_JOIN_GROUP)`. ioquake3 uses it to
 * join the LAN server-browser group, on the IPv6 path that never opens. */
struct ipv6_mreq {
    struct in6_addr ipv6mr_multiaddr;
    unsigned int    ipv6mr_interface;
};

/* The all-zeroes address, `::`. Defined in `posix.c` - a real object rather than a macro, because
 * callers take its address and pass it by value. */
extern const struct in6_addr in6addr_any;
extern const struct in6_addr in6addr_loopback;

#define IPPROTO_IPV6 41

/* FreeBSD's option numbers, which are RFC 3493's - note that `IPV6_JOIN_GROUP` and
 * `IPV6_LEAVE_GROUP` are 12 and 13 here where Linux uses 20 and 21. Getting that wrong would send
 * a well-formed request for the wrong option. */
#define IPV6_MULTICAST_IF    9
#define IPV6_MULTICAST_HOPS 10
#define IPV6_MULTICAST_LOOP 11
#define IPV6_JOIN_GROUP     12
#define IPV6_LEAVE_GROUP    13
#define IPV6_V6ONLY         27

/* Buffer sizes for the text forms. 16 counts "255.255.255.255" plus its terminator; 46 counts the
 * longest IPv6 form, which is an IPv4-mapped address like "::ffff:255.255.255.255". */
#define INET_ADDRSTRLEN  16
#define INET6_ADDRSTRLEN 46

/*
 * **The address classifiers, and these are real.** They are pure arithmetic on the sixteen bytes -
 * no socket, no kernel, nothing the SDK has to supply - so unlike everything else in this section
 * they give the right answer. A program can classify an IPv6 address here perfectly well; what it
 * cannot do is open a socket to one.
 *
 * `ff00::/8` is multicast; all-zeroes is the unspecified address. Only the two ioquake3 asks for are
 * here, for the reason the top of `posix.c` gives: a third added because the standard has one is a
 * macro nothing uses.
 */
#define IN6_IS_ADDR_MULTICAST(a) (((const struct in6_addr *)(a))->s6_addr[0] == 0xffu)
#define IN6_IS_ADDR_UNSPECIFIED(a)                                             \
    (((const struct in6_addr *)(a))->__u6_addr.__u6_addr32[0] == 0 &&          \
     ((const struct in6_addr *)(a))->__u6_addr.__u6_addr32[1] == 0 &&          \
     ((const struct in6_addr *)(a))->__u6_addr.__u6_addr32[2] == 0 &&          \
     ((const struct in6_addr *)(a))->__u6_addr.__u6_addr32[3] == 0)

#ifdef __cplusplus
extern "C" {
#endif

/* Little-endian host, so these swap. Written as inline byte work rather than a builtin so that
 * there is nothing to link and nothing to get wrong at `-O0`. */
static inline uint16_t htons(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }
static inline uint16_t ntohs(uint16_t v) { return htons(v); }
static inline uint32_t htonl(uint32_t v) {
    return ((v & 0x000000ffu) << 24) | ((v & 0x0000ff00u) << 8) |
           ((v & 0x00ff0000u) >> 8)  | ((v & 0xff000000u) >> 24);
}
static inline uint32_t ntohl(uint32_t v) { return htonl(v); }

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_NETINET_IN_H */
