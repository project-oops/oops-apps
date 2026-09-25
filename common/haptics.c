/*
 * The bodies behind `haptics.h`. See that file for why a decay thread rather than a frame hook.
 *
 * # The two motors
 *
 * A DualSense has a heavy motor and a light one, and `oops_input_set_rumble` takes both. A hit
 * reads best as mostly the heavy one with a little of the light on top: the heavy motor carries
 * the weight of the impact and the light one gives it an edge. Driving them equally reads as a
 * phone on vibrate.
 *
 * # Why it decays rather than switching off
 *
 * A square pulse - full on, then nothing - reads as a click, because the motors have enough
 * inertia that starting is felt and stopping is not. Ramping down over a few frames is what makes
 * it read as an impact with mass behind it, which is the point for a rolling ball.
 */
#include "haptics.h"

#include "oops/input.h"
#include "oops/system.h"
#include "oops/thread.h"
#include "oops/time.h"

/* How often the thread re-sends the motor levels. Sixteen milliseconds is about a frame; going
   finer costs syscalls for a change no hand can feel. */
#define HAPTICS_TICK_MS 16u

/* Fraction of the level that survives each tick. 0.82^4 is about a third, so a hit is most of
   the way down in four ticks - roughly 60ms, which is an impact rather than a buzz. */
#define HAPTICS_DECAY 0.82f

/* Below this the motors are off rather than nearly off: a level that never quite reaches zero
   leaves them humming inaudibly and draining the pad. */
#define HAPTICS_FLOOR 0.02f

#define HAPTICS_CONF "/app0/oops-input"

static oops_thread_t haptics_thread;
static volatile int haptics_running;
static volatile int haptics_on = 1;
/* Scaled to an integer because it is written from the game and read from the thread, and a float
   store is not guaranteed to be seen whole. 0..1000. */
static volatile int haptics_level;

static void haptics_send(int level_milli) {
    if (level_milli < 0) level_milli = 0;
    if (level_milli > 1000) level_milli = 1000;

    /* The heavy motor carries the hit and the light one sharpens it - see the header. */
    const int heavy = (level_milli * 255) / 1000;
    const int light = (level_milli * 255) / 1000 / 3;

    oops_input_set_rumble(0u, (unsigned char)light, (unsigned char)heavy);
}

static void *haptics_loop(void *arg) {
    (void)arg;

    int last_sent = -1;

    while (haptics_running) {
        int level = haptics_level;

        if (level > 0) {
            /* **Decayed here and stored back**, so a bump arriving mid-tick raises the level the
               next tick starts from rather than being overwritten by this one's result. */
            int next = (int)((float)level * HAPTICS_DECAY);
            if (next < (int)(HAPTICS_FLOOR * 1000.0f)) next = 0;
            haptics_level = next;
        }

        /* **Only on a change.** The motors hold what they were last told, so re-sending the same
           level every tick is a syscall per frame for nothing - and sending zero repeatedly is
           the same waste on the whole of a quiet menu. */
        if (level != last_sent) {
            haptics_send(haptics_on ? level : 0);
            last_sent = level;
        }

        oops_time_sleep_ms(HAPTICS_TICK_MS);
    }

    /* Silent on the way out, whatever the level was - see the header. */
    haptics_send(0);
    return (void *)0;
}

/* `key=on|off` from the config file. */
static int haptics_conf_on(const char *key, int def) {
    char buf[16];
    if (oops_config_value(HAPTICS_CONF, key, buf, sizeof(buf)) != 0) return def;
    if (buf[0] == '0' || buf[0] == 'n' || buf[0] == 'N') return 0;
    if ((buf[0] == 'o' || buf[0] == 'O') && (buf[1] == 'f' || buf[1] == 'F')) return 0;
    return 1;
}

int oops_haptics_init(void) {
    if (haptics_running) return 0;

    haptics_on = haptics_conf_on("rumble", 1);
    haptics_level = 0;
    haptics_running = 1;

    /* **A default priority and a small stack.** The loop holds two ints and calls two SDK
     * functions, so 64 KiB is generous; and it must not outrank the game - a rumble tick that
     * preempts the physics would trade a felt bump for a dropped frame. 0 is the SDK's own
     * default priority. */
    haptics_thread = oops_thread_create("oops-haptics", haptics_loop, (void *)0, 0, 64u * 1024u);

    if (!haptics_thread) {
        /* **Every entry point becomes a no-op rather than a direct call.** Setting the motors
         * from the caller's thread with nothing to turn them off is worse than no rumble: the
         * pad runs until the title exits. */
        haptics_running = 0;
        oops_log_warn("HAPTIC", "no decay thread - rumble is disabled for this run");
        return -1;
    }

    oops_log_info("HAPTIC", "rumble ready (enabled=%d)", haptics_on);
    return 0;
}

void oops_haptics_quit(void) {
    if (!haptics_running) return;
    haptics_running = 0;
    if (haptics_thread) {
        oops_thread_join(haptics_thread, (void **)0);
        haptics_thread = (oops_thread_t)0;
    }
    haptics_send(0);
}

void oops_haptics_bump(float strength) {
    if (!haptics_running || !haptics_on) return;

    if (strength <= 0.0f) return;
    if (strength > 1.0f) strength = 1.0f;

    const int level = (int)(strength * 1000.0f);

    /* **Raised, never replaced.** A ball rattling down a slope fires a stream of small bounces,
       and letting the newest one set the level would cut off the big hit that started them. */
    if (level > haptics_level) haptics_level = level;
}

void oops_haptics_silence(void) {
    haptics_level = 0;
    haptics_send(0);
}

int oops_haptics_enabled(void) {
    return haptics_on;
}

void oops_haptics_set_enabled(int on) {
    haptics_on = on ? 1 : 0;
    if (!haptics_on) oops_haptics_silence();
}
