/*
 * `arpa/inet.h` - address conversion, over `oops/net.h`.
 *
 * ioquake3's `net_ip.c:70` includes this and, as it happens, calls nothing from it - the header is
 * part of the conventional BSD sockets include set. But unlike `sys/mman.h` next door, these
 * functions *can* be implemented here: `oops_net_inet_pton` and `oops_net_inet_ntop` already exist
 * and do exactly this work, so declaring them costs nothing and the next port gets them for free.
 *
 * `htons` and friends come from `<netinet/in.h>`, which this includes, because that is where a
 * caller of either header expects to find them.
 *
 * # `inet_ntoa` returns a pointer to static storage
 *
 * That is the interface, not a shortcut: the caller does not free it and the next call overwrites
 * it. It is why `inet_ntop` replaced it, and why this is not thread-safe - exactly as the original
 * is not. A caller holding two results at once has a bug on every platform.
 */
#ifndef OOPS_POSIX_ARPA_INET_H
#define OOPS_POSIX_ARPA_INET_H

#include <netinet/in.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Network byte order in, text out and back. `INADDR_NONE` on a malformed string, which is what
 * `inet_addr` has always answered and why `inet_pton` exists. */
in_addr_t inet_addr(const char *cp);
/* See the note above: static storage, overwritten by the next call. */
char *inet_ntoa(struct in_addr in);

/* `AF_INET` only; 1 on success, 0 for a malformed address, -1 for an unsupported family. */
int inet_pton(int af, const char *src, void *dst);
/* `dst` on success, NULL with `errno` set. `size` must be at least 16 for `AF_INET`. */
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_ARPA_INET_H */
