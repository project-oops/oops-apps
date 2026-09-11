#include "platform_oops.h"
#include "oops/time.h"
#include "oops/syscall.h"
#include "oops/krw.h"
#include "oops/freestd.h"

static void klog(const char *msg) {
#ifndef OOPS_HOST_BUILD
    char buf[160];
    const char *prefix = "[WIPEOUT] ";
    int n = 0;
    while (prefix[n] && n < 16) { buf[n] = prefix[n]; n++; }
    int m = 0;
    while (msg[m] && n < (int)sizeof(buf) - 2) { buf[n++] = msg[m++]; }
    buf[n++] = '\n';
    buf[n] = '\0';
    (void)sys_call(SYS_klog, 7, (long)buf, 0, 0, 0, 0);
#else
    (void)msg;
#endif
}

int wipeout_platform_init(wipeout_platform_t *plat, uint32_t width, uint32_t height) {
    if (!plat) return -1;
    for (size_t i = 0; i < sizeof(wipeout_platform_t); i++) {
        ((uint8_t *)plat)[i] = 0;
    }

    plat->width = width ? width : 1280;
    plat->height = height ? height : 720;

    /* Initialize display via AGC backend on Prospero */
    plat->disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, plat->width, plat->height);
    if (!plat->disp || !oops_display_is_ready(plat->disp)) {
        klog("display would not open");
        return -1;
    }
    plat->framebuffer = oops_display_get_framebuffer(plat->disp);

    /* Initialize controller input */
    oops_input_init();

    /* Initialize audio PCM port (44.1kHz stereo, 1024-sample chunks) */
    plat->audio = oops_audio_open(44100, 2, 1024);
    plat->running = true;

    klog("platform initialized with AGC presentation");
    return 0;
}

void wipeout_platform_update_input(wipeout_platform_t *plat) {
    if (!plat) return;
    oops_pad_state_t pad;
    if (oops_input_poll(0, &pad) == 0 && pad.connected) {
        /* DualSense stick deadzone normalization */
        float sx = (float)pad.left_stick_x / 128.0f;
        float sy = (float)pad.left_stick_y / 128.0f;
        if (sx > -0.15f && sx < 0.15f) sx = 0.0f;
        if (sy > -0.15f && sy < 0.15f) sy = 0.0f;
        plat->input.left_x = sx;
        plat->input.left_y = sy;

        plat->input.l2_trigger = (float)pad.l2_trigger / 255.0f;
        plat->input.r2_trigger = (float)pad.r2_trigger / 255.0f;

        plat->input.thrust = (pad.buttons & OOPS_BUTTON_CROSS) != 0;
        plat->input.fire = (pad.buttons & OOPS_BUTTON_SQUARE) != 0;
        plat->input.look_back = (pad.buttons & OOPS_BUTTON_CIRCLE) != 0;
        plat->input.change_view = (pad.buttons & OOPS_BUTTON_TRIANGLE) != 0;
        plat->input.pause = (pad.buttons & OOPS_BUTTON_OPTIONS) != 0;

        /* Exit combo: L1 + R1 + Options */
        if ((pad.buttons & OOPS_BUTTON_L1) &&
            (pad.buttons & OOPS_BUTTON_R1) &&
            (pad.buttons & OOPS_BUTTON_OPTIONS)) {
            plat->input.exit_combo = true;
            plat->running = false;
        }
    }
}

void wipeout_platform_swap_buffers(wipeout_platform_t *plat) {
    if (!plat || !plat->disp) return;
    /* Hardware GPU compute tiles the frame into the scanout surface */
    oops_display_flip(plat->disp);
}

void wipeout_platform_audio_write(wipeout_platform_t *plat, const int16_t *samples, uint32_t count) {
    if (!plat || !plat->audio || !samples || count == 0) return;
    oops_audio_write(plat->audio, samples, (size_t)count);
}

void wipeout_platform_exit(wipeout_platform_t *plat) {
    if (!plat) return;
    klog("platform exiting");
    if (plat->audio) {
        oops_audio_close(plat->audio);
        plat->audio = (void *)0;
    }
    oops_input_close();
    if (plat->disp) {
        oops_display_close(plat->disp);
        plat->disp = (void *)0;
    }
    plat->running = false;
}

wipeout_vec2i_t wipeout_platform_screen_size(const wipeout_platform_t *plat) {
    wipeout_vec2i_t sz;
    sz.x = plat ? (int32_t)plat->width : 1280;
    sz.y = plat ? (int32_t)plat->height : 720;
    return sz;
}

double wipeout_platform_now(void) {
    return oops_time_get_seconds();
}

#ifndef OOPS_HOST_BUILD
int wipeout_start(const payload_args_t *args);

int wipeout_start(const payload_args_t *args) {
    if (args) {
        sys_call_init(args);
    }
    klog("wipeout payload entry reached");

    wipeout_platform_t plat;
    if (wipeout_platform_init(&plat, 1280, 720) != 0) {
        return -1;
    }

    /* Main platform loop */
    while (plat.running) {
        wipeout_platform_update_input(&plat);

        /* Clear buffer with retro dark blue track color */
        if (plat.framebuffer) {
            uint32_t *p = plat.framebuffer;
            for (size_t i = 0; i < (size_t)plat.width * plat.height; i++) {
                p[i] = 0xff050b18u;
            }
        }

        wipeout_platform_swap_buffers(&plat);
    }

    wipeout_platform_exit(&plat);
    return 0;
}
#endif
