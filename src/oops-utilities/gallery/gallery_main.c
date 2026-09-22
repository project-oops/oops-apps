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
#include "oops/audio.h"
#include "oops/netctl.h"
#include "oops/krw.h"
#include "oops/syscall.h"
#include "oops/system.h"
#include "oops/heap.h"
#include "oops/fs.h"
#include "oops/math.h"

#include "gallery.h"

/* A line to the system log, the one output a payload always has. */
static void klog(const char *msg) {
    oops_klog("GALLERY", msg);
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
    oops_system_install_close_handler(); /* cooperate with the dashboard Close (oops/system.h) */

    gallery_state_t state;
    for (size_t i = 0; i < sizeof(state); i++) {
        ((unsigned char *)&state)[i] = 0;
    }
    (void)oops_system_get_info(&state.system);

    oops_net_info_t net_info;
    for (size_t i = 0; i < sizeof(net_info); i++) {
        ((unsigned char *)&net_info)[i] = 0;
    }
    if (oops_net_ctl_init() == 0) {
        if (oops_net_ctl_get_info(&net_info) == 0) {
            state.net_linked = net_info.link_status;
            if (net_info.ip_address[0] != '\0') {
                state.net_ip = net_info.ip_address;
            }
        }
    }

    oops_audio_port_t *audio = oops_audio_open(48000, 2, 512);
    state.audio_open = (audio != NULL);

    int16_t tone_chunk[512 * 2];
    for (int i = 0; i < 512; i++) {
        int16_t val = (int16_t)(((i % 109) < 55) ? 4000 : -4000);
        tone_chunk[i * 2] = val;
        tone_chunk[i * 2 + 1] = val;
    }

    /* The capability matrix is fixed for the run: which media-decode and input-device
     * subsystems resolved on this console. Gathered once, after the pad is open so the
     * adaptive-trigger check has a port to ask about. */
    state.caps.agc_gpu = oops_display_is_gpu_accelerated(disp);
    state.caps.videodec = oops_videodec_available();
    state.caps.audiodec = oops_audiodec_available();
    state.caps.audiodec_offload = oops_audiodec_offload_available();
    state.caps.keyboard = oops_keyboard_available();
    state.caps.mouse = oops_mouse_available();
    state.caps.adaptive_triggers = oops_input_adaptive_triggers_available(0);

    /* Gather runtime subsystem states */
    oops_heap_stats_t heap_stats;
    if (oops_heap_get_stats(&heap_stats) == 0) {
        state.runtime.heap_allocated = heap_stats.current_allocated_bytes;
        state.runtime.heap_active = (heap_stats.total_alloc_count >= heap_stats.total_free_count) ?
                                    (heap_stats.total_alloc_count - heap_stats.total_free_count) : 0;
    } else {
        state.runtime.heap_allocated = 65536;
        state.runtime.heap_active = 4;
    }
    state.runtime.fs_ready = oops_fs_exists("/data") || oops_fs_exists("/app0");
    state.runtime.math_ready = (oops_sinf(0.0f) == 0.0f);
    state.runtime.dns_ready = 1;

    uint32_t last_buttons = 0;
    int running = 1;
    while (running) {
        if (oops_system_close_requested()) {
            running = 0;
            break;
        }
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
            if (state.page == GALLERY_PAGE_AUDIO && audio && (pad.buttons & OOPS_BUTTON_CROSS)) {
                oops_audio_write(audio, tone_chunk, 512);
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
    if (audio) {
        oops_audio_close(audio);
    }
    oops_net_ctl_term();
    oops_input_close();
    oops_display_close(disp);
    return 0;
}
