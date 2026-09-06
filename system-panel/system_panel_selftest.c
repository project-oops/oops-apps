/*
 * Host self-test for the system panel.
 *
 * The point of an app's host test is the same as the SDK's: exercise the drawing on an
 * ordinary machine so a bug in the app is told apart from a bug on the console. It allocates
 * a plain framebuffer, renders the panel into it, and checks the render actually happened -
 * pixels changed from the cleared background, and the expected number of rows came back.
 *
 * It does not check the *values*: on a host the platform symbols behind oops_system_get_info
 * are absent, so the numbers are the SDK's defaults, not a real machine's. Asserting on them
 * would be asserting on the host, which measures nothing.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oops/display.h"
#include "oops/draw.h"
#include "oops/system.h"

#include "system_panel.h"

/*
 * The SDK's draw.c defines oops_display_get_surface, which reaches into the display backend.
 * This test builds its own surface and never opens a display, so these three exist only to
 * satisfy the link - the same host-stub shape the SDK and Porthole use to keep a host build
 * free of the platform.
 */
uint32_t *oops_display_get_framebuffer(oops_display_t *disp) {
    (void)disp;
    return 0;
}
unsigned int oops_display_get_width(const oops_display_t *disp) {
    (void)disp;
    return 0;
}
unsigned int oops_display_get_height(const oops_display_t *disp) {
    (void)disp;
    return 0;
}

#define W 1280
#define H 720

int main(void) {
    uint32_t *pixels = calloc((size_t)W * H, sizeof(uint32_t));
    if (!pixels) {
        fprintf(stderr, "system-panel selftest: out of memory\n");
        return 1;
    }

    oops_surface_t surf = {
        .pixels = pixels,
        .width = W,
        .height = H,
        .pitch = W,
    };

    oops_system_info_t info;
    (void)oops_system_get_info(&info);

    int rows = system_panel_render(&surf, &info);

    /* The render clears to the panel background, so a fully-zero buffer means nothing drew. */
    int any_accent = 0;
    for (size_t i = 0; i < (size_t)W * H; i++) {
        if (pixels[i] == 0xFF00FFFFu) { /* the accent colour, from the title and the rule */
            any_accent = 1;
            break;
        }
    }

    int ok = 1;
    if (rows != 4) {
        fprintf(stderr, "system-panel selftest: expected 4 rows, got %d\n", rows);
        ok = 0;
    }
    if (!any_accent) {
        fprintf(stderr, "system-panel selftest: nothing was drawn (no accent pixels)\n");
        ok = 0;
    }
    /* A guard against a render that draws off the surface: the last pixel must be background,
     * never left uninitialised or stamped by an out-of-bounds write. */
    if (pixels[(size_t)W * H - 1] != 0xFF0D1116u) {
        fprintf(stderr, "system-panel selftest: last pixel is not the background\n");
        ok = 0;
    }

    free(pixels);

    if (ok) {
        printf("system-panel selftest: ok (%d rows rendered; draw path verified on host)\n",
               rows);
        return 0;
    }
    return 1;
}
