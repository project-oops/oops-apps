/*
 * Tilting the floor by tilting the pad.
 *
 * # Why this is a shim and not a patch
 *
 * Neverball has had a tilt-sensor interface since it supported the Wiimote. `share/tilt.h` is six
 * functions, upstream's own Makefile picks one backend out of `tilt_wii.c`, `tilt_loop.c` and
 * `tilt_null.c` (Makefile:349-357), and the whole of the game-side integration is four lines at
 * `ball/main.c:270`:
 *
 *     if (tilt_stat())
 *     {
 *         st_angle(tilt_get_x(), tilt_get_z());
 *         while (tilt_get_button(&b, &s)) ...
 *     }
 *
 * So a DualSense backend is not a change to upstream at all. It is a fourth arm of a switch
 * upstream already wrote, and our Makefile chooses it the way upstream's chooses the others -
 * `tilt_null` comes out of `NB_SHARE` and this file goes into `PAYLOAD_SRCS`. Nothing is patched,
 * nothing is rebased, and the next upstream bump does not know this exists.
 *
 * # The units
 *
 * `game_set_ang` passes what it is given straight into `input_set_x`/`input_set_z`
 * (ball/game_server.c:894), which is the same place the stick's own path arrives at after
 * multiplying by `ANGLE_BOUND` - twenty degrees, ball/game_server.h:7. So these return **degrees**
 * and the game clamps them. The sign convention is the stick's: `game_set_x` negates and
 * `game_set_z` does not.
 *
 * # What is not settled
 *
 * Which physical axis of the pad is pitch and which is roll, and which way round each runs, is a
 * property of how the IMU is mounted and how far the player is sitting from upright. It is one
 * short session with a pad to get right and it cannot be got right by reading. So every part of
 * it is a knob in `/app0/oops-input` - `tilt-swap`, `tilt-invert-x`, `tilt-invert-z`, and a
 * sensitivity - and the defaults below are the standard aerospace conversion rather than a
 * measurement. Changing one costs a `pros restore` of a 40-byte file, not a rebuild.
 */
#include "oops/fs.h"
#include "oops/input.h"
#include "oops/system.h"

#include <math.h>

/* **Upstream's own interface, included rather than restated.** This file is in `PAYLOAD_SRCS`,
 * so it is held to the repository's `-Werror -Wmissing-prototypes` - which is what caught the
 * first version of it declaring nothing. Including `tilt.h` is also the check that matters: if a
 * future upstream changes one of these six signatures, this stops compiling instead of quietly
 * defining a function nobody calls. `-Iupstream/share` is in `NB_CFLAGS`. */
#include "tilt.h"

#include "nb_tilt.h"

/* Where the knobs live. Its own file rather than `/app0/oops-gl`, because that one belongs to the
   renderer and this is input - a reader should not have to know that a tilt setting ended up in
   the graphics file. */
#define NB_TILT_CONF "/app0/oops-input"

/* Where the on/off choice is remembered - see `nb_tilt_set_enabled` for why it is its own file
   and not a line in the one above. */
#define NB_TILT_STATE "/app0/oops-tilt"

static int   tilt_have;      /* a pad answered at init */
static int   tilt_on;        /* the player turned it on */
/*
 * **Pitch and roll go to the other axis, and this is measured rather than derived.**
 *
 * The first hardware session reported it exactly: tilting the pad forwards, raising the rear,
 * tilted the board sideways and raised its right edge; tilting the pad right, raising its left,
 * tilted the board backwards and raised its front. That is the two axes swapped - the pad's
 * pitch driving the board's roll and vice versa - which is the one thing the file header said
 * could not be settled by reading and would need a session with a pad. It did, and this is the
 * answer.
 *
 * Still a knob, because the *signs* are a separate question from the order and one of them may
 * yet be wrong in a way that only feels wrong rather than looking it.
 */
