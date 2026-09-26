/*
 * `ifaddrs.h` - the interface list, and **`getifaddrs` fails**.
 *
 * # Why failing is the right answer rather than an empty list
 *
 * There is no interface enumeration on this platform. The two honest options were to fail, or to
 * succeed with an empty list; they differ in what a caller concludes. An empty list says "this
 * machine has no network interfaces that are up", which is false - the console has one. Failing
 * says "this cannot be answered here", which is true.
 *
 * ioquake3's `net_ip.c:1278` handles the failure exactly as one would hope - it prints
 * "Unable to get list of network interfaces" and carries on with `numIP = 0` - so nothing is lost
 * by telling it the truth.
 *
 * # What it costs, and the upgrade path
 *
 * A caller that enumerates interfaces to pick a bind address or to answer LAN discovery will find
 * nothing. For a single-player title that is invisible. For anything that wants to be found on a
 * LAN it is not, and the fix is real rather than hypothetical: `oops_net_ctl_get_info` in
 * `<oops/net.h>` knows the console's address, so a future `getifaddrs` can return one entry built
 * from it. That is a small job and it is not done here because nothing has asked for it - the point
 * of the failure is that it will be noticed by whoever does.
 *
 * `freeifaddrs` is a no-op, which is correct: nothing was allocated.
 */
#ifndef OOPS_POSIX_IFADDRS_H
#define OOPS_POSIX_IFADDRS_H

#include <sys/socket.h>

struct ifaddrs {
    struct ifaddrs  *ifa_next;
    char            *ifa_name;
    unsigned int     ifa_flags;
    struct sockaddr *ifa_addr;
    struct sockaddr *ifa_netmask;
    struct sockaddr *ifa_dstaddr;
    void            *ifa_data;
};

#ifdef __cplusplus
extern "C" {
#endif

/* Always -1 with `errno` set, and `*ifap` set to NULL so a caller that ignores the return does not
 * walk an uninitialised pointer. See above. */
int getifaddrs(struct ifaddrs **ifap);
/* Nothing was allocated, so there is nothing to release. */
void freeifaddrs(struct ifaddrs *ifa);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_IFADDRS_H */
