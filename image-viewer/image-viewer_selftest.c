/*
 * Host self-test for image-viewer.
 *
 * Renders a small solid image into a wide surface and checks the fit: the image lands centred,
 * a pixel in the middle is the image's colour, and the corners are the background - i.e. it
 * scaled to fit rather than stretching or overflowing. Decoding a file is the console-only
 * half and is not exercised here.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "oops/display.h"
#include "oops/draw.h"

#include "image-viewer.h"

uint32_t *oops_display_get_framebuffer(oops_display_t *disp) { (void)disp; return 0; }
unsigned int oops_display_get_width(const oops_display_t *disp) { (void)disp; return 0; }
unsigned int oops_display_get_height(const oops_display_t *disp) { (void)disp; return 0; }

#define W 1280
#define H 720
#define BG 0xFF0D1116u
#define FILL 0xFF44AA88u

int main(void) {
    uint32_t *pixels = malloc((size_t)W * H * sizeof(uint32_t));
    if (!pixels) {
        return 1;
    }
    oops_surface_t surf = { .pixels = pixels, .width = W, .height = H, .pitch = W };

    /* A small square image, one solid colour. A square in a 16:9 surface must be height-bound,
     * so it should end up H tall, centred horizontally, with background either side. */
    enum { N = 8 };
    uint32_t src[N * N];
    for (int i = 0; i < N * N; i++) {
        src[i] = FILL;
    }
    image_rgba_t img = { src, N, N };

    int ok = 1;
    if (image_viewer_render(&surf, &img, BG) != 0) {
        fprintf(stderr, "image-viewer selftest: render refused a good image\n");
        ok = 0;
    }

    /* Centre pixel is the image. */
    if (pixels[(size_t)(H / 2) * W + (W / 2)] != FILL) {
        fprintf(stderr, "image-viewer selftest: centre is not the image\n");
        ok = 0;
    }
    /* Corners are background - the square did not stretch to the wide edges. */
    if (pixels[0] != BG || pixels[W - 1] != BG) {
        fprintf(stderr, "image-viewer selftest: corners are not background (stretched?)\n");
        ok = 0;
    }
    /* The fit is height-bound: the top and bottom centre columns are the image. */
    if (pixels[(size_t)0 * W + (W / 2)] != FILL || pixels[(size_t)(H - 1) * W + (W / 2)] != FILL) {
        fprintf(stderr, "image-viewer selftest: image did not fill the height\n");
        ok = 0;
    }

    /* A nonsense image is refused, not drawn. */
    image_rgba_t bad = { src, 0, 0 };
    if (image_viewer_render(&surf, &bad, BG) == 0) {
        fprintf(stderr, "image-viewer selftest: rendered a zero-size image\n");
        ok = 0;
    }

    free(pixels);
    if (ok) {
        printf("image-viewer selftest: ok (fit-to-screen centres and preserves aspect)\n");
        return 0;
    }
    return 1;
}
