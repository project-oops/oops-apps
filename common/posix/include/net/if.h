/*
 * `net/if.h` - network interface names and flags.
 *
 * ioquake3's `net_ip.c:71` includes it unconditionally on every non-Windows platform, and uses
 * `IFF_UP` when walking the interface list and `if_nametoindex` for an IPv6 scope id.
 *
 * The flag values are FreeBSD's, because that is the kernel underneath and a program comparing
 * against them should see the platform's own numbers.
 *
 * `struct ifreq` and the `SIOCGIF*` ioctls are deliberately absent: they are the older way to
 * enumerate interfaces, nothing in this tree uses them, and a `struct ifreq` that no `ioctl` here
 * would fill is a shape without a meaning.
 */
#ifndef OOPS_POSIX_NET_IF_H
#define OOPS_POSIX_NET_IF_H

#define IF_NAMESIZE 16
#define IFNAMSIZ    IF_NAMESIZE

#define IFF_UP          0x0001
#define IFF_BROADCAST   0x0002
#define IFF_DEBUG       0x0004
#define IFF_LOOPBACK    0x0008
#define IFF_POINTOPOINT 0x0010
#define IFF_RUNNING     0x0040
#define IFF_NOARP       0x0080
#define IFF_PROMISC     0x0100
#define IFF_ALLMULTI    0x0200
#define IFF_MULTICAST   0x8000

#ifdef __cplusplus
extern "C" {
#endif

/*
 * **Always 0, which is the documented "no such interface".**
 *
 * There is no interface-name-to-index mapping to consult here - see `ifaddrs.h` next door for why
 * enumeration is not available either. Callers use the result as an IPv6 scope id, and 0 means
 * "unspecified scope", which is the correct thing to say when the scope is unknown rather than a
 * number that names the wrong interface.
 */
unsigned int if_nametoindex(const char *ifname);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_NET_IF_H */
