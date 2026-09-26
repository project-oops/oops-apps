/*
 * SDL3's timer and system-time backend, over `oops/time.h`.
 *
 * Four symbols, and three of them are a rename. `oops/time.h` already publishes exactly
 * what SDL wants: a monotonic counter, its frequency, and a microsecond sleep.
 */
#include "SDL_internal.h"

#include "oops/time.h"

#ifdef SDL_TIMER_PRIVATE

/* Reached through `-I<upstream>/src`, because this file sits outside upstream's tree.
 */
#include "timer/SDL_timer_c.h"

Uint64 SDL_GetPerformanceCounter(void) {
    return (Uint64)oops_time_get_counter();
}

Uint64 SDL_GetPerformanceFrequency(void) {
    return (Uint64)oops_time_get_counter_frequency();
}

/*
 * **Rounded up, not down, and never to zero.** `oops_time_sleep_us` takes microseconds
 * and SDL asks in nanoseconds, so anything under a microsecond truncates to nothing -
 * and SDL's callers use a short delay as a yield inside a spin, where returning
 * immediately turns the loop into a busy-wait that starves whatever it is waiting for.
 * Rounding up costs at most a microsecond and keeps the call meaning what it says.
 *
 * The 32-bit argument is the other edge: `oops_time_sleep_us` takes a `uint32_t`, which
 * tops out near 71 minutes, and SDL's `Uint64` does not. A long sleep is split rather
 * than truncated, because a truncated one would return early and look like a spurious
 * wakeup.
 */
void SDL_SYS_DelayNS(Uint64 ns) {
    Uint64 us = (ns + 999u) / 1000u;

    if (ns > 0u && us == 0u) {
        us = 1u;
    }
    while (us > 0u) {
        const uint32_t step = (us > 0xF0000000u) ? 0xF0000000u : (uint32_t)us;
        oops_time_sleep_us(step);
        us -= step;
    }
}

#endif /* SDL_TIMER_PRIVATE */

#ifdef SDL_TIME_PRIVATE

#include "time/SDL_time_c.h"

/*
 * **The console has locale settings and the SDK does not read them.**
 *
 * `oops/netctl.h` knows the machine's address and `oops/system.h` its model and serial;
 * nothing exposes the user's date or time format, which live in the system software's
 * settings rather than in anything a payload queries today.
 *
 * SDL's contract here is that a platform writes what it knows and leaves the rest alone
 * - the Unix backend fills these from `nl_langinfo` and simply does not touch them when
 * that says nothing. So this touches neither, and SDL's caller keeps the defaults it
 * set before calling. That is the honest answer: not "the format is unknown", which
 * would be a third value nobody handles, but "this platform did not answer", which is a
 * case the interface already has.
 *
 * A port that wants the real formats wants a `sceSystemServiceParamGetInt` for them,
 * which `oops/system.h` has the shape for - `SCE_SYSTEM_SERVICE_PARAM_ID_DATE_FORMAT`
 * and its `TIME_FORMAT` sibling are the ids. Worth doing when something asks; nothing
 * has.
 */
void SDL_GetSystemTimeLocalePreferences(SDL_DateFormat *df, SDL_TimeFormat *tf) {
    (void)df;
    (void)tf;
}

#endif /* SDL_TIME_PRIVATE */