static int   tilt_swap = 1;
static float tilt_sign_x = -1.0f; /* `game_set_x` negates; match it */
static float tilt_sign_z = 1.0f;
/* **Degrees of floor per degree of pad, and 1.6 was far too much.**
 *
 * The floor clamps at `ANGLE_BOUND`, twenty degrees, so a gain of 1.6 put the floor hard over
 * at twelve and a half degrees of wrist - which is inside the range a hand wanders through while
 * simply holding a controller. Reported as "too aggressive" on the first hardware run, and it
 * was. At 0.7 the floor reaches its limit at about twenty-eight degrees of pad, which is a
 * deliberate movement rather than a twitch. */
static float tilt_gain = 0.7f;
/* Degrees of pad ignored around level. Raised with the gain: a wider dead band is what stops the
 * ball drifting while the player thinks the pad is still. */
static float tilt_dead = 2.5f;

/* The attitude that counts as level, as a conjugated quaternion. `oops_input_reset_orientation`
 * exists and zeroes the pad's own reference, but it cannot be called from a menu without also
 * disturbing anything else reading orientation - and re-centring is a thing this backend wants to
 * do often. So level is kept here and applied as a rotation, which is the same arithmetic and
 * touches nothing outside this file. */
static float ref_x, ref_y, ref_z, ref_w = 1.0f;

/* A `key=value` from the config file as a float, or `def` when it is absent or unparseable.
 *
 * Its own parser because the SDK's reader hands back text and this needs three signed decimals;
 * `strtof` is in the libc but taking a dependency on locale-shaped parsing for "1.6" is more
 * than the job needs. Accepts a leading sign, digits, one point, digits. */
static float conf_f(const char *key, float def) {
    char buf[32];
    if (oops_config_value(NB_TILT_CONF, key, buf, sizeof(buf)) != 0) return def;

    const char *p = buf;
    float sign = 1.0f;
    if (*p == '-') { sign = -1.0f; p++; } else if (*p == '+') { p++; }
    if (*p < '0' || *p > '9') {
        if (*p != '.') return def;
    }
    float v = 0.0f;
    while (*p >= '0' && *p <= '9') { v = v * 10.0f + (float)(*p - '0'); p++; }
    if (*p == '.') {
        p++;
        float scale = 0.1f;
        while (*p >= '0' && *p <= '9') { v += (float)(*p - '0') * scale; scale *= 0.1f; p++; }
    }
    return sign * v;
}

/* A `key=on|off|1|0` as a flag. */
static int conf_b(const char *key, int def) {
    char buf[16];
    if (oops_config_value(NB_TILT_CONF, key, buf, sizeof(buf)) != 0) return def;
    if (buf[0] == '1' || buf[0] == 'o' || buf[0] == 'O') {
        /* "on" and "off" both start with o, so the second character decides. */
        return !(buf[1] == 'f' || buf[1] == 'F');
    }
    if (buf[0] == 'y' || buf[0] == 'Y' || buf[0] == 't' || buf[0] == 'T') return 1;
    return 0;
}

