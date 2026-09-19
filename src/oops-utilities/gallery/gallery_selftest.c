/*
 * Host self-test for the gallery.
 *
 * Renders every page into a plain buffer and checks each drew and none wrote past the surface.
 * It does not check what the pages say - on a host the platform behind the SDK is absent, so
 * the values are defaults - only that the drawing is sound, which is a fact about the app
 * rather than about the machine it is not running on.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oops/display.h"
#include "oops/draw.h"

#include "gallery.h"

/* draw.c defines oops_display_get_surface, which reaches the display backend this host test
 * never opens; these satisfy the link, the SDK's own host-stub shape. */
uint32_t *oops_display_get_framebuffer(oops_display_t *disp) { (void)disp; return 0; }
unsigned int oops_display_get_width(const oops_display_t *disp) { (void)disp; return 0; }
unsigned int oops_display_get_height(const oops_display_t *disp) { (void)disp; return 0; }

#define W 1280
#define H 720
#define GUARD 0xDEADBEEFu

int main(void) {
    /* One guard pixel past the surface: render must never touch it. */
    uint32_t *pixels = malloc(((size_t)W * H + 1) * sizeof(uint32_t));
    if (!pixels) {
        fprintf(stderr, "gallery selftest: out of memory\n");
        return 1;
    }
    pixels[(size_t)W * H] = GUARD;

    oops_surface_t surf = { .pixels = pixels, .width = W, .height = H, .pitch = W };

    int ok = 1;
    for (int page = 0; page < gallery_page_count(); page++) {
        for (size_t i = 0; i < (size_t)W * H; i++) {
            pixels[i] = 0;
        }
        gallery_state_t state;
        memset(&state, 0, sizeof(state));
        state.page = page;
        state.pad.connected = 1;
        state.pad.buttons = OOPS_BUTTON_CROSS;
        state.net_ip = "192.168.1.211";
        state.net_linked = 1;
        state.audio_open = 1;
        /* Mixed capabilities so the caps page exercises both the available (green) and absent
         * (red) branches, not just one. */
        state.caps.agc_gpu = 1;
        state.caps.videodec = 1;
        state.caps.audiodec = 1;
        state.caps.audiodec_offload = 1;
        state.caps.keyboard = 0;
        state.caps.mouse = 0;
        state.caps.adaptive_triggers = 0;
        (void)oops_system_get_info(&state.system);

        int drawn = gallery_render(&surf, &state);
        if (drawn != page) {
            fprintf(stderr, "gallery selftest: page %d rendered as %d\n", page, drawn);
            ok = 0;
        }

        int any = 0;
        for (size_t i = 0; i < (size_t)W * H; i++) {
            if (pixels[i] != 0) { any = 1; break; }
        }
        if (!any) {
            fprintf(stderr, "gallery selftest: page %d drew nothing\n", page);
            ok = 0;
        }
        if (pixels[(size_t)W * H] != GUARD) {
            fprintf(stderr, "gallery selftest: page %d wrote past the surface\n", page);
            ok = 0;
        }
    }

    /* Navigation wraps both ways rather than falling off the ends. */
    if (gallery_wrap_page(-1) != gallery_page_count() - 1 ||
        gallery_wrap_page(gallery_page_count()) != 0) {
        fprintf(stderr, "gallery selftest: page wrap is wrong\n");
        ok = 0;
    }

    free(pixels);
    if (ok) {
        printf("gallery selftest: ok (%d pages rendered; draw path and bounds verified)\n",
               gallery_page_count());
        return 0;
    }
    return 1;
}
