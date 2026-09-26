/*
 * SDL's timer backend over `oops/time.h`. Upstream's `src/timer/unix/` calls
 * `clock_gettime` and `nanosleep`, which the SDK does not expose.
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
    /* SDL_GetTicks64 counts milliseconds since SDL started, not since boot. */
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
