/*
 * SDL3's timer and system-time backend, over `oops/time.h`'s monotonic counter, its
 * frequency and its microsecond sleep.
 */
#include "SDL_internal.h"

#include "oops/time.h"

#ifdef SDL_TIMER_PRIVATE

/* Reached through `-I<upstream>/src`; this file sits outside upstream's tree. */
#include "timer/SDL_timer_c.h"

Uint64 SDL_GetPerformanceCounter(void) {
    return (Uint64)oops_time_get_counter();
}

Uint64 SDL_GetPerformanceFrequency(void) {
    return (Uint64)oops_time_get_counter_frequency();
}

/*
 * Nanoseconds round up to microseconds and never to zero: SDL's callers use a short
 * delay as a yield inside a spin. `oops_time_sleep_us` takes a `uint32_t`, so a long
 * sleep is split into steps rather than truncated.
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
 * The SDK does not expose the user's date or time format, so neither is written and
 * SDL's caller keeps the defaults it set, as the Unix backend does when `nl_langinfo`
 * has no answer. The system service ids are `SCE_SYSTEM_SERVICE_PARAM_ID_DATE_FORMAT`
 * and `..._TIME_FORMAT`.
 */
void SDL_GetSystemTimeLocalePreferences(SDL_DateFormat *df, SDL_TimeFormat *tf) {
    (void)df;
    (void)tf;
}

#endif /* SDL_TIME_PRIVATE */
