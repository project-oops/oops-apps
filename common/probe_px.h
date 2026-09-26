/*
 * probe_px.h - the pixel helpers gl1-probe and gl2-probe share.
 *
 * Both probes draw into a PROBE_W x PROBE_H viewport at GL's origin, the bottom-left of
 * a full-size display (the display refuses a buffer of the probe's own size), and read
 * pixels back as 0xAARRGGBB words.
 */
#ifndef OOPS_APPS_PROBE_PX_H
#define OOPS_APPS_PROBE_PX_H

#include <GL/gl.h>
#include <stddef.h>
#include <stdint.h>

#define PROBE_W 128
#define PROBE_H 96
#define PROBE_DISPLAY_W 1920
#define PROBE_DISPLAY_H 1080

/* The clear colour every check starts from: a value no check draws, so "unchanged" is
 * distinguishable from "drawn black" and from "drawn white". */
#define PROBE_BG 0xff202020u

/* One pixel of a PROBE_W-wide snapshot. */
#define SCAN_PX(s, x, y) ((s)[(y) * PROBE_W + (x)])

/* The drawn frame. glFinish submits the pending stream and waits on its fence, and is a
 * no-op with nothing pending. On the target the render target is write-combined, so the
 * command processor's cached copy is the one read. */
static inline const uint32_t *probe_frame(const uint32_t *fb) {
    glFinish();
#ifndef OOPS_HOST_BUILD
    const GLuint *rb = glGetFrameReadback();
    if (rb)
        return (const uint32_t *)rb;
#endif
    return fb;
}

/* Copies the probe's rectangle out of a frame whose rows are stride words apart,
 * starting at row0. Leaves out untouched when f is NULL. */
static inline void probe_snapshot(const uint32_t *f, unsigned int row0,
                                  unsigned int stride, uint32_t *out) {
    if (!f)
        return;
    for (int y = 0; y < PROBE_H; y++) {
        const uint32_t *row = f + (size_t)(row0 + (unsigned int)y) * (size_t)stride;
        for (int x = 0; x < PROBE_W; x++)
            out[y * PROBE_W + x] = row[x];
    }
}

static inline int chan_r(uint32_t c) {
    return (int)((c >> 16) & 0xffu);
}
static inline int chan_g(uint32_t c) {
    return (int)((c >> 8) & 0xffu);
}
static inline int chan_b(uint32_t c) {
    return (int)(c & 0xffu);
}

/* Each channel within tol of the expected value: the rasteriser interpolates, and the
 * hardware path may round differently from the software one. */
static inline int near_rgb(uint32_t c, int r, int g, int b, int tol) {
    int dr = chan_r(c) - r, dg = chan_g(c) - g, db = chan_b(c) - b;
    if (dr < 0)
        dr = -dr;
    if (dg < 0)
        dg = -dg;
    if (db < 0)
        db = -db;
    return dr <= tol && dg <= tol && db <= tol;
}

#endif