void tilt_init(void) {
    oops_pad_state_t pad;

    tilt_swap = conf_b("tilt-swap", 1);
    tilt_sign_x = conf_b("tilt-invert-x", 0) ? 1.0f : -1.0f;
    tilt_sign_z = conf_b("tilt-invert-z", 0) ? -1.0f : 1.0f;
    tilt_gain = conf_f("tilt-gain", 0.7f);
    tilt_dead = conf_f("tilt-deadzone", 2.5f);

    /* **The pad has to answer, and its IMU has to be saying something.** A quaternion of all
     * zeroes is not a rotation, and a controller that reports one has no usable orientation -
     * offering motion tilt for it would be an option that does nothing, which is worse than no
     * option. `w` is 1 at rest and never 0 for a real attitude. */
    tilt_have = 0;
    if (oops_input_poll(0u, &pad) == 0 && pad.connected) {
        const float w = pad.orientation[3];
        const float x = pad.orientation[0];
        const float y = pad.orientation[1];
        const float z = pad.orientation[2];
        if (w * w + x * x + y * y + z * z > 0.5f) tilt_have = 1;
    }

    /* Off unless the player asked for it - see `nb_tilt_enabled`. Last session's choice wins,
       then a `tilt=` preset in the config, then off. */
    tilt_on = tilt_have && conf_b("tilt", 0);
    {
        void *saved = (void *)0;
        size_t n = 0;
        if (oops_fs_read_all(NB_TILT_STATE, &saved, &n) == 0 && saved) {
            if (n >= 1u) tilt_on = tilt_have && (*(const char *)saved == '1');
            oops_fs_free_data(saved);
        }
    }
    if (tilt_on) nb_tilt_recentre();

    oops_log_info("TILT", "motion tilt: pad=%d enabled=%d gain=%d/100 deadzone=%d/100",
                  tilt_have, tilt_on, (int)(tilt_gain * 100.0f), (int)(tilt_dead * 100.0f));
}

void tilt_free(void) {
    tilt_on = 0;
}

int tilt_stat(void) {
    /* **This is what switches the game's input mode**, so it has to be the player's choice and
     * not merely "a pad is present". `ball/main.c:270` feeds `st_angle` instead of the stick
     * whenever this is true. */
    return tilt_on;
}

int tilt_get_button(int *b, int *s) {
    /* The Wiimote backend reports its own buttons here because it *is* the controller. The pad's
       buttons already arrive through SDL, and reporting them twice would double every press. */
    (void)b;
    (void)s;
    return 0;
}

/* The pad's attitude relative to level, as pitch and roll in degrees.
 *
 * `q_rel = conj(ref) * q`, then the standard ZYX extraction. Both angles come out of one poll so
 * they describe the same instant - reading the pad twice, once per accessor, would let the two
 * halves of a fast flick disagree. */
static void tilt_angles(float *out_pitch, float *out_roll) {
    oops_pad_state_t pad;

    *out_pitch = 0.0f;
    *out_roll = 0.0f;

    if (oops_input_poll(0u, &pad) != 0 || !pad.connected) return;

    /*
     * **The touchpad re-centres, and there has to be something that does.**
     *
     * Level is whatever the pad was doing when motion was switched on, so a player who turns it
     * on while slouched and then sits up finds the floor permanently tipped - and *putting the
     * pad flat on a table does not fix it*, which was the first thing tried on hardware and the
     * most reasonable thing to try. The reference is ours, not the sensor's, so only we can
     * clear it.
     *
     * The touchpad click because Neverball binds nothing to it: Options is the pause menu, the
     * face buttons are the camera and the menus, and the shoulders rotate the view. Edge
     * triggered, so holding it does not re-centre sixty times a second - which would make the
     * floor follow the pad instead of reading its angle.
     */
    {
        static int touch_was_down;
        const int touch_down = (pad.buttons & OOPS_BUTTON_TOUCHPAD) ? 1 : 0;

        if (touch_down && !touch_was_down) {
            ref_x = pad.orientation[0];
            ref_y = pad.orientation[1];
            ref_z = pad.orientation[2];
            ref_w = pad.orientation[3];
            oops_log_info("TILT", "re-centred");
        }
        touch_was_down = touch_down;
    }

    const float qx = pad.orientation[0], qy = pad.orientation[1];
    const float qz = pad.orientation[2], qw = pad.orientation[3];

    /* conj(ref) * q */
    const float rx = -ref_x, ry = -ref_y, rz = -ref_z, rw = ref_w;
    const float x = rw * qx + rx * qw + ry * qz - rz * qy;
    const float y = rw * qy - rx * qz + ry * qw + rz * qx;
    const float z = rw * qz + rx * qy - ry * qx + rz * qw;
    const float w = rw * qw - rx * qx - ry * qy - rz * qz;

    const float rad_to_deg = 57.2957795f;

    /* Roll about the forward axis. */
    float roll = atan2f(2.0f * (w * x + y * z), 1.0f - 2.0f * (x * x + y * y)) * rad_to_deg;

    /* Pitch about the lateral axis, with the argument clamped: a quaternion that has drifted a
       hair past unit length makes `asinf` return a NaN, and a NaN reaching `input_set_x` tilts
       the floor to nowhere and stays there. */
    float s = 2.0f * (w * y - z * x);
    if (s > 1.0f) s = 1.0f;
    if (s < -1.0f) s = -1.0f;
    float pitch = asinf(s) * rad_to_deg;

    if (tilt_swap) {
        const float t = pitch;
        pitch = roll;
        roll = t;
    }

    /* **The deadzone is subtracted, not zeroed through.** Zeroing inside it and passing the raw
     * angle outside puts a step at the boundary, so the floor jumps the moment the player moves
     * past it. Taking the deadzone off the magnitude keeps the response continuous from level. */
    if (pitch > tilt_dead) pitch -= tilt_dead;
    else if (pitch < -tilt_dead) pitch += tilt_dead;
    else pitch = 0.0f;

    if (roll > tilt_dead) roll -= tilt_dead;
    else if (roll < -tilt_dead) roll += tilt_dead;
    else roll = 0.0f;

    *out_pitch = pitch * tilt_gain * tilt_sign_x;
    *out_roll = roll * tilt_gain * tilt_sign_z;
}

