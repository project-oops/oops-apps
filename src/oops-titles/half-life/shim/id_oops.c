/*
 * `Posix_GetNetDeviceAddresses`, in place of `engine/platform/posix/id_posix.c`.
 *
 * The engine builds a machine identity from the network interfaces' hardware addresses.
 * Upstream's FreeBSD branch reads the link-layer entries `getifaddrs` returns, through
 * `net/if_dl.h`. The shared `getifaddrs` always fails (`common/posix/include/ifaddrs.h`
 * says why) and there is no `net/if_dl.h`, so that branch could only ever find nothing.
 * Upstream's fallback branch - for every platform with no way to ask - reports none
 * directly, and this is that fallback: the engine then derives its identity from the
 * other sources it tries, and from `~/.xash_id` once it has written one.
 */
#include <stdint.h>

int Posix_GetNetDeviceAddresses(uint64_t *addresses, int max);

int Posix_GetNetDeviceAddresses(uint64_t *addresses, int max) {
    (void)addresses;
    (void)max;
    return 0;
}
