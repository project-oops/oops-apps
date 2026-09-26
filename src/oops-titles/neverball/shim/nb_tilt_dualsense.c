/*
 * Tilting the floor by tilting the pad: a DualSense backend for `share/tilt.h`.
 *
 * Upstream picks a tilt backend at Makefile:349-357 and consumes it at
 * `ball/main.c:270`; this file is a fourth arm of that switch, so nothing is patched.
 *
 * `game_set_ang` passes values straight to `input_set_x`/`input_set_z`
 * (ball/game_server.c:894), where the stick path arrives scaled by `ANGLE_BOUND`
 * (ball/game_server.h:7). So these return degrees, which the game clamps, with the
 * stick's signs: `game_set_x` negates and `game_set_z` does not. Axis order, signs,
 * gain and deadzone are knobs in `/app0/oops-input`.
 */
#include "oops/fs.h"
#include "oops/input.h"
#include "oops/system.h"

#include <math.h>

/* Upstream's interface, included so a signature change stops the compile.
 * `-Iupstream/share` is in `NB_CFLAGS`. */
#include "tilt.h"

#include "nb_tilt.h"

/* The tilt knobs, in the input config rather than the renderer's `/app0/oops-gl`. */
#define NB_TILT_CONF "/app0/oops-input"

/* The remembered on/off choice - see `nb_tilt_set_enabled`. */
#define NB_TILT_STATE "/app0/oops-tilt"

static int tilt_have; /* a pad answered at init */
static int tilt_on;   /* the player turned it on */
/* The pad's pitch drives the board's roll and its roll the board's pitch, as measured
 * on hardware. A knob, like the signs. */
static int tilt_swap = 1;
static float tilt_sign_x = -1.0f; /* `game_set_x` negates; match it */
static float tilt_sign_z = 1.0f;
/* Degrees of floor per degree of pad. The floor clamps at `ANGLE_BOUND`, so 0.7 reaches
 * the limit at about 28 degrees of pad - a deliberate movement, not a hand's wander. */
static float tilt_gain = 0.7f;
/* Degrees of pad ignored around level, so the ball does not drift while the pad is held
 * still. */
static float tilt_dead = 2.5f;

/* The attitude that counts as level, applied as a rotation. Kept here rather than using
 * `oops_input_reset_orientation`, which would disturb other orientation readers. */
static float ref_x, ref_y, ref_z, ref_w = 1.0f;

/* A `key=value` from the config file as a float, or `def` when it is absent or
 * unparseable. Accepts a leading sign, digits, one point, digits; no locale-dependent
 * `strtof`. */
static float conf_f(const char *key, float def) {
    char buf[32];
    if (oops_config_value(NB_TILT_CONF, key, buf, sizeof(buf)) != 0)
        return def;

    const char *p = buf;
    float sign = 1.0f;
    if (*p == '-') {
        sign = -1.0f;
        p++;
    } else if (*p == '+') {
        p++;
    }
    if (*p < '0' || *p > '9') {
        if (*p != '.')
            return def;
    }
    float v = 0.0f;
    while (*p >= '0' && *p <= '9') {
        v = v * 10.0f + (float)(*p - '0');
        p++;
    }
    if (*p == '.') {
        p++;
        float scale = 0.1f;
        while (*p >= '0' && *p <= '9') {
            v += (float)(*p - '0') * scale;
            scale *= 0.1f;
            p++;
        }
    }
    return sign * v;
}

/* A `key=on|off|1|0` as a flag. */
static int conf_b(const char *key, int def) {
    char buf[16];
    if (oops_config_value(NB_TILT_CONF, key, buf, sizeof(buf)) != 0)
        return def;
    if (buf[0] == '1' || buf[0] == 'o' || buf[0] == 'O') {
        /* "on" and "off" both start with o, so the second character decides. */
        return !(buf[1] == 'f' || buf[1] == 'F');
    }
    if (buf[0] == 'y' || buf[0] == 'Y' || buf[0] == 't' || buf[0] == 'T')
        return 1;
    return 0;
}

