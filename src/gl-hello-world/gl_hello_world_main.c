#include "gl_hello_world.h"
#include "oops/display.h"
#include "oops/draw.h"
#include "oops/input.h"
#include "oops/time.h"
#include "oops/syscall.h"
#include "oops/freestd.h"
#include "oops/krw.h"

#ifndef OOPS_HOST_BUILD
__attribute__((weak)) void exit(int status);
__attribute__((weak)) int sceKernelUsleep(unsigned int microseconds);

static void app_klog(const char *msg) {
    char buf[160];
    const char *pfx = "[GL-MAIN] ";
    int n = 0;
    while (pfx[n] && n < 16) { buf[n] = pfx[n]; n++; }
    int m = 0;
    while (msg[m] && n < (int)sizeof(buf) - 2) { buf[n++] = msg[m++]; }
    buf[n++] = '\n';
    buf[n] = '\0';
    (void)sys_call(SYS_klog, 7, (long)buf, 0, 0, 0, 0);
}

static void u32_to_hex(uint32_t val, char *out) {
    const char *hex = "0123456789abcdef";
    out[0] = '0'; out[1] = 'x';
    for (int i = 7; i >= 0; i--) {
        out[2 + i] = hex[val & 0xf];
        val >>= 4;
    }
    out[10] = '\0';
}
#endif

int gl_hello_world_start(const payload_args_t *args);

