/*
 * pad-viz payload entry.
 *
 * Executed by a homebrew ELF loader (elfldr) with payload_args in rdi. Opens the display and
 * the pad, then loops: take a batched low-latency read, draw the newest sample as a controller
 * diagram, and flip. Exits on L1+R1+Options held together, so every ordinary button is free to
 * press and watch light up.
 *
 * The drawing is pad-viz.c, shared with the host self-test. This file is the console-only part:
 * the display, the batched input read, and the loop that ties them together.
 */

#include "oops/display.h"
#include "oops/draw.h"
#include "oops/input.h"
#include "oops/krw.h"
#include "oops/syscall.h"
#include "oops/system.h"

#include "pad-viz.h"

/* A line to the system log, the one output a payload always has. */
static void klog(const char *msg) {
    oops_klog("PAD-VIZ", msg);
}

/* The exit gesture: L1, R1 and Options together - a combo no single press triggers, so the
 * face and shoulder buttons stay free to test. */
static int exit_combo(uint32_t buttons) {
    return (buttons & OOPS_BUTTON_L1) && (buttons & OOPS_BUTTON_R1) &&
           (buttons & OOPS_BUTTON_OPTIONS);
}

int padviz_start(const payload_args_t *args);

int padviz_start(const payload_args_t *args) {
    if (args) {
        sys_call_init(args);
    }
    klog("pad-viz payload entry reached");

    oops_display_t *disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, 1280, 720);
    if (!disp || !oops_display_is_ready(disp)) {
        klog("display would not open - see oops_display_get_last_error");
        return -1;
    }
    oops_input_init();

    padviz_state_t state;
    for (size_t i = 0; i < sizeof(state); i++) {
        ((unsigned char *)&state)[i] = 0;
    }

    int running = 1;
    while (running) {
        /* One batched read a frame: up to a full batch of samples in a single driver request.
         * The newest (last) sample is what the diagram draws; the count is shown so the
         * low-latency path is visibly delivering more than one record per frame. */
        oops_pad_state_t batch[OOPS_MAX_PAD_SAMPLES];
        int n = oops_input_poll_batch(0, batch, OOPS_MAX_PAD_SAMPLES);
        if (n > 0) {
            state.pad = batch[n - 1];
            state.sample_count = n;
        } else {
            /* No batched samples this frame (or the batched path is unavailable here): fall
             * back to a single current-state read so the diagram still tracks the pad. */
            oops_pad_state_t one;
            if (oops_input_poll(0, &one) == 0) {
                state.pad = one;
            }
            state.sample_count = 0;
        }

        if (exit_combo(state.pad.buttons)) {
            running = 0;
        }

        if (state.pad.buttons & OOPS_BUTTON_TRIANGLE) {
            oops_input_set_rumble(0, 180, 180);
            oops_input_set_lightbar(0, 255, 100, 0);
        } else {
            oops_input_set_rumble(0, 0, 0);
        }

        oops_surface_t surf = oops_display_get_surface(disp);
        if (surf.pixels) {
            (void)padviz_render(&surf, &state);
        }
        oops_display_flip(disp);
    }

    klog("pad-viz exiting");
    oops_input_close();
    oops_display_close(disp);
    return 0;
}
