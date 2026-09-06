#ifndef OOPS_APPS_IMAGE_VIEWER_H
#define OOPS_APPS_IMAGE_VIEWER_H

#include "oops/draw.h"

/*
 * image-viewer: show an image, fit to the screen.
 *
 * The fit-and-blit is pure - a source buffer in, a destination surface out - so it is testable
 * on a host with a made-up image. Reading and decoding a file from disk is the console-only
 * half; this is the part that puts pixels where they go.
 */

/* A decoded image: tightly-packed 0xAARRGGBB pixels, `w` by `h`. */
typedef struct image_rgba {
    const uint32_t *pixels;
    unsigned int w;
    unsigned int h;
} image_rgba_t;

/*
 * Draw `img` into `surf`, scaled to fit while preserving aspect and centred, on a filled
 * background. Nearest-neighbour: correct and cheap, and a real viewer's first pass. Returns 0
 * on success, non-zero if the inputs make no sense.
 */
int image_viewer_render(oops_surface_t *surf, const image_rgba_t *img, oops_color_t background);

#endif /* OOPS_APPS_IMAGE_VIEWER_H */
