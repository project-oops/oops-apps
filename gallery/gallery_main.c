/*
 * Gallery payload entry.
 *
 * Executed by a homebrew ELF loader (elfldr) with payload_args in rdi. Opens the display and
 * the pad, then loops: read the pad, page on L1/R1, exit on circle, draw the page, flip.
 *
 * The drawing is gallery.c, shared with the host self-test. This file is the part that only
 * runs on the console: the display, the input, and the loop that ties them together.
 */

#include "oops/display.h"
#include "oops/draw.h"
#include "oops/input.h"
#include "oops/keyboard.h"
#include "oops/mouse.h"
#include "oops/videodec.h"
#include "oops/audiodec.h"
#include "oops/krw.h"
#include "oops/syscall.h"
#include "oops/system.h"

#include "gallery.h"

/* A line to the system log, the one output a payload always has. */
static void klog(const char *msg) {
    char buf[160];
    const char *prefix = "[GALLERY] ";
    int n = 0;
    while (prefix[n] && n < 16) { buf[n] = prefix[n]; n++; }
    int m = 0;
    while (msg[m] && n < (int)sizeof(buf) - 2) { buf[n++] = msg[m++]; }
    buf[n++] = '\n';
    buf[n] = '\0';
    (void)sys_call(SYS_klog, 7, (long)buf, 0, 0, 0, 0);
}

/* A button seen this frame but not last: an edge, so a page turn is one press not a slide. */
static int pressed(uint32_t now, uint32_t was, uint32_t mask) {
    return (now & mask) && !(was & mask);
}

int gallery_start(const payload_args_t *args);

int gallery_start(const payload_args_t *args) {
    if (args) {
        sys_call_init(args);
    }
    klog("gallery payload entry reached");

    oops_display_t *disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, 1280, 720);
    if (!disp || !oops_display_is_ready(disp)) {
        klog("display would not open - see oops_display_get_last_error");
        return -1;
    }
    oops_input_init();

    gallery_state_t state;
    for (size_t i = 0; i < sizeof(state); i++) {
        ((unsigned char *)&state)[i] = 0;
    }
    (void)oops_system_get_info(&state.system);
    state.net_ip = 0;

    /* The capability matrix is fixed for the run: which media-decode and input-device
     * subsystems resolved on this console. Gathered once, after the pad is open so the
     * adaptive-trigger check has a port to ask about. */
    state.caps.videodec = oops_videodec_available();
    state.caps.audiodec = oops_audiodec_available();
    state.caps.audiodec_offload = oops_audiodec_offload_available();
    state.caps.keyboard = oops_keyboard_available();
    state.caps.mouse = oops_mouse_available();
    state.caps.adaptive_triggers = oops_input_adaptive_triggers_available(0);

    uint32_t last_buttons = 0;
    int running = 1;
    while (running) {
        oops_pad_state_t pad;
        if (oops_input_poll(0, &pad) == 0) {
            state.pad = pad;
            if (pressed(pad.buttons, last_buttons, OOPS_BUTTON_R1)) {
                state.page = gallery_wrap_page(state.page + 1);
            }
            if (pressed(pad.buttons, last_buttons, OOPS_BUTTON_L1)) {
                state.page = gallery_wrap_page(state.page - 1);
            }
            if (pressed(pad.buttons, last_buttons, OOPS_BUTTON_CIRCLE)) {
                running = 0;
            }
            last_buttons = pad.buttons;
        }

        oops_surface_t surf = oops_display_get_surface(disp);
        if (surf.pixels) {
            (void)gallery_render(&surf, &state);
        }
        oops_display_flip(disp);
    }

    klog("gallery exiting");
    oops_input_close();
    oops_display_close(disp);
    return 0;
}