float tilt_get_x(void) {
    float pitch, roll;
    tilt_angles(&pitch, &roll);
    return pitch;
}

float tilt_get_z(void) {
    float pitch, roll;
    tilt_angles(&pitch, &roll);
    return roll;
}

/*---------------------------------------------------------------------------*/
/* This port's own three, declared in `nb_tilt.h`. */

int nb_tilt_available(void) {
    return tilt_have;
}

int nb_tilt_enabled(void) {
    return tilt_on;
}

void nb_tilt_recentre(void) {
    oops_pad_state_t pad;

    if (oops_input_poll(0u, &pad) == 0 && pad.connected) {
        ref_x = pad.orientation[0];
        ref_y = pad.orientation[1];
        ref_z = pad.orientation[2];
        ref_w = pad.orientation[3];
    }
}

void nb_tilt_set_enabled(int on) {
    if (!tilt_have) {
        tilt_on = 0;
        return;
    }
    tilt_on = on ? 1 : 0;

    /*
     * **Written down, because `nb_tilt.h` said it was and it was not.**
     *
     * The header promised this survived the launch from the day it was written; nothing did the
     * writing, so the first hardware session had to turn motion tilt on again every single run.
     * That is the same class of mistake as the Controls page telling a player to press Options
     * to re-centre when nothing was bound to it - a comment describing an intention as though it
     * were behaviour.
     *
     * Its own one-byte file rather than a `tilt=` line in `/app0/oops-input`: that file also
     * holds the gain, the deadzone and the axis knobs, and rewriting it from here to change one
     * value would mean parsing and re-emitting the rest, with a truncated file as the cost of
     * getting it wrong. `tilt_init` reads this first and falls back to the `tilt=` key, so a
     * preset in the config still works and this only ever overrides it.
     */
    {
        const char v = tilt_on ? '1' : '0';
        if (oops_fs_write_all(NB_TILT_STATE, &v, 1u) != 0) {
            oops_log_warn("TILT", "could not write %s - the choice will not survive the launch",
                          NB_TILT_STATE);
        }
    }

    /* **Whatever way the pad is being held becomes level**, at the moment it is switched on. A
       player reaches for this while slouched, and a fixed reference would start them with the
       floor already tipped. */
    if (tilt_on) nb_tilt_recentre();

    oops_log_info("TILT", "motion tilt %s", tilt_on ? "on" : "off");
}
