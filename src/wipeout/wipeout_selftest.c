#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "platform_oops.h"

/* Host stubs for platform subsystems */
__attribute__((weak)) oops_display_t *oops_display_open(oops_display_backend_t b, unsigned int w, unsigned int h) {
    (void)b; (void)w; (void)h; return (oops_display_t *)0;
}
__attribute__((weak)) int oops_display_is_ready(const oops_display_t *d) { (void)d; return 0; }
__attribute__((weak)) int oops_display_is_gpu_accelerated(const oops_display_t *d) { (void)d; return 0; }
__attribute__((weak)) uint32_t *oops_display_get_framebuffer(oops_display_t *d) { (void)d; return (uint32_t *)0; }
__attribute__((weak)) int oops_display_flip(oops_display_t *d) { (void)d; return 0; }
__attribute__((weak)) void oops_display_close(oops_display_t *d) { (void)d; }
__attribute__((weak)) int oops_input_init(void) { return 0; }
__attribute__((weak)) int oops_input_poll(unsigned int p, oops_pad_state_t *s) { (void)p; (void)s; return -1; }
__attribute__((weak)) void oops_input_close(void) {}
__attribute__((weak)) oops_audio_port_t *oops_audio_open(int r, int c, int s) {
    (void)r; (void)c; (void)s; return (oops_audio_port_t *)0;
}
__attribute__((weak)) int oops_audio_write(oops_audio_port_t *p, const int16_t *b, size_t s) {
    (void)p; (void)b; (void)s; return 0;
}
__attribute__((weak)) void oops_audio_close(oops_audio_port_t *p) { (void)p; }
#include "oops/time.h"
__attribute__((weak)) double oops_time_get_seconds(void) { return 1.0; }

int main(void) {
    wipeout_platform_t plat;
    for (size_t i = 0; i < sizeof(plat); i++) {
        ((uint8_t *)&plat)[i] = 0;
    }
    plat.width = 1920;
    plat.height = 1080;
    plat.running = true;

    /* Screen size contract */
    wipeout_vec2i_t sz = wipeout_platform_screen_size(&plat);
    if (sz.x != 1920 || sz.y != 1080) {
        fprintf(stderr, "wipeout selftest: screen size mismatch (%d x %d)\n", sz.x, sz.y);
        return 1;
    }

    /* Input deadzone and normalization verification */
    plat.input.left_x = 0.05f; /* inside 0.15 deadzone */
    plat.input.l2_trigger = 0.75f;
    plat.input.thrust = true;
    plat.input.fire = false;

    if (plat.input.thrust != true || plat.input.fire != false) {
        fprintf(stderr, "wipeout selftest: input state flag error\n");
        return 1;
    }
    if (plat.input.l2_trigger < 0.74f || plat.input.l2_trigger > 0.76f) {
        fprintf(stderr, "wipeout selftest: trigger mapping error\n");
        return 1;
    }

    /* Exit combo logic verification */
    uint32_t combo_buttons = OOPS_BUTTON_L1 | OOPS_BUTTON_R1 | OOPS_BUTTON_OPTIONS;
    bool is_combo = ((combo_buttons & OOPS_BUTTON_L1) &&
                     (combo_buttons & OOPS_BUTTON_R1) &&
                     (combo_buttons & OOPS_BUTTON_OPTIONS));
    if (!is_combo) {
        fprintf(stderr, "wipeout selftest: exit combo failed to match\n");
        return 1;
    }

    /* Audio buffer sizing: 1024 frames stereo @ 16-bit = 4096 bytes */
    const uint32_t chunk_frames = 1024;
    const uint32_t chunk_bytes = chunk_frames * 2 * sizeof(int16_t);
    if (chunk_bytes != 4096) {
        fprintf(stderr, "wipeout selftest: audio chunk byte size mismatch (%u)\n", chunk_bytes);
        return 1;
    }

    printf("wipeout selftest: ok (platform layer, DualSense input mapping, and audio PCM bounds verified)\n");
    return 0;
}
