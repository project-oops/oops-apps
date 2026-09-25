/*
 * Freestanding <sys/time.h> for the webview build's C side (QuickJS).
 *
 * The SDK's freestanding libc has no <sys/time.h>; QuickJS includes it for `gettimeofday`. `struct
 * timespec`, `clock_gettime` and the `CLOCK_*` ids now live in the SDK's <time.h> (added while this
 * was in flight), so this deliberately does NOT redefine them - it only supplies what <sys/time.h>
 * uniquely owns: `struct timeval` and `gettimeofday`. The daisy UI does not depend on wall-clock
 * time, so this returns the epoch. Part of the webview freestanding-libc workaround
 * (REQ-20260925T0110Z-e1d9).
 */
#ifndef OOPSY_SHIM_SYS_TIME_H
#define OOPSY_SHIM_SYS_TIME_H

#include <time.h>   /* SDK's: struct timespec, clock_gettime, CLOCK_* */

struct timeval  { long tv_sec; long tv_usec; };
struct timezone { int tz_minuteswest; int tz_dsttime; };

static inline int gettimeofday(struct timeval *__tv, void *__tz) {
    (void)__tz;
    if (__tv) { __tv->tv_sec = 0; __tv->tv_usec = 0; }
    return 0;
}

#endif /* OOPSY_SHIM_SYS_TIME_H */
