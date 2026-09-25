/*
 * Webview build: compiler-rt runtime helpers the freestanding payload does not otherwise link.
 *
 * __udivti3 is the 128-bit unsigned division helper clang emits for `unsigned __int128 / __int128`
 * (QuickJS's bignum path reaches it). -nostdlib means compiler-rt is not linked, so it is provided
 * here with a plain shift/subtract long division - deliberately NOT using `/` on the 128-bit type,
 * which would lower straight back to this function. Filed for the SDK (REQ-20260925T0110Z-e1d9).
 *
 * clock_gettime is declared by the SDK's <time.h> and used inside src/time/time.c via the syscall,
 * but the libc *function* is not defined there yet (QuickJS calls it directly). Provided here over
 * the same SYS_clock_gettime path; --allow-multiple-definition tolerates the SDK adding its own.
 */

#include <time.h>
#include "oops/syscall.h"

int clock_gettime(int clk_id, struct timespec *ts);
int clock_gettime(int clk_id, struct timespec *ts) {
    if (!ts) return -1;
    if (sys_call(SYS_clock_gettime, (long)clk_id, (long)ts, 0, 0, 0, 0) != 0) {
        ts->tv_sec = 0;
        ts->tv_nsec = 0;
    }
    return 0;
}

typedef unsigned __int128 __oopsy_u128;

__oopsy_u128 __udivti3(__oopsy_u128 num, __oopsy_u128 den);

__oopsy_u128 __udivti3(__oopsy_u128 num, __oopsy_u128 den) {
    if (den == 0) return 0;            /* match compiler-rt: undefined; return 0 rather than trap */
    if (den > num) return 0;
    if (den == num) return 1;

    __oopsy_u128 quot = 0;
    __oopsy_u128 bit = 1;
    __oopsy_u128 d = den;

    /* Shift the divisor up until it would exceed the numerator, tracking the matching quotient bit. */
    while (d <= num && (d & ((__oopsy_u128)1 << 127)) == 0) {
        d <<= 1;
        bit <<= 1;
    }

    while (bit != 0) {
        if (num >= d) {
            num -= d;
            quot |= bit;
        }
        d >>= 1;
        bit >>= 1;
    }
    return quot;
}
