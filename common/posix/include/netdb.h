/*
 * `gethostbyname`, over `oops_net_resolve`.
 *
 * # It is the old interface, and that is deliberate
 *
 * `getaddrinfo` replaced this decades ago and is what new code should use. Craft's `client.c:218`
 * calls `gethostbyname`, and a port's job here is to compile the port rather than to modernise
 * it - so this is the interface that exists, and `getaddrinfo` is absent until something asks.
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

#endif /* OOPS_POSIX_NETDB_H */
