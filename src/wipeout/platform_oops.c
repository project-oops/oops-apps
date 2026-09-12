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

#ifndef OOPS_HOST_BUILD
#include "agc/display.h"

__attribute__((weak)) void exit(int status);
__attribute__((weak)) int sceKernelUsleep(unsigned int microseconds);

static void wipeout_agc_logger(const char *tag, const char *msg, uint64_t val) {
    char buf[160];
    char hex[17];
    uint64_t v = val;
    for (int i = 15; i >= 0; i--) {
        uint8_t d = (uint8_t)(v & 0xf);
        hex[i] = (char)(d < 10 ? ('0' + d) : ('a' + d - 10));
        v >>= 4;
    }
    hex[16] = '\0';
    int hstart = 0;
    while (hstart < 15 && hex[hstart] == '0') {
        hstart++;
    }

    int n = 0;
    const char *pfx = "[AGC] ";
    while (pfx[n] && n < 8) { buf[n] = pfx[n]; n++; }
    int m = 0;
    while (tag && tag[m] && n < 32) { buf[n++] = tag[m++]; }
    if (n < 34) { buf[n++] = ':'; buf[n++] = ' '; }
    m = 0;
    while (msg && msg[m] && n < 80) { buf[n++] = msg[m++]; }
    if (n < 84) { buf[n++] = ' '; buf[n++] = '='; buf[n++] = ' '; buf[n++] = '0'; buf[n++] = 'x'; }
    m = hstart;
    while (hex[m] && n < (int)sizeof(buf) - 2) { buf[n++] = hex[m++]; }
    buf[n] = '\0';
    klog(buf);
}
#endif

int wipeout_platform_init(wipeout_platform_t *plat, uint32_t width, uint32_t height) {
    if (!plat) return -1;
    for (size_t i = 0; i < sizeof(wipeout_platform_t); i++) {
        ((uint8_t *)plat)[i] = 0;
    }

    plat->width = width ? width : 1920;
    plat->height = height ? height : 1080;

#ifndef OOPS_HOST_BUILD
    agc_display_set_logger(wipeout_agc_logger);
#endif

    /* Initialize display via AGC backend on Prospero */
    plat->disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, plat->width, plat->height);
    if (!plat->disp || !oops_display_is_ready(plat->disp)) {
        klog("display would not open");
#ifndef OOPS_HOST_BUILD
        int err = plat->disp ? oops_display_get_last_error(plat->disp) : -1;
        wipeout_agc_logger("disp", "oops_display_open failed, last_error", (uint64_t)(uint32_t)err);
#endif
        return -1;
    }
    plat->framebuffer = oops_display_get_framebuffer(plat->disp);

    /* Initialize controller input */
    oops_input_init();

    /* Initialize audio PCM port (44.1kHz stereo, 1024-sample chunks) */
    plat->audio = oops_audio_open(44100, 2, 1024);
    plat->running = true;

    if (oops_display_is_gpu_accelerated(plat->disp)) {
        klog("platform initialized with hardware RDNA2 compute presentation");
    } else {
        klog("platform initialized with software presentation");
    }
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
    sz.x = plat ? (int32_t)plat->width : 1920;
    sz.y = plat ? (int32_t)plat->height : 1080;
    return sz;
}

double wipeout_platform_now(void) {
    return oops_time_get_seconds();
}

#ifndef OOPS_HOST_BUILD
int wipeout_start(const payload_args_t *args);

int wipeout_start(const payload_args_t *args) {
    sys_call_init(args);
    klog("wipeout entry reached");

    wipeout_platform_t plat;
    if (wipeout_platform_init(&plat, 1920, 1080) != 0) {
        klog("wipeout platform init failed; terminating process");
        if (exit) {
            exit(1);
        }
        (void)sys_call(SYS_exit, 1, 0, 0, 0, 0, 0);
        for (;;) {
            if (sceKernelUsleep) {
                sceKernelUsleep(1000000);
            } else {
                __asm__ volatile("pause");
            }
        }
    }

    /* Main platform loop with high-contrast diagnostic test pattern */
    uint32_t frame_num = 0;
    while (plat.running) {
        wipeout_platform_update_input(&plat);

        if (plat.framebuffer) {
            uint32_t *p = plat.framebuffer;
            uint32_t w = plat.width;
            uint32_t h = plat.height;

            /* 7 high-contrast vertical color bars:
             * White, Yellow, Cyan, Green, Magenta, Red, Blue */
            static const uint32_t bar_colors[7] = {
                0xffffffffu, /* White */
                0xffffff00u, /* Yellow */
                0xff00ffffu, /* Cyan */
                0xff00ff00u, /* Green */
                0xffff00ffu, /* Magenta */
                0xffff0000u, /* Red */
                0xff0000ffu  /* Blue */
            };

            for (uint32_t y = 0; y < h; y++) {
                for (uint32_t x = 0; x < w; x++) {
                    /* 20-pixel bright white border around screen */
                    if (x < 20 || x >= w - 20 || y < 20 || y >= h - 20) {
                        p[y * w + x] = 0xffffffffu;
                    } else if (y >= h / 4 && y < (3 * h) / 4) {
                        /* Middle band: 7 vertical color bars */
                        uint32_t bar_idx = (x - 20) / ((w - 40) / 7);
                        if (bar_idx >= 7) bar_idx = 6;
                        p[y * w + x] = bar_colors[bar_idx];
                    } else if (y < h / 4) {
                        /* Top band: Solid bright Magenta */
                        p[y * w + x] = 0xffff00ffu;
                    } else {
                        /* Bottom band: Solid bright Cyan */
                        p[y * w + x] = 0xff00ffffu;
                    }
                }
            }

            /* Animated ticker box: 80x80 dark square moving across the color bars */
            uint32_t span = (w > 160) ? (w - 160) : 100;
            uint32_t box_x = 40 + ((frame_num * 8) % span);
            uint32_t box_y = (h / 2) - 40;
            for (uint32_t by = 0; by < 80; by++) {
                for (uint32_t bx = 0; bx < 80; bx++) {
                    p[(box_y + by) * w + (box_x + bx)] = 0xff000000u;
                }
            }
            frame_num++;
        }

        wipeout_platform_swap_buffers(&plat);
    }

    wipeout_platform_exit(&plat);
    klog("wipeout execution complete; terminating process");
    if (exit) {
        exit(0);
    }
    (void)sys_call(SYS_exit, 0, 0, 0, 0, 0, 0);
    for (;;) {
        if (sceKernelUsleep) {
            sceKernelUsleep(1000000);
        } else {
            __asm__ volatile("pause");
        }
    }
}
#endif
