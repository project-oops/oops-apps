/*
 * IPv4 address types, enough for a port that opens a TCP connection.
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