void tilt_init(void) {
    oops_pad_state_t pad;

    tilt_swap = conf_b("tilt-swap", 1);
    tilt_sign_x = conf_b("tilt-invert-x", 0) ? 1.0f : -1.0f;
    tilt_sign_z = conf_b("tilt-invert-z", 0) ? -1.0f : 1.0f;
    tilt_gain = conf_f("tilt-gain", 0.7f);
    tilt_dead = conf_f("tilt-deadzone", 2.5f);

    /* The pad must answer with a real attitude. An all-zero quaternion is not a
     * rotation, and the option is then not offered. */
    tilt_have = 0;
    if (oops_input_poll(0u, &pad) == 0 && pad.connected) {
        const float w = pad.orientation[3];
        const float x = pad.orientation[0];
        const float y = pad.orientation[1];
        const float z = pad.orientation[2];
        if (w * w + x * x + y * y + z * z > 0.5f)
            tilt_have = 1;
    }

    /* The saved choice wins, then a `tilt=` preset in the config, then off. */
    tilt_on = tilt_have && conf_b("tilt", 0);
    {
        void *saved = (void *)0;
        size_t n = 0;
        if (oops_fs_read_all(NB_TILT_STATE, &saved, &n) == 0 && saved) {
            if (n >= 1u)
                tilt_on = tilt_have && (*(const char *)saved == '1');
            oops_fs_free_data(saved);
        }
    }
    if (tilt_on)
        nb_tilt_recentre();

    oops_log_info("TILT", "motion tilt: pad=%d enabled=%d gain=%d/100 deadzone=%d/100",
                  tilt_have, tilt_on, (int)(tilt_gain * 100.0f),
                  (int)(tilt_dead * 100.0f));
}

void tilt_free(void) {
    tilt_on = 0;
}

int tilt_stat(void) {
    /* This switches the game's input mode (`ball/main.c:270` feeds `st_angle` instead
     * of the stick), so it is the player's choice, not pad presence. */
    return tilt_on;
}

int tilt_get_button(int *b, int *s) {
    /* The pad's buttons already arrive through SDL; reporting them here would double
       every press. */
    (void)b;
    (void)s;
    return 0;
}

/* The pad's attitude relative to level, as pitch and roll in degrees.
 *
 * `q_rel = conj(ref) * q`, then the standard ZYX extraction. Both angles come from one
 * poll so they describe the same instant. */
static void tilt_angles(float *out_pitch, float *out_roll) {
    oops_pad_state_t pad;

    *out_pitch = 0.0f;
    *out_roll = 0.0f;

    if (oops_input_poll(0u, &pad) != 0 || !pad.connected)
        return;

    /*
     * The touchpad click re-centres: level is this file's reference, not the sensor's,
     * and Neverball binds nothing to the touchpad. Edge triggered, so holding it does
     * not make the floor follow the pad.
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
    float roll =
        atan2f(2.0f * (w * x + y * z), 1.0f - 2.0f * (x * x + y * y)) * rad_to_deg;

    /* Pitch about the lateral axis, with the argument clamped: a quaternion slightly
       past unit length makes `asinf` return a NaN, which `input_set_x` would keep. */
    float s = 2.0f * (w * y - z * x);
    if (s > 1.0f)
        s = 1.0f;
    if (s < -1.0f)
        s = -1.0f;
    float pitch = asinf(s) * rad_to_deg;

    if (tilt_swap) {
        const float t = pitch;
        pitch = roll;
        roll = t;
    }

    /* The deadzone is subtracted from the magnitude, so the response is continuous from
     * level with no step at the boundary. */
    if (pitch > tilt_dead)
        pitch -= tilt_dead;
    else if (pitch < -tilt_dead)
        pitch += tilt_dead;
    else
        pitch = 0.0f;

    if (roll > tilt_dead)
        roll -= tilt_dead;
    else if (roll < -tilt_dead)
        roll += tilt_dead;
    else
        roll = 0.0f;

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
     * The choice persists in its own one-byte file, so the config in `/app0/oops-input`
     * is never rewritten. `tilt_init` reads this first and falls back to `tilt=`.
     */
    {
        const char v = tilt_on ? '1' : '0';
        if (oops_fs_write_all(NB_TILT_STATE, &v, 1u) != 0) {
            oops_log_warn("TILT",
                          "could not write %s - the choice will not survive the launch",
                          NB_TILT_STATE);
        }
    }

    /* The pad's attitude when switched on becomes level. */
    if (tilt_on)
        nb_tilt_recentre();

    oops_log_info("TILT", "motion tilt %s", tilt_on ? "on" : "off");
}
