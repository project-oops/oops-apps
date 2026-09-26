/*
 * SDL's timer backend over `oops/time.h`. Six functions, and it is here rather than
 * upstream's `src/timer/unix/` because that one calls `clock_gettime` and `nanosleep`,
 * and this SDK exposes neither - checked, not assumed.
 *
 * Nothing else about the port needs this file to exist, which is exactly why it does: a
 * context and an event pump with no clock cannot be run, and a title's first call is
 * usually `SDL_GetTicks`.
 */
#include "SDL_internal.h"

#ifdef SDL_TIMER_PROSPERO

#include "SDL_timer.h"
#include "timer/SDL_timer_c.h"

#include "oops/time.h"

static SDL_bool ticks_started = SDL_FALSE;
static Uint64 start_ms;

void SDL_TicksInit(void) {
    if (ticks_started) {
        return;
    }
    ticks_started = SDL_TRUE;

    oops_time_init();
    /*
     * SDL_GetTicks64 is milliseconds since SDL started, not since the machine did.
     * Taking the base here rather than returning the raw counter is what keeps a
     * title's first frame near zero instead of wherever the console happened to be.
     */
    start_ms = oops_time_get_ms();
}

void SDL_TicksQuit(void) {
    ticks_started = SDL_FALSE;
}

Uint64 SDL_GetTicks64(void) {
    if (!ticks_started) {
        SDL_TicksInit();
    }
    return oops_time_get_ms() - start_ms;
}

Uint64 SDL_GetPerformanceCounter(void) {
    if (!ticks_started) {
        SDL_TicksInit();
    }
    return oops_time_get_counter();
}

Uint64 SDL_GetPerformanceFrequency(void) {
    if (!ticks_started) {
        SDL_TicksInit();
    }
    return oops_time_get_counter_frequency();
}

void SDL_Delay(Uint32 ms) {
    oops_time_sleep_ms(ms);
}

#endif /* SDL_TIMER_PROSPERO */
