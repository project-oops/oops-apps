#include "image-viewer.h"

#include "oops/draw.h"

int image_viewer_render(oops_surface_t *surf, const image_rgba_t *img, oops_color_t background) {
    if (!surf || !surf->pixels || !img || !img->pixels || img->w == 0 || img->h == 0) {
        return -1;
    }

    oops_draw_clear(surf, background);

    unsigned int dw = surf->width;
    unsigned int dh = surf->height;

    /* The largest integer-free scale that fits both axes, as a fixed-point 16.16 ratio so no
     * float is needed - this compiles for the target as well as the host. Compare cross-
     * multiplied rather than dividing, to pick the tighter axis exactly. */
    unsigned long long fit_w = (unsigned long long)dw * img->h;
    unsigned long long fit_h = (unsigned long long)dh * img->w;
    unsigned int out_w;
    unsigned int out_h;
    if (fit_w < fit_h) {
        /* width-bound */
        out_w = dw;
        out_h = (unsigned int)(((unsigned long long)dw * img->h) / img->w);
    } else {
        /* height-bound */
        out_h = dh;
        out_w = (unsigned int)(((unsigned long long)dh * img->w) / img->h);
    }
    if (out_w == 0) out_w = 1;
    if (out_h == 0) out_h = 1;

    unsigned int ox = (dw - out_w) / 2;
    unsigned int oy = (dh - out_h) / 2;

    for (unsigned int y = 0; y < out_h; y++) {
        unsigned int sy = (y * img->h) / out_h;
        if (sy >= img->h) sy = img->h - 1;
        uint32_t *drow = surf->pixels + (size_t)(oy + y) * surf->pitch + ox;
        const uint32_t *srow = img->pixels + (size_t)sy * img->w;
        for (unsigned int x = 0; x < out_w; x++) {
            unsigned int sx = (x * img->w) / out_w;
            if (sx >= img->w) sx = img->w - 1;
            drow[x] = srow[sx];
        }
    }

    return 0;
}
