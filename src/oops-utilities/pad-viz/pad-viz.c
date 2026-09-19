#include "pad-viz.h"

#include "oops/draw.h"

#define BG     0xFF0D1116u
#define DIM    OOPS_COLOR_GRAY
#define HOT    OOPS_COLOR_CYAN
#define VALUE  OOPS_COLOR_WHITE
#define ACCENT OOPS_COLOR_CYAN
#define LABEL  OOPS_COLOR_GRAY

/* Freestanding decimal, so this file compiles for the target as well as the host. */
static const char *u32_dec(uint32_t value, char *out) {
    char rev[11];
    int n = 0;
    do {
        rev[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u && n < 10);
    int i = 0;
    while (n > 0) {
        out[i++] = rev[--n];
    }
    out[i] = '\0';
    return out;
}

/* A one-pixel-thick rectangle border, since draw offers only filled rects. */
static void rect_outline(oops_surface_t *s, int x, int y, int w, int h, oops_color_t c) {
    oops_draw_rect(s, x, y, w, 2, c);
    oops_draw_rect(s, x, y + h - 2, w, 2, c);
    oops_draw_rect(s, x, y, 2, h, c);
    oops_draw_rect(s, x + w - 2, y, 2, h, c);
}

static int is_down(uint32_t buttons, uint32_t mask) {
    return (buttons & mask) != 0;
}

/* A rectangular button: filled when pressed, outlined when not. */
static void btn_rect(oops_surface_t *s, int x, int y, int w, int h, int pressed) {
    if (pressed) {
        oops_draw_rect(s, x, y, w, h, HOT);
    } else {
        rect_outline(s, x, y, w, h, DIM);
    }
}

/* A round face button with a centred one-letter label. */
static void face_btn(oops_surface_t *s, int cx, int cy, int pressed, const char *label) {
    if (pressed) {
        oops_draw_circle(s, cx, cy, 22, HOT, 1);
    } else {
        oops_draw_circle(s, cx, cy, 22, DIM, 0);
    }
    oops_draw_text(s, cx - 6, cy - 8, label, pressed ? BG : VALUE, 2);
}

/* An analog stick: a ring with a thumb that moves with the axes; the thumb lights when the
 * stick is clicked (L3/R3). */
static void stick(oops_surface_t *s, int cx, int cy, int r, int sx, int sy, int clicked) {
    oops_draw_circle(s, cx, cy, r, DIM, 0);
    oops_draw_circle(s, cx, cy, 10, DIM, 0);
    int dx = sx * (r - 14) / 128;
    int dy = sy * (r - 14) / 128;
    oops_draw_circle(s, cx + dx, cy + dy, 14, clicked ? HOT : VALUE, 1);
}

/* An analog trigger: an outlined bar filled left-to-right by the 0..255 value. */
static void trigger_bar(oops_surface_t *s, int x, int y, int w, int h, uint8_t v) {
    rect_outline(s, x, y, w, h, DIM);
    int fill = (int)v * (w - 4) / 255;
    if (fill > 0) {
        oops_draw_rect(s, x + 2, y + 2, fill, h - 4, ACCENT);
    }
}

/* The touch-pad: an outline that lights when clicked, with up to two live contacts mapped from
 * the reported resolution into the drawn rectangle. */
static void touchpad(oops_surface_t *s, int x, int y, int w, int h,
                     const oops_pad_state_t *pad) {
    int clicked = is_down(pad->buttons, OOPS_BUTTON_TOUCHPAD);
    rect_outline(s, x, y, w, h, clicked ? HOT : DIM);
    for (int t = 0; t < 2; t++) {
        if (!pad->touch[t].active) {
            continue;
        }
        /* Reported touch resolution is ~1920x942; map into the drawn pad. */
        int px = x + (int)((uint32_t)pad->touch[t].x * (uint32_t)(w - 8) / 1919u) + 4;
        int py = y + (int)((uint32_t)pad->touch[t].y * (uint32_t)(h - 8) / 941u) + 4;
        oops_draw_circle(s, px, py, 8, ACCENT, 1);
    }
}

/* A tilt box: a dot driven by the accelerometer's x/y, clamped to +/-1 g. */
static void tilt_box(oops_surface_t *s, int x, int y, int w, int h, const float accel[3]) {
    rect_outline(s, x, y, w, h, DIM);
    float ax = accel[0];
    float ay = accel[1];
    if (ax < -1.0f) ax = -1.0f; else if (ax > 1.0f) ax = 1.0f;
    if (ay < -1.0f) ay = -1.0f; else if (ay > 1.0f) ay = 1.0f;
    int dotx = x + w / 2 + (int)(ax * (float)(w / 2 - 12));
    int doty = y + h / 2 + (int)(ay * (float)(h / 2 - 12));
    oops_draw_circle(s, dotx, doty, 10, VALUE, 1);
}

int padviz_render(oops_surface_t *surf, const padviz_state_t *state) {
    if (!surf || !state) {
        return -1;
    }
    const oops_pad_state_t *pad = &state->pad;
    uint32_t b = pad->buttons;

#ifndef OOPS_APP_VERSION
#define OOPS_APP_VERSION "dev"
#endif

    oops_draw_clear(surf, BG);
    oops_draw_text(surf, 48, 36, "OOPS pad", ACCENT, 4);

    /* UI version placeholder */
    char ver_buf[64];
    int vi = 0;
    ver_buf[vi++] = 'v';
    ver_buf[vi++] = ' ';
    const char *vp = OOPS_APP_VERSION;
    while (*vp && vi < (int)sizeof(ver_buf) - 1) {
        ver_buf[vi++] = *vp++;
    }
    ver_buf[vi] = '\0';
    oops_draw_text(surf, 330, 48, ver_buf, LABEL, 2);

    oops_draw_text(surf, 1000, 44, pad->connected ? "connected" : "no pad",
                   pad->connected ? OOPS_COLOR_GREEN : OOPS_COLOR_RED, 2);

    /* Shoulders and triggers, top corners. */
    trigger_bar(surf, 250, 150, 170, 26, pad->l2_trigger);
    btn_rect(surf, 250, 184, 170, 30, is_down(b, OOPS_BUTTON_L1));
    oops_draw_text(surf, 250, 128, "L2 / L1", LABEL, 2);
    trigger_bar(surf, 860, 150, 170, 26, pad->r2_trigger);
    btn_rect(surf, 860, 184, 170, 30, is_down(b, OOPS_BUTTON_R1));
    oops_draw_text(surf, 860, 128, "R2 / R1", LABEL, 2);

    /* Centre: Create, touch-pad, Options. */
    btn_rect(surf, 470, 236, 40, 22, is_down(b, OOPS_BUTTON_CREATE));
    oops_draw_text(surf, 470, 214, "Create", LABEL, 2);
    touchpad(surf, 530, 150, 220, 120, pad);
    btn_rect(surf, 770, 236, 40, 22, is_down(b, OOPS_BUTTON_OPTIONS));
    oops_draw_text(surf, 770, 214, "Options", LABEL, 2);

    /* D-pad, left. */
    btn_rect(surf, 303, 360, 34, 34, is_down(b, OOPS_BUTTON_UP));
    btn_rect(surf, 303, 428, 34, 34, is_down(b, OOPS_BUTTON_DOWN));
    btn_rect(surf, 269, 394, 34, 34, is_down(b, OOPS_BUTTON_LEFT));
    btn_rect(surf, 337, 394, 34, 34, is_down(b, OOPS_BUTTON_RIGHT));

    /* Face buttons, right: triangle, circle, cross, square. */
    face_btn(surf, 980, 366, is_down(b, OOPS_BUTTON_TRIANGLE), "T");
    face_btn(surf, 1024, 410, is_down(b, OOPS_BUTTON_CIRCLE), "O");
    face_btn(surf, 980, 454, is_down(b, OOPS_BUTTON_CROSS), "X");
    face_btn(surf, 936, 410, is_down(b, OOPS_BUTTON_SQUARE), "S");

    /* Sticks (thumb lights on L3/R3), and the tilt box between them. */
    stick(surf, 470, 470, 64, pad->left_stick_x, pad->left_stick_y, is_down(b, OOPS_BUTTON_L3));
    stick(surf, 810, 470, 64, pad->right_stick_x, pad->right_stick_y, is_down(b, OOPS_BUTTON_R3));
    oops_draw_text(surf, 560, 320, "tilt", LABEL, 2);
    tilt_box(surf, 560, 344, 160, 150, pad->acceleration);

    /* Footer: the batched-read sample count (proof the low-latency path delivers), and how to
     * leave. */
    char num[11];
    char footer[48];
    int k = 0;
    const char *pfx = "samples: ";
    while (*pfx) footer[k++] = *pfx++;
    const char *p = u32_dec((uint32_t)(state->sample_count < 0 ? 0 : state->sample_count), num);
    while (*p) footer[k++] = *p++;
    footer[k] = '\0';
    oops_draw_text(surf, 48, (int)surf->height - 52, footer, VALUE, 2);
    oops_draw_text(surf, 260, (int)surf->height - 52, "Triangle: rumble | L1+R1+Options to exit", LABEL, 2);
    return 0;
}
