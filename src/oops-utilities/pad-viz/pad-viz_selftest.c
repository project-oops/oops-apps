/*
 * Host self-test for pad-viz.
 *
 * Renders several controller states into a plain buffer and checks each drew and none
 * wrote past the surface. It does not check what the diagram says - on a host there is
 * no pad - only that the drawing is sound for neutral, fully-pressed, deflected, and
 * touched states.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oops/draw.h"

#include "pad-viz.h"

#define W 1280
#define H 720
#define GUARD 0xDEADBEEFu

static int render_one(uint32_t *pixels, const padviz_state_t *state, const char *name) {
    oops_surface_t surf = {.pixels = pixels, .width = W, .height = H, .pitch = W};
    for (size_t i = 0; i < (size_t)W * H; i++) {
        pixels[i] = 0;
    }
    pixels[(size_t)W * H] = GUARD;

    if (padviz_render(&surf, state) != 0) {
        fprintf(stderr, "pad-viz selftest: render(%s) returned error\n", name);
        return 0;
    }
    int any = 0;
    for (size_t i = 0; i < (size_t)W * H; i++) {
        if (pixels[i] != 0) {
            any = 1;
            break;
        }
    }
    if (!any) {
        fprintf(stderr, "pad-viz selftest: state '%s' drew nothing\n", name);
        return 0;
    }
    if (pixels[(size_t)W * H] != GUARD) {
        fprintf(stderr, "pad-viz selftest: state '%s' wrote past the surface\n", name);
        return 0;
    }
    return 1;
}

int main(void) {
    uint32_t *pixels = malloc(((size_t)W * H + 1) * sizeof(uint32_t));
    if (!pixels) {
        fprintf(stderr, "pad-viz selftest: out of memory\n");
        return 1;
    }

    int ok = 1;
    padviz_state_t state;

    /* Neutral: nothing pressed, sticks centred. */
    memset(&state, 0, sizeof(state));
    state.pad.connected = 1;
    ok &= render_one(pixels, &state, "neutral");

    /* Everything pressed, triggers full, sticks hard over, two touches, tilted. */
    memset(&state, 0, sizeof(state));
    state.pad.connected = 1;
    state.pad.buttons = 0xFFFFFFFFu;
    state.pad.l2_trigger = 255;
    state.pad.r2_trigger = 255;
    state.pad.left_stick_x = 127;
    state.pad.left_stick_y = -128;
    state.pad.right_stick_x = -128;
    state.pad.right_stick_y = 127;
    state.pad.touch[0].active = 1;
    state.pad.touch[0].x = 0;
    state.pad.touch[0].y = 0;
    state.pad.touch[1].active = 1;
    state.pad.touch[1].x = 1919;
    state.pad.touch[1].y = 941;
    state.pad.acceleration[0] = 2.0f;  /* clamped */
    state.pad.acceleration[1] = -2.0f; /* clamped */
    state.sample_count = 7;
    ok &= render_one(pixels, &state, "all-pressed");

    /* Disconnected, mid-deflection, negative sample count guarded. */
    memset(&state, 0, sizeof(state));
    state.pad.connected = 0;
    state.pad.left_stick_x = 40;
    state.pad.left_stick_y = -60;
    state.pad.l2_trigger = 128;
    state.sample_count = -1;
    ok &= render_one(pixels, &state, "disconnected");

    /* Bad arguments are rejected without touching memory. */
    if (padviz_render(NULL, &state) != -1) {
        fprintf(stderr, "pad-viz selftest: null surface not rejected\n");
        ok = 0;
    }
    oops_surface_t surf = {.pixels = pixels, .width = W, .height = H, .pitch = W};
    if (padviz_render(&surf, NULL) != -1) {
        fprintf(stderr, "pad-viz selftest: null state not rejected\n");
        ok = 0;
    }

    free(pixels);
    if (ok) {
        printf(
            "pad-viz selftest: ok (states rendered; draw path and bounds verified)\n");
        return 0;
    }
    return 1;
}