__attribute__((visibility("default"))) int gl_hello_world_start(const payload_args_t *args) {
#ifndef OOPS_HOST_BUILD
    if (args) {
        sys_call_init(args);
    }
    app_klog("starting gl-hello-world (Stage 1 AGC Hardware 3D Triangle)...");

    /* 1. Open display */
    oops_display_t *disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, 1920, 1080);
    if (!disp || !oops_display_is_ready(disp)) {
        app_klog("failed to open display");
        return -1;
    }
    app_klog("display opened successfully at 1920x1080 60Hz");

    uint32_t *fb = oops_display_get_framebuffer(disp);
    oops_surface_t surf = oops_display_get_surface(disp);

    /* 2. Initialize controller input */
    oops_input_init();

    /* 3. Initialize hardware RDNA2 3D triangle pipeline */
    gl_triangle_pipeline_t pipe;
    int rc_init = gl_triangle_init(&pipe, fb, 1920, 1080);
    if (rc_init != 0) {
        app_klog("gl_triangle_init returned failure");
    }

    uint64_t frame = 0;
    bool running = true;

    while (running) {
        /* Poll controller */
        oops_pad_state_t pad;
        if (oops_input_poll(0, &pad) == 0 && pad.connected) {
            /* Exit combo: L1 + R1 + OPTIONS or CIRCLE */
            if (((pad.buttons & OOPS_BUTTON_L1) &&
                 (pad.buttons & OOPS_BUTTON_R1) &&
                 (pad.buttons & OOPS_BUTTON_OPTIONS)) ||
                (pad.buttons & OOPS_BUTTON_CIRCLE)) {
                app_klog("exit combo received, terminating cleanly");
                running = false;
                break;
            }
        }

        /* Clear background to dark navy */
        uint32_t bg_color = 0xFF101824u;
        oops_draw_clear(&surf, bg_color);

        /* 4. Dispatch hardware RDNA2 GPU draw with dynamic rotation and color */
        if (pipe.initialized && pipe.use_hardware) {
            gl_triangle_dispatch(&pipe, bg_color, 0); /* 0 = dynamic rainbow color cycle */
        }

        /* 5. Render high-contrast framing border */
        /* Outer white border (20px) */
        oops_draw_rect(&surf, 0, 0, 1920, 20, OOPS_COLOR_WHITE);
        oops_draw_rect(&surf, 0, 1060, 1920, 20, OOPS_COLOR_WHITE);
        oops_draw_rect(&surf, 0, 0, 20, 1080, OOPS_COLOR_WHITE);
        oops_draw_rect(&surf, 1900, 0, 20, 1080, OOPS_COLOR_WHITE);

        /* 6. Render diagnostic HUD */
        /* Dark HUD backing box */
        oops_draw_rect(&surf, 50, 50, 800, 290, 0xD00a0e14u);
        oops_draw_rect(&surf, 48, 48, 804, 2, OOPS_COLOR_CYAN);
        oops_draw_rect(&surf, 48, 340, 804, 2, OOPS_COLOR_CYAN);
        oops_draw_rect(&surf, 48, 48, 2, 294, OOPS_COLOR_CYAN);
        oops_draw_rect(&surf, 850, 48, 2, 294, OOPS_COLOR_CYAN);

        oops_draw_text(&surf, 70, 68, "OOPS-GL HELLO WORLD - STAGE 2 ROTATING TRIANGLE", OOPS_COLOR_WHITE, 2);
        oops_draw_text(&surf, 70, 98, "AMD RDNA2 HARDWARE RASTERIZER (NGG + PS DYNAMIC PIPELINE)", OOPS_COLOR_YELLOW, 1);

        char h1[12], h2[12];

        /* Hardware Queue status */
        u32_to_hex((uint32_t)pipe.metrics.rc_queue, h1);
        oops_draw_text(&surf, 70, 120, "AGC TYPE 0 GRAPHICS QUEUE: READY",
                       pipe.metrics.rc_queue == 0 ? OOPS_COLOR_GREEN : OOPS_COLOR_RED, 1);

        /* Fence status */
        u32_to_hex(pipe.metrics.fence_val, h1);
        oops_draw_text(&surf, 70, 142, "EOP RELEASE_MEM FENCE: ", OOPS_COLOR_WHITE, 1);
        oops_draw_text(&surf, 260, 142, h1, OOPS_COLOR_CYAN, 1);
        oops_draw_text(&surf, 370, 142, pipe.metrics.fence_hit ? "[RETIRED / HIT]" : "[WAITING]",
                       pipe.metrics.fence_hit ? OOPS_COLOR_GREEN : OOPS_COLOR_RED, 1);

        /* VS Canary */
        u32_to_hex(pipe.metrics.canary_vs, h1);
        u32_to_hex(pipe.metrics.canary_vs_done, h2);
        oops_draw_text(&surf, 70, 164, "VERTEX SHADER CANARY : ", OOPS_COLOR_WHITE, 1);
        oops_draw_text(&surf, 260, 164, h1, OOPS_COLOR_CYAN, 1);
        oops_draw_text(&surf, 370, 164, "-> DONE:", OOPS_COLOR_WHITE, 1);
        oops_draw_text(&surf, 440, 164, h2, OOPS_COLOR_GREEN, 1);

        /* PS Canary */
        u32_to_hex(pipe.metrics.canary_ps, h1);
        u32_to_hex(pipe.metrics.canary_ps_done, h2);
        oops_draw_text(&surf, 70, 186, "PIXEL SHADER CANARY  : ", OOPS_COLOR_WHITE, 1);
        oops_draw_text(&surf, 260, 186, h1, OOPS_COLOR_CYAN, 1);
        oops_draw_text(&surf, 370, 186, "-> DONE:", OOPS_COLOR_WHITE, 1);
        oops_draw_text(&surf, 440, 186, h2, OOPS_COLOR_GREEN, 1);

        /* Execution Wave Info */
        u32_to_hex(pipe.metrics.canary_exec, h1);
        oops_draw_text(&surf, 70, 208, "STARTUP EXEC_LO MASK : ", OOPS_COLOR_WHITE, 1);
        oops_draw_text(&surf, 260, 208, h1, OOPS_COLOR_YELLOW, 1);

        /* Dynamic Rotation Angle */
        uint32_t deg = (uint32_t)(pipe.metrics.rotation_angle * 57.2957795f) % 360u;
        char dstr[16];
        int dlen = 0;
        uint32_t dtmp = deg;
        if (dtmp == 0) { dstr[dlen++] = '0'; }
        else {
            char r[8]; int rlen = 0;
            while (dtmp > 0) { r[rlen++] = (char)('0' + (dtmp % 10)); dtmp /= 10; }
            while (rlen > 0) { dstr[dlen++] = r[--rlen]; }
        }
        dstr[dlen++] = ' '; dstr[dlen++] = 'd'; dstr[dlen++] = 'e'; dstr[dlen++] = 'g';
        dstr[dlen] = '\0';
        oops_draw_text(&surf, 70, 230, "DYNAMIC ROTATION DEG : ", OOPS_COLOR_WHITE, 1);
        oops_draw_text(&surf, 260, 230, dstr, OOPS_COLOR_CYAN, 1);

        /* Dynamic Primitive Color */
        u32_to_hex(pipe.metrics.current_color, h1);
        oops_draw_text(&surf, 70, 252, "DYNAMIC VERTEX COLOR : ", OOPS_COLOR_WHITE, 1);
        oops_draw_text(&surf, 260, 252, h1, pipe.metrics.current_color, 1);

        /* Frame Counter */
        char fstr[32];
        int flen = 0;
        uint64_t ftmp = frame;
        if (ftmp == 0) { fstr[flen++] = '0'; }
        else {
            char r[20]; int rlen = 0;
            while (ftmp > 0) { r[rlen++] = (char)('0' + (ftmp % 10)); ftmp /= 10; }
            while (rlen > 0) { fstr[flen++] = r[--rlen]; }
        }
        fstr[flen] = '\0';
        oops_draw_text(&surf, 70, 274, "PRESENTED FRAME COUNT: ", OOPS_COLOR_WHITE, 1);
        oops_draw_text(&surf, 260, 274, fstr, OOPS_COLOR_WHITE, 1);

        /* 7. Present frame at 60 FPS (oops_display_flip synchronizes to VBLANK) */
        oops_display_flip(disp);
        app_klog("presented-frame");

        frame++;
    }

    gl_triangle_fini(&pipe);
    oops_display_close(disp);
    app_klog("gl-hello-world terminated gracefully");
#endif
    return 0;
}
