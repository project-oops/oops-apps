/*
 * gl1-probe: does oops-gl actually do OpenGL 1.x?
 *
 * gl-cube answers a narrower question. It is a pinned *oracle* - one measured frame, asserted
 * register by register against a real console - and it is very good at noticing that the path
 * which drew has stopped drawing. What it does not do is exercise breadth: twelve triangles,
 * one texture, lighting, depth, culling, and nothing else. Around a hundred entry points have
 * been added since it was recorded and not one of them is on its path.
 *
 * This is the other half. Each check drives one feature and then **reads the pixels back and
 * decides**, because a GL call that returns without error has proved nothing at all - the whole
 * failure mode this project keeps meeting is a call that succeeds and draws the wrong thing.
 *
 * # How a check is written
 *
 * Every check clears to a known colour, draws, and samples specific pixels. A check must be
 * able to *fail*: where the obvious fixture would pass against a broken implementation - a
 * square symmetric about x=y, a colour with equal channels - the fixture is deliberately
 * skewed. That has caught two real bugs in the oops-gl unit tests already, and the same
 * discipline applies here where there is no mutation testing to fall back on.
 *
 * The suite runs identically on the host software rasteriser and on the console, which is the
 * point: a result that differs between them is a hardware-path bug, and nothing else in the
 * repository can see one.
 */

#include "gl1_probe.h"

#include <GL/gl.h>
#include <oops/display.h>

#ifdef OOPS_HOST_BUILD
#include <string.h>
#else
#include <oops/freestd.h>
#endif

/*
 * The area a check works in - a **corner of a full-size display**, not a display of its own.
 *
 * This used to open the display at 128x96, which works perfectly on the host and cannot work on
 * a console: `libSceVideoOut` will not register a buffer of that size. oops-sdk already knew the
 * shape of this - `agc_display.c` promotes a requested 1280x720 to 1080p because 720p buffer
 * registration is refused unless the console is configured for 720p scanout, and records that
 * "universal 1080p is supported across all output modes" - but nothing promotes 128x96, so the
 * open failed, the framebuffer was NULL, and the first check to touch it took the process down
 * with a null read. Every app in this repository that runs on hardware opens 1920x1080.
 *
 * So the display is opened at a size the hardware will scan out, and the probe draws into a
 * PROBE_W x PROBE_H viewport **at GL's origin**, which is the bottom-left. Keeping it there is
 * what makes the change small: `glScissor` is flipped against the framebuffer height by the
 * rasteriser, so a scissor box at the origin lands in the probe's own region exactly as it did
 * when the framebuffer was the probe's size, and every check's coordinates are unchanged. Only
 * `px()` has to know where the region starts.
 */
#define PROBE_W 128
#define PROBE_H 96
#define PROBE_DISPLAY_W 1920
#define PROBE_DISPLAY_H 1080

/* The clear colour every check starts from: a value no check draws, so "unchanged" is
 * distinguishable from "drawn black" and from "drawn white". */
#define PROBE_BG 0xff202020u

static uint32_t *g_fb;
static oops_display_t *g_disp;
static unsigned int g_fb_w;   /* the display's real width, which is the row stride */
static unsigned int g_fb_h;
static unsigned int g_row0;   /* the framebuffer row the probe's logical row 0 sits on */

/*
 * Where a check's pixels come from, and why it is not simply the render target.
 *
 * On the host the software rasteriser writes the target directly, so reading it straight back is
 * both free and correct. **On a console neither is true**, and this file used to do it anyway:
 *
 *   - The draw path builds PM4 into the command buffer and returns. oops-sdk's `gl_draw.c` says
 *     it in as many words - "Host builds have no GPU: the software rasterizer stands in for it
 *     there, and only there" - so until the stream is submitted and its end-of-pipe fence comes
 *     back, nothing has touched the target. `glFinish()` is what does that, and it blocks on the
 *     fence rather than returning optimistically.
 *   - `glClear` takes the same split. So without this a check would not merely miss what it
 *     drew, it would compare against a background that was never painted, and the very first
 *     check would fail on `PROBE_BG`.
 *   - The target is uncached write-combined memory. The copy worth reading is the one the
 *     command processor makes into cached memory inside the same submission.
 *
 * Every sample goes through here, rather than each of the checks remembering to. `glFinish()`
 * with nothing pending is a no-op, so the cost is one submission per draw-then-sample sequence -
 * which is the fewest the question can be asked in.
 */
static const uint32_t *frame(void) {
    glFinish();
#ifndef OOPS_HOST_BUILD
    const GLuint *rb = glGetFrameReadback();
    if (rb) return (const uint32_t *)rb;
#endif
    return (const uint32_t *)g_fb;
}

static uint32_t px(int x, int y) {
    if (!g_fb || x < 0 || y < 0 || x >= PROBE_W || y >= PROBE_H) return 0u;
    const uint32_t *f = frame();
    if (!f) return 0u;
    return f[(size_t)(g_row0 + (unsigned int)y) * (size_t)g_fb_w + (size_t)x];
}

/* Several checks draw one picture two ways and require the results to match word for word.
 * Both go through frame(), so both are synchronised the same way a single sample is - and both
 * walk the probe's own rectangle rather than the whole display, which is now much larger than
 * the area any check touches.
 *
 * **The NULL check is not defensive dressing.** `frame()` answers NULL when the display never
 * opened, and these two used to index it anyway; that is the null read that took the first
 * hardware run of this probe down. `px()` guarded itself and these did not. */
static void frame_snapshot(uint32_t *out) {
    const uint32_t *f = frame();
    if (!f) return;
    for (int y = 0; y < PROBE_H; y++) {
        const uint32_t *row = f + (size_t)(g_row0 + (unsigned int)y) * (size_t)g_fb_w;
        for (int x = 0; x < PROBE_W; x++) out[y * PROBE_W + x] = row[x];
    }
}

static int frame_matches(const uint32_t *want) {
    const uint32_t *f = frame();
    if (!f) return 0;
    for (int y = 0; y < PROBE_H; y++) {
        const uint32_t *row = f + (size_t)(g_row0 + (unsigned int)y) * (size_t)g_fb_w;
        for (int x = 0; x < PROBE_W; x++) {
            if (row[x] != want[y * PROBE_W + x]) return 0;
        }
    }
    return 1;
}

static int chan_r(uint32_t c) { return (int)((c >> 16) & 0xffu); }
static int chan_g(uint32_t c) { return (int)((c >> 8) & 0xffu); }
static int chan_b(uint32_t c) { return (int)(c & 0xffu); }

/* Within a tolerance, because the rasteriser interpolates and the hardware path may round
 * differently from the software one. A check that needs exactness says so by using == itself. */
static int near_rgb(uint32_t c, int r, int g, int b, int tol) {
    int dr = chan_r(c) - r, dg = chan_g(c) - g, db = chan_b(c) - b;
    if (dr < 0) dr = -dr;
    if (dg < 0) dg = -dg;
    if (db < 0) db = -db;
    return dr <= tol && dg <= tol && db <= tol;
}

static void reset_view(void) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_ALPHA_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    /* State the checks below drive and that nothing used to put back, so a check that changed
     * it would have silently changed the meaning of every check after it. */
    glViewport(0, 0, PROBE_W, PROBE_H);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glDepthMask(GL_TRUE);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glClearColor(0.125f, 0.125f, 0.125f, 1.0f); /* 0x20 per channel */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    (void)glGetError();
}

/* A filled rectangle in the given colour, through glRectf. Used as the "something drew" probe
 * by several checks. */
static void draw_rect(float x0, float y0, float x1, float y1, float r, float g, float b) {
    glColor3f(r, g, b);
    glRectf(x0, y0, x1, y1);
}

/* The same, at a chosen eye-space z, for the checks that need two surfaces at different
 * depths. glRectf is flat at z = 0, which is exactly no use to them. */
static void draw_quad_z(float x0, float y0, float x1, float y1, float z,
                        float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex3f(x0, y0, z);
    glVertex3f(x1, y0, z);
    glVertex3f(x1, y1, z);
    glVertex3f(x0, y1, z);
    glEnd();
}

/* ------------------------------------------------------------------------- */

static int check_clear_and_rect(void) {
    reset_view();
    /* **Not square and not centred**, so a transposed or mirrored rectangle fails rather than
     * landing on itself. */
    draw_rect(-0.75f, -0.25f, 0.25f, 0.75f, 1.0f, 0.0f, 0.0f);
    if (glGetError() != GL_NO_ERROR) return 0;

    /* Inside the rectangle: red. The rectangle spans x in [-0.75, 0.25], y in [-0.25, 0.75],
     * so NDC (-0.25, 0.25) is inside and (0.6, -0.6) is outside. */
    uint32_t inside = px(PROBE_W * 3 / 8, PROBE_H * 3 / 8);
    uint32_t outside = px(PROBE_W * 7 / 8, PROBE_H * 7 / 8);
    if (!near_rgb(inside, 255, 0, 0, 8)) return 0;
    if (outside != PROBE_BG) return 0;
    return 1;
}

static int check_scissor(void) {
    reset_view();
    glEnable(GL_SCISSOR_TEST);
    /* A box in the lower-left quarter, in window coordinates. */
    glScissor(0, 0, PROBE_W / 2, PROBE_H / 2);
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f);
    glDisable(GL_SCISSOR_TEST);
    if (glGetError() != GL_NO_ERROR) return 0;

    /* GL's origin is bottom-left and the framebuffer's row 0 is the top, so the scissored
     * region is the *bottom* half in window terms - the lower rows of the image. A probe that
     * got the flip wrong would find the green in the wrong half, which is the point of
     * checking both. */
    uint32_t in_box = px(PROBE_W / 4, PROBE_H * 3 / 4);
    uint32_t out_box = px(PROBE_W * 3 / 4, PROBE_H / 4);
    if (!near_rgb(in_box, 0, 255, 0, 8)) return 0;
    if (out_box != PROBE_BG) return 0;
    return 1;
}

/* User clip planes, on oops-gl's own stage.
 *
 * Worth having as a check rather than as a synthetic probe: obSCEne has measured clip planes
 * three times and every one of those runs reconstructed the stage by hand and used the
 * passthrough VGT_SHADER_STAGES_EN (0x02002000), which is not what oops-gl programmes
 * (0x00c12010). This draws through the same path everything else here does.
 *
 * The plane keeps x >= 0 in eye space, so the left half of the viewport goes and the right half
 * stays. Both halves are sampled: a clipper that discards everything and one that discards
 * nothing each pass a check that only looks at one side.
 */
static int check_clip_plane(void) {
    reset_view();
    static const GLdouble keep_positive_x[4] = {1.0, 0.0, 0.0, 0.0};
    glClipPlane(GL_CLIP_PLANE0, keep_positive_x);
    glEnable(GL_CLIP_PLANE0);
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);
    glDisable(GL_CLIP_PLANE0);
    if (glGetError() != GL_NO_ERROR) return 0;

    const uint32_t cut = px(PROBE_W / 4, PROBE_H / 2);
    const uint32_t kept = px(PROBE_W * 3 / 4, PROBE_H / 2);
    if (cut != PROBE_BG) return 0;
    if (!near_rgb(kept, 255, 0, 255, 8)) return 0;

    /* Disabled, the same draw covers both halves - so the cut above was the plane and not some
     * other reason the left side was empty. */
    reset_view();
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);
    if (glGetError() != GL_NO_ERROR) return 0;
    if (!near_rgb(px(PROBE_W / 4, PROBE_H / 2), 255, 0, 255, 8)) return 0;
    return 1;
}

/* Texture coordinate generation.
 *
 * Generation happens on the CPU at vertex assembly, so the coordinate reaches the hardware in the
 * vertex buffer like any other - this ought to pass on the console. The check is here because
 * "ought to" is what the last eight failures all were: a feature implemented on the software path
 * and never once run through the real one.
 *
 * GL_OBJECT_LINEAR with the plane (0.5, 0, 0, 0.5) maps object x in [-1, 1] onto s in [0, 1], so
 * the quad's left edge samples column 0 and its right edge column 1 of a two-column texture.
 */
static int check_texgen(void) {
    reset_view();
    static const GLubyte texels[8] = {
        255, 0, 0, 255,      0, 0, 255, 255,   /* one row: red then blue */
    };
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    static const GLfloat splane[4] = {0.5f, 0.0f, 0.0f, 0.5f};
    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glTexGenfv(GL_S, GL_OBJECT_PLANE, splane);
    glEnable(GL_TEXTURE_GEN_S);

    /* t is supplied by hand, which is the ordinary way this is used. */
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.9f, -0.9f, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f( 0.9f, -0.9f, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f( 0.9f,  0.9f, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.9f,  0.9f, 0.0f);
    glEnd();

    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_2D);
    if (glGetError() != GL_NO_ERROR) { glDeleteTextures(1, &tex); return 0; }

    const uint32_t left = px(PROBE_W / 4, PROBE_H / 2);
    const uint32_t right = px(PROBE_W * 3 / 4, PROBE_H / 2);
    glDeleteTextures(1, &tex);
    if (!near_rgb(left, 255, 0, 0, 8)) return 0;
    if (!near_rgb(right, 0, 0, 255, 8)) return 0;
    return 1;
}

/* Stencil.
 *
 * **This is expected to fail on hardware today**, and that is the point of having it. Stencil is
 * implemented on the software rasteriser only; the hardware path needs a stencil surface and the
 * DB_STENCIL_INFO value that turns it on, which is obSCEne REQ-20260917T1845Z-3d5b. Until that
 * lands the console has no stencil buffer, so the masked draw below will not be masked.
 *
 * A check that fails honestly is worth more than a gap nothing mentions: this is the line that
 * will change from FAIL to pass when the surface is bound, and it is how we will know.
 */
static int check_stencil(void) {
    reset_view();
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
    glEnable(GL_STENCIL_TEST);

    /* Stamp 1 into the left half. */
    glStencilFunc(GL_ALWAYS, 1, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    draw_rect(-1.0f, -1.0f, 0.0f, 1.0f, 0.2f, 0.2f, 0.2f);

    /* Then draw everywhere, keeping only where the stamp landed. */
    glStencilFunc(GL_EQUAL, 1, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f);
    glDisable(GL_STENCIL_TEST);
    if (glGetError() != GL_NO_ERROR) return 0;

    const uint32_t inside = px(PROBE_W / 4, PROBE_H / 2);
    const uint32_t outside = px(PROBE_W * 3 / 4, PROBE_H / 2);
    if (!near_rgb(inside, 0, 255, 0, 8)) return 0;
    /* The right half was never stamped, so the second draw must have been rejected there and
     * the first draw's grey must still be showing. */
    if (near_rgb(outside, 0, 255, 0, 8)) return 0;
    return 1;
}

/* glDrawPixels and glBitmap at the raster position.
 *
 * These write the colour buffer from the CPU rather than going through the rasteriser, so what
 * this really checks on hardware is that the frame is flushed first - a pixel rectangle written
 * into a target the GPU is about to draw over would vanish, which is the same class of bug the
 * alpha test had.
 */
static int check_raster_ops(void) {
    reset_view();
    /* An 8x8 block rather than a 2x2, and counted rather than point-sampled.
     *
     * GL's window y runs up from the bottom and this probe's logical row 0 is the top of its
     * region, so the two conventions differ by a pixel at the exact centre - a 2x2 block lands
     * just beside where a centre sample looks. Counting is the honest check anyway: what this
     * asks is "did the rectangle reach the colour buffer", and a point sample answers that only
     * if the arithmetic on both sides already agrees. */
    static GLubyte block[8 * 8 * 4];
    for (int i = 0; i < 8 * 8; i++) {
        block[i * 4 + 0] = 255; block[i * 4 + 1] = 0;
        block[i * 4 + 2] = 255; block[i * 4 + 3] = 255;
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glRasterPos2f(0.0f, 0.0f);
    GLint valid = 0;
    glGetIntegerv(GL_CURRENT_RASTER_POSITION_VALID, &valid);
    if (!valid) return 0;
    glDrawPixels(8, 8, GL_RGBA, GL_UNSIGNED_BYTE, block);
    if (glGetError() != GL_NO_ERROR) return 0;

    int drawn = 0;
    for (int y = 0; y < PROBE_H; y++) {
        for (int x = 0; x < PROBE_W; x++) {
            if (near_rgb(px(x, y), 255, 0, 255, 8)) drawn++;
        }
    }
    if (drawn < 60) return 0;

    /* And it landed near the middle, not in a corner - so "it drew" is not the whole claim. */
    int near_middle = 0;
    for (int y = PROBE_H / 4; y < PROBE_H * 3 / 4; y++) {
        for (int x = PROBE_W / 4; x < PROBE_W * 3 / 4; x++) {
            if (near_rgb(px(x, y), 255, 0, 255, 8)) near_middle++;
        }
    }
    if (near_middle < 60) return 0;

    /* A position that clipped draws nothing, which is the rule most easily got wrong. */
    reset_view();
    glRasterPos2f(5.0f, 5.0f);
    glGetIntegerv(GL_CURRENT_RASTER_POSITION_VALID, &valid);
    if (valid) return 0;
    glDrawPixels(8, 8, GL_RGBA, GL_UNSIGNED_BYTE, block);
    if (glGetError() != GL_NO_ERROR) return 0;
    for (int y = 0; y < PROBE_H; y++) {
        for (int x = 0; x < PROBE_W; x++) {
            if (near_rgb(px(x, y), 255, 0, 255, 8)) return 0;
        }
    }
    return 1;
}

static int check_colour_mask(void) {
    reset_view();
    /* Green off: a white rectangle must come back magenta, not white and not grey. */
    glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE);
    draw_rect(-0.5f, -0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (glGetError() != GL_NO_ERROR) return 0;

    uint32_t c = px(PROBE_W / 2, PROBE_H / 2);
    /* Red and blue written, green left at the clear value of 0x20. */
    if (chan_r(c) < 240 || chan_b(c) < 240) return 0;
    if (chan_g(c) > 0x40) return 0;
    return 1;
}

static int check_blend(void) {
    reset_view();
    draw_rect(-0.5f, -0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 1.0f, 0.5f);
    glRectf(-0.5f, -0.5f, 0.5f, 0.5f);
    glDisable(GL_BLEND);
    if (glGetError() != GL_NO_ERROR) return 0;

    /* Half red, half blue. **Neither channel may be at an extreme** - if blending were ignored
     * the result is pure blue, and if the source alpha were ignored it is pure blue too, so
     * the test is that both channels are in the middle. */
    uint32_t c = px(PROBE_W / 2, PROBE_H / 2);
    if (chan_r(c) < 90 || chan_r(c) > 170) return 0;
    if (chan_b(c) < 90 || chan_b(c) > 170) return 0;
    return 1;
}

static int check_depth(void) {
    reset_view();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* **The camera looks down -z, so a larger eye z is nearer.** With
     * glOrtho(-1, 1, -1, 1, -1, 1) the mapping is window_z = (1 - eye_z) / 2, which puts
     * eye z = +0.5 at 0.25 (near) and eye z = -0.5 at 0.75 (far). Getting that backwards is
     * easy and gives a check that fails against a correct implementation, which is worse than
     * no check at all.
     *
     * Near first, then far over the top of it: with GL_LESS the far one must lose. That is the
     * case which fails if depth is written but never compared. */
    glColor3f(0.0f, 1.0f, 0.0f); /* near */
    glBegin(GL_QUADS);
    glVertex3f(-0.5f, -0.5f, 0.5f); glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);   glVertex3f(-0.5f, 0.5f, 0.5f);
    glEnd();
    glColor3f(1.0f, 0.0f, 0.0f); /* far, and must not win */
    glBegin(GL_QUADS);
    glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);   glVertex3f(-0.5f, 0.5f, -0.5f);
    glEnd();
    glDisable(GL_DEPTH_TEST);
    if (glGetError() != GL_NO_ERROR) return 0;

    uint32_t c = px(PROBE_W / 2, PROBE_H / 2);
    return near_rgb(c, 0, 255, 0, 8);
}

static int check_vertex_arrays(void) {
    reset_view();
    static const GLfloat verts[12] = {
        -0.6f, -0.6f, 0.0f,   0.6f, -0.6f, 0.0f,
         0.6f,  0.6f, 0.0f,  -0.6f,  0.6f, 0.0f,
    };
    static const GLfloat cols[16] = {
        1.0f, 0.0f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
    };
    static const GLushort idx[6] = {0, 1, 2, 0, 2, 3};

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, verts);
    glColorPointer(4, GL_FLOAT, 0, cols);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, idx);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    if (glGetError() != GL_NO_ERROR) return 0;

    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 255, 0, 0, 8);
}

/* GL 1.5 buffer objects, which is how nearly all code written since about 2003 draws. The
 * pointer is an *offset* here, and offset zero arrives as NULL - the case a naive array reader
 * mistakes for "no array bound". */
static int check_buffer_objects(void) {
    reset_view();
    static const GLfloat verts[12] = {
        -0.6f, -0.6f, 0.0f,   0.6f, -0.6f, 0.0f,
         0.6f,  0.6f, 0.0f,  -0.6f,  0.6f, 0.0f,
    };
    static const GLushort idx[6] = {0, 1, 2, 0, 2, 3};

    GLuint vbo = 0, ibo = 0;
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);
    if (vbo == 0u || ibo == 0u) return 0;

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof verts, verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)sizeof idx, idx, GL_STATIC_DRAW);

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, NULL); /* offset 0 into the bound buffer */
    glColor3f(0.0f, 0.0f, 1.0f);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ibo);
    if (glGetError() != GL_NO_ERROR) return 0;

    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 0, 0, 255, 8);
}

static int check_display_list(void) {
    reset_view();
    GLuint list = glGenLists(1);
    if (list == 0u) return 0;
    glNewList(list, GL_COMPILE);
    glColor3f(1.0f, 1.0f, 0.0f);
    glRectf(-0.4f, -0.4f, 0.4f, 0.4f);
    glEndList();
    if (glGetError() != GL_NO_ERROR) return 0;

    /* Nothing should have been drawn by compiling it. */
    if (px(PROBE_W / 2, PROBE_H / 2) != PROBE_BG) return 0;

    glCallList(list);
    glDeleteLists(list, 1);
    if (glGetError() != GL_NO_ERROR) return 0;

    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 255, 255, 0, 8);
}

static int check_texture(void) {
    reset_view();
    /* A 2x2 texture with four *different* colours, so a transposed or flipped sample lands on
     * a different one. A checkerboard of two colours would not. */
    static const GLubyte texels[16] = {
        255, 0, 0, 255,      0, 255, 0, 255,
        0, 0, 255, 255,      255, 255, 0, 255,
    };
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor3f(1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.8f, -0.8f, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 0.8f, -0.8f, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 0.8f,  0.8f, 0.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.8f,  0.8f, 0.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    if (glGetError() != GL_NO_ERROR) { glDeleteTextures(1, &tex); return 0; }

    /* Four quadrants, four colours. Which corner holds which depends on the orientation, so
     * this asserts only that **all four are present and distinct** - a sampler returning one
     * colour everywhere, or two, fails. Orientation is gl-cube's job, with a measured frame to
     * compare against. */
    uint32_t a = px(PROBE_W * 5 / 16, PROBE_H * 5 / 16);
    uint32_t b = px(PROBE_W * 11 / 16, PROBE_H * 5 / 16);
    uint32_t c = px(PROBE_W * 5 / 16, PROBE_H * 11 / 16);
    uint32_t d = px(PROBE_W * 11 / 16, PROBE_H * 11 / 16);
    /* Deleted *after* the pixels are read. Deleting first is legal GL and is what this used to
     * do, and it is a separate question with a check of its own at the end of the suite - so it
     * is asked once, deliberately, where a failure costs one result instead of every result
     * after it. See `tex-delete-in-frame`. */
    glDeleteTextures(1, &tex);
    if (a == b || a == c || a == d || b == c || b == d || c == d) return 0;
    if (a == PROBE_BG || b == PROBE_BG || c == PROBE_BG || d == PROBE_BG) return 0;
    return 1;
}

static int check_read_pixels(void) {
    reset_view();
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);

    static GLubyte row[PROBE_W * 4];
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, PROBE_W, 1, GL_RGBA, GL_UNSIGNED_BYTE, row);
    if (glGetError() != GL_NO_ERROR) return 0;

    /* Magenta: red and blue high, green low. A channel-order mistake shows here because the
     * three differ. */
    if (row[0] < 240 || row[2] < 240) return 0;
    if (row[1] > 16) return 0;
    return 1;
}

static int check_matrix_stack(void) {
    reset_view();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(0.5f, 0.0f, 0.0f);
    draw_rect(-0.2f, -0.2f, 0.2f, 0.2f, 0.0f, 1.0f, 1.0f);
    glPopMatrix();
    if (glGetError() != GL_NO_ERROR) return 0;

    /* Translated right by 0.5 in NDC: present at x = +0.5, absent at the origin. If the pop
     * failed to restore, the *next* check would drift - so this also draws at the origin after
     * the pop and requires it to land there. */
    uint32_t moved = px(PROBE_W * 3 / 4, PROBE_H / 2);
    uint32_t origin_before = px(PROBE_W / 2, PROBE_H / 2);
    if (!near_rgb(moved, 0, 255, 255, 8)) return 0;
    if (origin_before != PROBE_BG) return 0;

    draw_rect(-0.2f, -0.2f, 0.2f, 0.2f, 1.0f, 0.5f, 0.0f);
    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 255, 128, 0, 12);
}

static int check_attrib_stack(void) {
    reset_view();
    glDisable(GL_BLEND);
    glPushAttrib(GL_COLOR_BUFFER_BIT | GL_ENABLE_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glPopAttrib();
    if (glGetError() != GL_NO_ERROR) return 0;

    /* Blending must be off again. Drawing the same rectangle twice additively would saturate;
     * with the pop honoured it simply replaces, so the result is the plain colour. */
    draw_rect(-0.5f, -0.5f, 0.5f, 0.5f, 0.25f, 0.0f, 0.0f);
    draw_rect(-0.5f, -0.5f, 0.5f, 0.5f, 0.25f, 0.0f, 0.0f);
    uint32_t c = px(PROBE_W / 2, PROBE_H / 2);
    if (chan_r(c) > 90) return 0;   /* saturated or doubled means the pop did not take */
    if (chan_r(c) < 40) return 0;
    return 1;
}

static int check_alpha_test(void) {
    reset_view();
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);

    /* Below the reference: must be discarded entirely. */
    glColor4f(1.0f, 0.0f, 0.0f, 0.25f);
    glRectf(-0.5f, -0.5f, 0.0f, 0.5f);
    /* Above it: must survive. */
    glColor4f(0.0f, 1.0f, 0.0f, 0.75f);
    glRectf(0.0f, -0.5f, 0.5f, 0.5f);
    glDisable(GL_ALPHA_TEST);
    if (glGetError() != GL_NO_ERROR) return 0;

    /* **Sampled well inside each rectangle, not on its edge.** The left one spans x pixels
     * 32..64 and the right one 64..96, so PROBE_W/4 and PROBE_W*3/4 land exactly on the
     * boundaries where a fill-rule difference decides the result. */
    uint32_t rejected = px(PROBE_W * 3 / 8, PROBE_H / 2);
    uint32_t kept = px(PROBE_W * 5 / 8, PROBE_H / 2);
    if (rejected != PROBE_BG) return 0;
    if (!near_rgb(kept, 0, 255, 0, 8)) return 0;
    return 1;
}

/* Points and lines, which reach the hardware as triangles.
 *
 * **This is the check that tests the claim.** obSCEne measured that a one- or two-vertex
 * primitive stalls the pipe - `fence-hit 0` across five sweeps, one of them on oops-gl's own
 * `VGT_SHADER_STAGES_EN`. The conclusion drawn from that was "points and lines need a stage this
 * does not build". The narrower reading is that the *native* primitive is shut, and that a line
 * drawn as a quad of two triangles asks the hardware for nothing it has not already retired.
 *
 * If the narrower reading is right this passes on the console. If it is wrong - if something
 * about the expansion or the inverse-matrix round trip does not survive the real pipeline - this
 * fails, and it fails here rather than in a title.
 */
static int check_points_and_lines(void) {
    reset_view();
    glLineWidth(4.0f);
    glColor3f(0.0f, 1.0f, 1.0f);
    glBegin(GL_LINES);
    glVertex3f(-0.8f, 0.0f, 0.0f);
    glVertex3f( 0.8f, 0.0f, 0.0f);
    glEnd();
    if (glGetError() != GL_NO_ERROR) return 0;

    /* The line crosses the middle, so the centre row is lit and the corners are not. */
    if (!near_rgb(px(PROBE_W / 2, PROBE_H / 2), 0, 255, 255, 8)) return 0;
    if (px(4, 4) != PROBE_BG) return 0;

    /* A point of a decent size lands where it was put. */
    reset_view();
    glPointSize(8.0f);
    glColor3f(1.0f, 1.0f, 0.0f);
    glBegin(GL_POINTS);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glEnd();
    if (glGetError() != GL_NO_ERROR) return 0;
    if (!near_rgb(px(PROBE_W / 2, PROBE_H / 2), 255, 255, 0, 8)) return 0;
    if (px(4, 4) != PROBE_BG) return 0;

    /* A loop closes: three vertices give three segments, so the third edge is lit where a strip
     * would have left the background. Sampled near the midpoint of the closing edge. */
    reset_view();
    glLineWidth(3.0f);
    glColor3f(1.0f, 0.0f, 1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(-0.7f, -0.7f, 0.0f);
    glVertex3f( 0.7f, -0.7f, 0.0f);
    glVertex3f( 0.0f,  0.7f, 0.0f);
    glEnd();
    if (glGetError() != GL_NO_ERROR) return 0;
    {
        /* The closing edge runs from (0, 0.7) back to (-0.7, -0.7); its midpoint is about
         * (-0.35, 0) in NDC, which is a quarter of the way in from the left at mid height. */
        const uint32_t on_closing_edge = px(PROBE_W * 5 / 16, PROBE_H / 2);
        if (!near_rgb(on_closing_edge, 255, 0, 255, 24)) return 0;
    }
    glLineWidth(1.0f);
    glPointSize(1.0f);
    return 1;
}

/* Fog.
 *
 * **Expected to fail on hardware today**, like `stencil`. Fog is implemented on the software
 * rasteriser; the pixel shaders do not blend towards the fog colour yet. When they do this goes
 * from FAIL to pass, and that line is how we will know.
 *
 * The likely route does not need the third parameter export `-7c40` confirmed: the texture
 * coordinate the vertex shader already exports is a vec4 whose z is unused, so a fog factor
 * computed per vertex on the CPU can ride there and only the pixel shaders change.
 */
static int check_fog(void) {
    reset_view();
    /* Wide near/far so a translated quad stays inside the clip volume. */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -20.0, 20.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    static const GLfloat blue[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    glFogfv(GL_FOG_COLOR, blue);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 1.0f);
    glFogf(GL_FOG_END, 3.0f);
    glEnable(GL_FOG);

    /* Far beyond the end: a red quad must come out as the fog colour. */
    glTranslatef(0.0f, 0.0f, -10.0f);
    draw_rect(-0.5f, -0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f);
    glDisable(GL_FOG);
    if (glGetError() != GL_NO_ERROR) return 0;

    if (!near_rgb(px(PROBE_W / 2, PROBE_H / 2), 0, 0, 255, 8)) return 0;
    return 1;
}

static int check_refusals(void) {
    reset_view();
    /* **The refusals are a feature and are checked like one.**
     *
     * Points and lines used to be checked here as refusals, on the grounds that a build which
     * quietly started accepting them would be claiming something the hardware had denied. The
     * distinction that note missed is what the measurement actually closed: the geometry engine
     * stalls on a one- or two-vertex *primitive*. It says nothing about drawing a line, which is
     * a screen-width quad and therefore triangles - and triangles are the thing the same sweeps
     * showed retiring and drawing.
     *
     * So they are checked as *draws* now, and `check_points_and_lines` below is where that is
     * verified properly. If the expansion is wrong on hardware, that check fails and this one
     * stays quiet, which is the right division of labour. */
    (void)glGetError();
    glBegin(GL_POINTS);
    glEnd();
    if (glGetError() != GL_NO_ERROR) return 0;
    glBegin(GL_LINES);
    glEnd();
    if (glGetError() != GL_NO_ERROR) return 0;

    /* An enum that is not a primitive mode at all is still refused. */
    glBegin((GLenum)0xdeadbeefu);
    glEnd();
    if (glGetError() != GL_INVALID_ENUM) return 0;

    /* A capability this subset does not have is refused rather than dropped. Written as its
     * specification value because oops-gl deliberately does not declare it - D009: an absent
     * feature is an absent symbol, so `GL_DITHER` is not in <GL/gl.h> at all. */
    glEnable(0x0BD0u /* GL_DITHER */);
    if (glGetError() != GL_INVALID_ENUM) return 0;

    /* And a query it cannot answer refuses rather than leaving the caller's buffer as it was. */
    GLint v = 0x5eed;
    glGetIntegerv(0x8000u /* GL_FOG_HINT */, &v);
    if (glGetError() != GL_INVALID_ENUM) return 0;
    if (v != 0x5eed) return 0;
    return 1;
}

/* **Lighting, which gl1-cube exercises and nothing checked independently.**
 *
 * A directional light down -z, and two quads whose normals face towards and away from it. The
 * lit one must be brighter. The fixture matters: a light at (0,0,1) with both normals differing
 * only in sign is the smallest arrangement where a normal that is ignored, normalised wrongly,
 * or transformed by the wrong matrix gives the same brightness for both.
 */
static int check_lighting(void) {
    reset_view();
    static const GLfloat pos[4] = {0.0f, 0.0f, 1.0f, 0.0f}; /* w=0: directional */
    static const GLfloat white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    static const GLfloat dim[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, white);
    glLightfv(GL_LIGHT0, GL_AMBIENT, dim);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, white);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, dim);

    /* Facing the light. */
    glNormal3f(0.0f, 0.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex3f(-0.9f, -0.4f, 0.0f); glVertex3f(-0.1f, -0.4f, 0.0f);
    glVertex3f(-0.1f,  0.4f, 0.0f); glVertex3f(-0.9f,  0.4f, 0.0f);
    glEnd();
    /* Facing away. */
    glNormal3f(0.0f, 0.0f, -1.0f);
    glBegin(GL_QUADS);
    glVertex3f(0.1f, -0.4f, 0.0f); glVertex3f(0.9f, -0.4f, 0.0f);
    glVertex3f(0.9f,  0.4f, 0.0f); glVertex3f(0.1f,  0.4f, 0.0f);
    glEnd();
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    if (glGetError() != GL_NO_ERROR) return 0;

    uint32_t lit = px(PROBE_W / 4, PROBE_H / 2);
    uint32_t unlit = px(PROBE_W * 3 / 4, PROBE_H / 2);
    if (chan_r(lit) < 200) return 0;        /* the facing quad is bright */
    if (chan_r(unlit) > 60) return 0;       /* the away-facing one is not */
    return 1;
}

/* Texture environment: GL_REPLACE ignores the vertex colour, GL_MODULATE multiplies by it.
 * **The vertex colour is deliberately not white**, because with white the two modes agree and
 * the check would pass against an implementation that only ever replaced. */
static int check_tex_env_modes(void) {
    reset_view();
    static const GLubyte texel[4] = {255, 255, 255, 255};
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEnable(GL_TEXTURE_2D);

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor3f(0.25f, 0.0f, 0.0f);
    glRectf(-0.9f, -0.4f, -0.1f, 0.4f);

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor3f(0.25f, 0.0f, 0.0f);
    glRectf(0.1f, -0.4f, 0.9f, 0.4f);

    glDisable(GL_TEXTURE_2D);
    glDeleteTextures(1, &tex);
    if (glGetError() != GL_NO_ERROR) return 0;

    uint32_t replaced = px(PROBE_W / 4, PROBE_H / 2);
    uint32_t modulated = px(PROBE_W * 3 / 4, PROBE_H / 2);
    /* Replace takes the white texel whole; modulate scales it by the dark red. */
    if (chan_r(replaced) < 240 || chan_g(replaced) < 240) return 0;
    if (chan_r(modulated) > 90 || chan_g(modulated) > 30) return 0;
    return 1;
}

/* Render-to-texture, which is what glCopyTexSubImage2D is for. Draws, copies the framebuffer
 * into a texture, clears, then draws the texture back and checks it survived the round trip. */
static int check_copy_tex(void) {
    reset_view();
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.75f, 1.0f);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 32, 32, 0);
    if (glGetError() != GL_NO_ERROR) { glDeleteTextures(1, &tex); return 0; }

    glClear(GL_COLOR_BUFFER_BIT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.8f, -0.8f, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 0.8f, -0.8f, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 0.8f,  0.8f, 0.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.8f,  0.8f, 0.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glDeleteTextures(1, &tex);
    if (glGetError() != GL_NO_ERROR) return 0;

    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 0, 191, 255, 16);
}

/* `glGetTexImage` reads a texture back. **Exact, not approximate**: no rasteriser is involved,
 * so anything other than the bytes that went in is a bug rather than a rounding difference. */
static int check_get_tex_image(void) {
    reset_view();
    static const GLubyte src[16] = {
        10, 60, 110, 160,   11, 61, 111, 161,
        12, 62, 112, 162,   13, 63, 113, 163,
    };
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, src);

    GLubyte back[16];
    for (int i = 0; i < 16; i++) back[i] = 0xcd;
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, back);
    glDeleteTextures(1, &tex);
    if (glGetError() != GL_NO_ERROR) return 0;

    for (int i = 0; i < 16; i++) {
        if (back[i] != src[i]) return 0;
    }
    return 1;
}

/* `glArrayElement`, `glDrawRangeElements` and `glInterleavedArrays` all draw the same quad three
 * different ways. **Each is compared against glDrawElements**, so this fails if any one of them
 * disagrees with the path that is already known to work. */
static int check_array_paths(void) {
    static const GLfloat verts[12] = {
        -0.7f, -0.7f, 0.0f,   0.7f, -0.7f, 0.0f,
         0.7f,  0.7f, 0.0f,  -0.7f,  0.7f, 0.0f,
    };
    static const GLushort idx[6] = {0, 1, 2, 0, 2, 3};
    static uint32_t want[PROBE_W * PROBE_H];

    reset_view();
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, verts);
    glColor3f(1.0f, 0.5f, 0.0f);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, idx);
    frame_snapshot(want);
    if (glGetError() != GL_NO_ERROR) return 0;

    /* glDrawRangeElements: the range is a hint, the picture must be identical. */
    reset_view();
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, verts);
    glColor3f(1.0f, 0.5f, 0.0f);
    glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, idx);
    if (!frame_matches(want)) { glDisableClientState(GL_VERTEX_ARRAY); return 0; }

    /* glArrayElement inside glBegin/glEnd: the same six vertices, assembled by hand. */
    reset_view();
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, verts);
    glColor3f(1.0f, 0.5f, 0.0f);
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 6; i++) glArrayElement((GLint)idx[i]);
    glEnd();
    if (!frame_matches(want)) { glDisableClientState(GL_VERTEX_ARRAY); return 0; }
    glDisableClientState(GL_VERTEX_ARRAY);

    /* glInterleavedArrays with GL_V3F: the same positions out of one buffer. */
    reset_view();
    glInterleavedArrays(GL_V3F, 0, verts);
    glColor3f(1.0f, 0.5f, 0.0f);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, idx);
    if (!frame_matches(want)) { glDisableClientState(GL_VERTEX_ARRAY); return 0; }
    glDisableClientState(GL_VERTEX_ARRAY);
    return glGetError() == GL_NO_ERROR;
}

/* The other spellings of the attribute calls must reach the same place as the float ones.
 * Drawn twice and compared pixel for pixel. */
static int check_type_variants(void) {
    static uint32_t want[PROBE_W * PROBE_H];

    reset_view();
    glColor3f(1.0f, 0.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex3f(-0.6f, -0.6f, 0.0f); glVertex3f(0.6f, -0.6f, 0.0f);
    glVertex3f( 0.6f,  0.6f, 0.0f); glVertex3f(-0.6f, 0.6f, 0.0f);
    glEnd();
    frame_snapshot(want);
    if (glGetError() != GL_NO_ERROR) return 0;

    reset_view();
    glColor3ub(255, 0, 255); /* 255 must divide by 255, not shift by 8 */
    glBegin(GL_QUADS);
    glVertex3d(-0.6, -0.6, 0.0); glVertex3d(0.6, -0.6, 0.0);
    glVertex3d( 0.6,  0.6, 0.0); glVertex3d(-0.6, 0.6, 0.0);
    glEnd();
    if (!frame_matches(want)) return 0;
    return glGetError() == GL_NO_ERROR;
}

/* The client attribute stack is a separate stack from the server one, and the pop must restore
 * the array state without disturbing anything else. */
static int check_client_attrib(void) {
    reset_view();
    static const GLfloat verts[12] = {
        -0.6f, -0.6f, 0.0f,   0.6f, -0.6f, 0.0f,
         0.6f,  0.6f, 0.0f,  -0.6f,  0.6f, 0.0f,
    };
    glVertexPointer(3, GL_FLOAT, 0, verts);
    glEnableClientState(GL_VERTEX_ARRAY);

    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    glDisableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 64, NULL);
    glPopClientAttrib();
    if (glGetError() != GL_NO_ERROR) return 0;

    /* If the pop worked the array is enabled again and points at `verts`, so this draws. */
    glColor3f(0.0f, 1.0f, 0.5f);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisableClientState(GL_VERTEX_ARRAY);
    if (glGetError() != GL_NO_ERROR) return 0;

    return near_rgb(px(PROBE_W / 2 - 8, PROBE_H / 2 + 8), 0, 255, 128, 12);
}

/* `glDepthRange` decides where NDC z lands in the depth buffer, and reversing it reverses which
 * of two fragments wins. **Two draws in the same order with the range flipped** - if the range
 * were ignored, both would give the same answer. */
static int check_depth_range(void) {
    reset_view();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glDepthRange(0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glColor3f(0.0f, 1.0f, 0.0f);
    glRectf(-0.5f, -0.5f, 0.5f, 0.5f); /* z = 0 */
    glColor3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);   glVertex3f(-0.5f, 0.5f, -0.5f);
    glEnd();
    uint32_t normal = px(PROBE_W / 2, PROBE_H / 2);

    glDepthRange(1.0, 0.0); /* reversed */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glColor3f(0.0f, 1.0f, 0.0f);
    glRectf(-0.5f, -0.5f, 0.5f, 0.5f);
    glColor3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);   glVertex3f(-0.5f, 0.5f, -0.5f);
    glEnd();
    uint32_t reversed = px(PROBE_W / 2, PROBE_H / 2);

    glDepthRange(0.0, 1.0);
    glDisable(GL_DEPTH_TEST);
    if (glGetError() != GL_NO_ERROR) return 0;

    /* The two must differ; which is which depends on the sign convention, and that is
     * gl1-cube's business because it has a measured frame to compare against. */
    return normal != reversed;
}

/* The per-object queries answer from the same field their setters write. Not a pixel check -
 * these have no visible effect - but a query that disagrees with the state it is querying is
 * exactly the bug this found in glIsEnabled. */
static int check_object_queries(void) {
    reset_view();
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GLint iv = 0;
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &iv);
    glDeleteTextures(1, &tex);
    if (iv != (GLint)GL_CLAMP_TO_EDGE) return 0;

    static const GLfloat amb[4] = {0.125f, 0.25f, 0.5f, 0.75f};
    GLfloat fv[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glLightfv(GL_LIGHT1, GL_AMBIENT, amb);
    glGetLightfv(GL_LIGHT1, GL_AMBIENT, fv);
    if (fv[0] != 0.125f || fv[3] != 0.75f) return 0;

    /* glIsEnabled must answer from the same list glEnable accepts - these two were absent once
     * and read back as off immediately after being switched on. */
    glEnable(GL_ALPHA_TEST);
    if (glIsEnabled(GL_ALPHA_TEST) != GL_TRUE) return 0;
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_POLYGON_OFFSET_FILL);
    if (glIsEnabled(GL_POLYGON_OFFSET_FILL) != GL_TRUE) return 0;
    glDisable(GL_POLYGON_OFFSET_FILL);

    return glGetError() == GL_NO_ERROR;
}

/* **Polygon offset**, which moves a filled polygon's depth so coplanar geometry can be drawn
 * over it. Two quads at exactly the same z with GL_LESS: without the offset the second loses,
 * with a negative offset pulling it nearer it wins. */
static int check_polygon_offset(void) {
    reset_view();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glColor3f(0.0f, 1.0f, 0.0f);
    glRectf(-0.5f, -0.5f, 0.5f, 0.5f);

    /* Same plane, so GL_LESS rejects it - this is the control. */
    glColor3f(1.0f, 0.0f, 0.0f);
    glRectf(-0.5f, -0.5f, 0.5f, 0.5f);
    if (!near_rgb(px(PROBE_W / 2, PROBE_H / 2), 0, 255, 0, 8)) {
        glDisable(GL_DEPTH_TEST);
        return 0;
    }

    /* With the offset pulling it nearer, it must now win. */
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0.0f, -4.0f);
    glColor3f(1.0f, 0.0f, 0.0f);
    glRectf(-0.5f, -0.5f, 0.5f, 0.5f);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_DEPTH_TEST);
    if (glGetError() != GL_NO_ERROR) return 0;

    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 255, 0, 0, 8);
}

/* **The blend equation, which was stored and used by nothing.**
 *
 * `GL_FUNC_SUBTRACT` against a known destination must come out darker than `GL_FUNC_ADD` does,
 * and `GL_MAX` must ignore the factors entirely. The fixture uses a mid-grey destination and a
 * mid-grey source so that add saturates upward and subtract lands near zero - with a black
 * destination, add and subtract of the same source agree and the check proves nothing.
 */
static int check_blend_equation(void) {
    reset_view();
    /* Destination: mid grey. */
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    glBlendEquation(GL_FUNC_ADD);
    glColor3f(0.5f, 0.5f, 0.5f);
    glRectf(-0.9f, -0.4f, -0.5f, 0.4f);

    glBlendEquation(GL_FUNC_SUBTRACT);
    glColor3f(0.5f, 0.5f, 0.5f);
    glRectf(-0.3f, -0.4f, 0.1f, 0.4f);

    glBlendEquation(GL_MAX);
    glColor3f(0.1f, 0.1f, 0.1f); /* darker than the destination, so MAX must keep the dest */
    glRectf(0.3f, -0.4f, 0.9f, 0.4f);

    glBlendEquation(GL_FUNC_ADD);
    glDisable(GL_BLEND);
    if (glGetError() != GL_NO_ERROR) return 0;

    uint32_t added = px(PROBE_W * 3 / 16, PROBE_H / 2);
    uint32_t subtracted = px(PROBE_W * 7 / 16, PROBE_H / 2);
    uint32_t maxed = px(PROBE_W * 12 / 16, PROBE_H / 2);

    if (chan_r(added) < 240) return 0;        /* 0.5 + 0.5 saturates to white */
    if (chan_r(subtracted) > 16) return 0;    /* 0.5 - 0.5 is black */
    if (!near_rgb(maxed, 128, 128, 128, 12)) return 0; /* max(0.1, 0.5) keeps the destination */
    return 1;
}

/* **Two-sided lighting is refused, and that is the behaviour being checked.**
 *
 * It needs a primitive's facing, which is not known when this computes lighting per vertex. It
 * used to set a field nothing read, so the call returned clean and nothing happened. A probe
 * that only tested features would not notice a refusal going missing, which is why the
 * refusals are checked as carefully as the features. */
static int check_two_side_refused(void) {
    reset_view();
    (void)glGetError();
    static const GLfloat on[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, on);
    if (glGetError() != GL_INVALID_ENUM) return 0;

    /* The light model's other parameters still work, so this is a refusal of one thing rather
     * than of the call. */
    static const GLfloat amb[4] = {0.25f, 0.25f, 0.25f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
    if (glGetError() != GL_NO_ERROR) return 0;
    GLfloat back[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glGetFloatv(GL_LIGHT_MODEL_AMBIENT, back);
    if (back[0] != 0.25f) return 0;
    return glGetError() == GL_NO_ERROR;
}

/* ------------------------------------------------------------------------- */

/* Two triangles of the same shape, left and right, wound opposite ways. */
static void cull_pair(void) {
    glColor3f(1.0f, 1.0f, 0.0f);
    glBegin(GL_TRIANGLES);
    glVertex3f(-0.9f, -0.4f, 0.0f);
    glVertex3f(-0.1f, -0.4f, 0.0f);
    glVertex3f(-0.5f,  0.4f, 0.0f);

    glVertex3f( 0.9f, -0.4f, 0.0f);
    glVertex3f( 0.1f, -0.4f, 0.0f);
    glVertex3f( 0.5f,  0.4f, 0.0f);
    glEnd();
}

#define CULL_LEFT_X  (PROBE_W / 4)
#define CULL_RIGHT_X (PROBE_W * 3 / 4)
#define CULL_Y       (PROBE_H * 55 / 100)

/*
 * Face culling - the one piece of per-draw state gl1-cube leans on that nothing here touched.
 * `GL_CULL_FACE` appeared exactly once in this file before, in reset_view(), being disabled.
 *
 * **Which winding is front is deliberately not asserted.** Facing is decided from the signed
 * area in window space, and window space here is Y-flipped relative to GL's, so writing down
 * which of the two triangles "should" survive would make this a test of my arithmetic. What the
 * specification fixes is the relationship, and that is what this checks: exactly one of an
 * opposite-wound pair survives, glFrontFace swaps which one, and GL_FRONT_AND_BACK removes
 * both. An implementation that ignores culling fails the first, one that ignores glFrontFace
 * fails the second, one that culls everything fails the control.
 */
static int check_cull_face(void) {
    /* Control first: with culling off both must draw, or the assertions below would pass for
     * the wrong reason. */
    reset_view();
    cull_pair();
    if (px(CULL_LEFT_X, CULL_Y) == PROBE_BG) return 0;
    if (px(CULL_RIGHT_X, CULL_Y) == PROBE_BG) return 0;

    reset_view();
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    cull_pair();
    const int left_ccw = px(CULL_LEFT_X, CULL_Y) != PROBE_BG;
    const int right_ccw = px(CULL_RIGHT_X, CULL_Y) != PROBE_BG;
    if (left_ccw == right_ccw) return 0; /* both survived, or neither */

    reset_view();
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);
    cull_pair();
    if ((px(CULL_LEFT_X, CULL_Y) != PROBE_BG) == left_ccw) return 0;
    if ((px(CULL_RIGHT_X, CULL_Y) != PROBE_BG) == right_ccw) return 0;

    reset_view();
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT_AND_BACK);
    cull_pair();
    if (px(CULL_LEFT_X, CULL_Y) != PROBE_BG) return 0;
    if (px(CULL_RIGHT_X, CULL_Y) != PROBE_BG) return 0;

    return glGetError() == GL_NO_ERROR;
}

/*
 * glViewport, which nothing here had ever called - every check drew into the whole window, so a
 * viewport applied as a scale but not an offset, or dropped entirely, looked identical.
 *
 * A quarter-size viewport in the middle: the full-NDC rectangle below has to land inside
 * x [32, 96) and y [24, 72) at this probe's 128x96, and nowhere else. The edge samples sit two
 * pixels either side of the left boundary, so getting the scale right but the offset wrong is a
 * failure rather than a near miss.
 */
static int check_viewport(void) {
    reset_view();
    glViewport(PROBE_W / 4, PROBE_H / 4, PROBE_W / 2, PROBE_H / 2);
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.6f, 1.0f);
    glViewport(0, 0, PROBE_W, PROBE_H);
    if (glGetError() != GL_NO_ERROR) return 0;

    if (px(PROBE_W / 2, PROBE_H / 2) == PROBE_BG) return 0;            /* centre: drawn */
    if (px(PROBE_W / 8, PROBE_H / 8) != PROBE_BG) return 0;            /* outside, top left */
    if (px(PROBE_W * 7 / 8, PROBE_H * 7 / 8) != PROBE_BG) return 0;    /* outside, bottom right */
    if (px(PROBE_W / 4 + 2, PROBE_H / 2) == PROBE_BG) return 0;        /* just inside the left edge */
    if (px(PROBE_W / 4 - 2, PROBE_H / 2) != PROBE_BG) return 0;        /* just outside it */
    return 1;
}

/*
 * glDepthMask: colour without depth.
 *
 * Under this projection - glOrtho(-1, 1, -1, 1, -1, 1) - eye z maps to window z as -z, so
 * z = +0.5 is the nearer surface and z = -0.5 the farther one.
 *
 * With writes on, the near quad records its depth and the far quad is rejected. With writes off
 * it never records anything, so the far quad passes against the cleared buffer and takes the
 * pixel. Both outcomes are asserted rather than only that the two differ, so a rasteriser that
 * had the mask inverted fails here too.
 */
static int check_depth_mask(void) {
    reset_view();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDepthMask(GL_TRUE);
    draw_quad_z(-0.6f, -0.6f, 0.6f, 0.6f,  0.5f, 1.0f, 0.0f, 0.0f); /* near, red */
    draw_quad_z(-0.6f, -0.6f, 0.6f, 0.6f, -0.5f, 0.0f, 0.0f, 1.0f); /* far, blue */
    const uint32_t writes_on = px(PROBE_W / 2, PROBE_H / 2);

    reset_view();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDepthMask(GL_FALSE);
    draw_quad_z(-0.6f, -0.6f, 0.6f, 0.6f,  0.5f, 1.0f, 0.0f, 0.0f);
    glDepthMask(GL_TRUE);
    draw_quad_z(-0.6f, -0.6f, 0.6f, 0.6f, -0.5f, 0.0f, 0.0f, 1.0f);
    const uint32_t writes_off = px(PROBE_W / 2, PROBE_H / 2);

    glDisable(GL_DEPTH_TEST);
    if (glGetError() != GL_NO_ERROR) return 0;

    if (!near_rgb(writes_on, 255, 0, 0, 8)) return 0;
    if (!near_rgb(writes_off, 0, 0, 255, 8)) return 0;
    return 1;
}

/*
 * Texture wrap modes, sampled rather than queried.
 *
 * `GL_TEXTURE_WRAP_S` was already set and read back through glGetTexParameteriv, which proves
 * the field round-trips and nothing about the sampler. This runs texture coordinates out to
 * s = 2 across the quad and asks where three of them land:
 *
 *   s = 0.25 and s = 0.75 are the two texel columns, in range under either mode.
 *   s = 1.25 is out of range. GL_REPEAT takes its fractional part, 0.25, and lands on the
 *   *first* column; GL_CLAMP_TO_EDGE holds it at the *last*.
 *
 * So the same pixel has to agree with a different in-range sample under each mode, which no
 * single wrap implementation can satisfy both ways round.
 */
static int check_texture_wrap(void) {
    static const GLubyte texels[16] = {
        255, 0, 0, 255,      0, 255, 0, 255,   /* row 0: red, green */
        0, 0, 255, 255,      255, 255, 0, 255, /* row 1: blue, yellow */
    };
    /* s = 0.25, 0.75 and 1.25 along a quad spanning NDC x [-0.8, 0.8] with s in [0, 2]. */
    const int x_lo = (int)(((-0.8f + 0.25f / 2.0f * 1.6f) + 1.0f) * 0.5f * (float)PROBE_W);
    const int x_hi = (int)(((-0.8f + 0.75f / 2.0f * 1.6f) + 1.0f) * 0.5f * (float)PROBE_W);
    const int x_out = (int)(((-0.8f + 1.25f / 2.0f * 1.6f) + 1.0f) * 0.5f * (float)PROBE_W);
    const int y = PROBE_H / 2;

    GLuint tex = 0;
    GLenum modes[2] = {GL_REPEAT, GL_CLAMP_TO_EDGE};
    uint32_t lo[2] = {0u, 0u}, hi[2] = {0u, 0u}, out[2] = {0u, 0u};

    for (int m = 0; m < 2; m++) {
        reset_view();
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)modes[m]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glEnable(GL_TEXTURE_2D);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glColor3f(1.0f, 1.0f, 1.0f);

        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.25f); glVertex3f(-0.8f, -0.5f, 0.0f);
        glTexCoord2f(2.0f, 0.25f); glVertex3f( 0.8f, -0.5f, 0.0f);
        glTexCoord2f(2.0f, 0.25f); glVertex3f( 0.8f,  0.5f, 0.0f);
        glTexCoord2f(0.0f, 0.25f); glVertex3f(-0.8f,  0.5f, 0.0f);
        glEnd();
        glDisable(GL_TEXTURE_2D);
        if (glGetError() != GL_NO_ERROR) { glDeleteTextures(1, &tex); return 0; }

        lo[m] = px(x_lo, y);
        hi[m] = px(x_hi, y);
        out[m] = px(x_out, y);
        glDeleteTextures(1, &tex); /* after the reads - see check_texture */
        if (lo[m] == PROBE_BG || hi[m] == PROBE_BG || out[m] == PROBE_BG) return 0;
        if (lo[m] == hi[m]) return 0; /* the two columns must be telling apart in the first place */
    }

    if (out[0] != lo[0]) return 0; /* GL_REPEAT wrapped to the first column */
    if (out[1] != hi[1]) return 0; /* GL_CLAMP_TO_EDGE held the last */
    return 1;
}

/*
 * glTexSubImage2D. The roadmap has listed it as done since it landed and nothing had drawn with
 * it - a sub-image that overwrote the whole level, or landed at the wrong offset, would have
 * been invisible here.
 *
 * A 4x4 of one colour with a 2x2 patch of another written into one quadrant: exactly one of the
 * four quadrants may change, and which one is not asserted for the same reason the texture
 * check does not assert orientation.
 */
static int check_tex_sub_image(void) {
    static GLubyte base[4 * 4 * 4];
    static GLubyte patch[2 * 2 * 4];
    for (int i = 0; i < 4 * 4; i++) {
        base[i * 4 + 0] = 40; base[i * 4 + 1] = 80; base[i * 4 + 2] = 200; base[i * 4 + 3] = 255;
    }
    for (int i = 0; i < 2 * 2; i++) {
        patch[i * 4 + 0] = 250; patch[i * 4 + 1] = 200; patch[i * 4 + 2] = 20; patch[i * 4 + 3] = 255;
    }

    uint32_t quad[2][4];
    for (int pass = 0; pass < 2; pass++) {
        GLuint tex = 0;
        reset_view();
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, base);
        if (pass == 1) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 2, 2, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, patch);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glEnable(GL_TEXTURE_2D);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glColor3f(1.0f, 1.0f, 1.0f);

        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.8f, -0.8f, 0.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex3f( 0.8f, -0.8f, 0.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex3f( 0.8f,  0.8f, 0.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.8f,  0.8f, 0.0f);
        glEnd();
        glDisable(GL_TEXTURE_2D);
        if (glGetError() != GL_NO_ERROR) { glDeleteTextures(1, &tex); return 0; }

        quad[pass][0] = px(PROBE_W * 5 / 16, PROBE_H * 5 / 16);
        quad[pass][1] = px(PROBE_W * 11 / 16, PROBE_H * 5 / 16);
        quad[pass][2] = px(PROBE_W * 5 / 16, PROBE_H * 11 / 16);
        quad[pass][3] = px(PROBE_W * 11 / 16, PROBE_H * 11 / 16);
        glDeleteTextures(1, &tex); /* after the reads - see check_texture */
    }

    /* Before the sub-image every quadrant is the base colour. */
    for (int q = 0; q < 4; q++) {
        if (quad[0][q] == PROBE_BG) return 0;
        if (quad[0][q] != quad[0][0]) return 0;
    }

    int changed = 0;
    for (int q = 0; q < 4; q++) {
        if (quad[1][q] != quad[0][q]) changed++;
    }
    return changed == 1;
}

/*
 * Two lights at once. GL_LIGHT1 appeared here only in a glLightfv/glGetLightfv round-trip, so
 * whether a second light contributes any light was untested - and GL_MAX_LIGHTS is reported as
 * eight a few checks further down.
 *
 * Both lights are kept dim and on separate channels so the answer cannot be hidden by
 * saturation: if the second light is ignored, green stays where one light left it.
 */
static int check_two_lights(void) {
    static const GLfloat pos0[4] = {0.0f, 0.0f, 1.0f, 0.0f}; /* w=0: directional */
    static const GLfloat pos1[4] = {0.0f, 0.0f, 1.0f, 0.0f};
    static const GLfloat red[4] = {0.35f, 0.0f, 0.0f, 1.0f};
    static const GLfloat green[4] = {0.0f, 0.35f, 0.0f, 1.0f};
    static const GLfloat black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    static const GLfloat white[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    uint32_t lit[2];
    for (int pass = 0; pass < 2; pass++) {
        reset_view();
        glEnable(GL_LIGHTING);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, black);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, white);

        glEnable(GL_LIGHT0);
        glLightfv(GL_LIGHT0, GL_POSITION, pos0);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, red);
        glLightfv(GL_LIGHT0, GL_AMBIENT, black);

        /* The second light is configured either way, so the two passes differ only in whether
         * it is enabled - not in what it was told. */
        glLightfv(GL_LIGHT1, GL_POSITION, pos1);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, green);
        glLightfv(GL_LIGHT1, GL_AMBIENT, black);
        if (pass == 1) glEnable(GL_LIGHT1); else glDisable(GL_LIGHT1);

        glNormal3f(0.0f, 0.0f, 1.0f);
        glBegin(GL_QUADS);
        glVertex3f(-0.6f, -0.6f, 0.0f); glVertex3f(0.6f, -0.6f, 0.0f);
        glVertex3f( 0.6f,  0.6f, 0.0f); glVertex3f(-0.6f, 0.6f, 0.0f);
        glEnd();

        glDisable(GL_LIGHTING);
        glDisable(GL_LIGHT0);
        glDisable(GL_LIGHT1);
        if (glGetError() != GL_NO_ERROR) return 0;
        lit[pass] = px(PROBE_W / 2, PROBE_H / 2);
    }

    /* One light: red, no green. Two: the green one adds to it without taking the red away. */
    if (chan_r(lit[0]) < 60) return 0;
    if (chan_g(lit[0]) > 20) return 0;
    if (chan_g(lit[1]) < 60) return 0;
    if (chan_r(lit[1]) < chan_r(lit[0]) - 8) return 0;
    return 1;
}

/* ------------------------------------------------------------------------- */

/*
 * **Deleting a texture that a built frame still references.**
 *
 * Legal GL: the *name* is freed at once, and the *storage* has to outlive the draws that name
 * it. On a deferred hardware path that is not automatic - `glDrawArrays` writes the texture's
 * address into a command buffer and returns, so freeing the memory before the submission hands
 * the sampler unmapped pages.
 *
 * This is not hypothetical and it is why the check is last. On 2026-09-17 the texture checks did
 * this incidentally, and the console answered with a GPU protection fault -
 * `client:TCP(8) access:Read`, `Unmapped page access`, 504 wavefronts with `XNACK_ERROR MEMVIOL`
 * - which reset the GPU, restarted the user interface, and cost every result after check nine.
 * oops-sdk now flushes before releasing texture storage. Asking the question once, at the end,
 * means a regression costs this one row rather than the whole run.
 */
static int check_tex_delete_in_frame(void) {
    static const GLubyte texels[16] = {
        255, 0, 0, 255,      0, 255, 0, 255,
        0, 0, 255, 255,      255, 255, 0, 255,
    };
    reset_view();
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor3f(1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.8f, -0.8f, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 0.8f, -0.8f, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 0.8f,  0.8f, 0.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.8f,  0.8f, 0.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    /* **Before anything is sampled**, which on the hardware path means before the frame has been
     * submitted. This is the whole point of the check. */
    glDeleteTextures(1, &tex);
    if (glGetError() != GL_NO_ERROR) return 0;

    /* The draw still has to have happened, and with the texture's own colours: an implementation
     * that quietly dropped the draw rather than faulting would leave the background. */
    uint32_t a = px(PROBE_W * 5 / 16, PROBE_H * 5 / 16);
    uint32_t b = px(PROBE_W * 11 / 16, PROBE_H * 11 / 16);
    if (a == PROBE_BG || b == PROBE_BG) return 0;
    if (a == b) return 0;
    return 1;
}

static int check_limits_reported(void) {
    reset_view();
    GLint v = 0;
    glGetIntegerv(GL_MAX_LIGHTS, &v);
    if (v < 8) return 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &v);
    if (v < 64) return 0;
    glGetIntegerv(GL_MAX_MODELVIEW_STACK_DEPTH, &v);
    if (v < 2) return 0;
    if (glGetError() != GL_NO_ERROR) return 0;

    const GLubyte *ver = glGetString(GL_VERSION);
    if (!ver || ver[0] < '1' || ver[0] > '9') return 0;
    return 1;
}

/* ------------------------------------------------------------------------- */

static const gl1_probe_case_t g_cases[] = {
    {"clear-and-rect",   check_clear_and_rect},
    {"scissor",          check_scissor},
    {"clip-plane",       check_clip_plane},
    {"texgen",           check_texgen},
    {"stencil",          check_stencil},
    {"raster-ops",       check_raster_ops},
    {"points-and-lines", check_points_and_lines},
    {"fog",              check_fog},
    {"colour-mask",      check_colour_mask},
    {"blend",            check_blend},
    {"depth-test",       check_depth},
    {"vertex-arrays",    check_vertex_arrays},
    {"buffer-objects",   check_buffer_objects},
    {"display-list",     check_display_list},
    {"texture-2d",       check_texture},
    {"read-pixels",      check_read_pixels},
    {"matrix-stack",     check_matrix_stack},
    {"attrib-stack",     check_attrib_stack},
    {"alpha-test",       check_alpha_test},
    {"lighting",         check_lighting},
    {"tex-env-modes",    check_tex_env_modes},
    {"copy-tex",         check_copy_tex},
    {"get-tex-image",    check_get_tex_image},
    {"array-paths",      check_array_paths},
    {"type-variants",    check_type_variants},
    {"client-attrib",    check_client_attrib},
    {"depth-range",      check_depth_range},
    {"object-queries",   check_object_queries},
    {"polygon-offset",   check_polygon_offset},
    {"blend-equation",   check_blend_equation},
    {"two-side-refused", check_two_side_refused},
    {"cull-face",        check_cull_face},
    {"viewport",         check_viewport},
    {"depth-mask",       check_depth_mask},
    {"texture-wrap",     check_texture_wrap},
    {"tex-sub-image",    check_tex_sub_image},
    {"two-lights",       check_two_lights},
    {"refusals",         check_refusals},
    {"limits",           check_limits_reported},
    /* **Last on purpose.** It is the one check that has taken the GPU down, so a regression in
     * it costs this row and nothing after it. See check_tex_delete_in_frame. */
    {"tex-delete-in-frame", check_tex_delete_in_frame},
};

int gl1_probe_case_count(void) {
    return (int)(sizeof(g_cases) / sizeof(g_cases[0]));
}

const char *gl1_probe_case_name(int i) {
    if (i < 0 || i >= gl1_probe_case_count()) return "?";
    return g_cases[i].name;
}

int gl1_probe_run(gl1_probe_result_t *out, int max) {
    g_disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, PROBE_DISPLAY_W, PROBE_DISPLAY_H);
    if (!g_disp) return -1;
    /* **A display that could not open is returned, not hidden** - `oops/display.h` says so, and
     * this did not ask. On hardware that meant a NULL framebuffer that looked like a working one
     * until the first check read it. gl1-cube has always checked; this now does too. */
    if (!oops_display_is_ready(g_disp)) {
        oops_display_close(g_disp);
        g_disp = (oops_display_t *)0;
        return -1;
    }
    void *ctx = glContextCreate(g_disp);
    if (!ctx) {
        oops_display_close(g_disp);
        g_disp = (oops_display_t *)0;
        return -1;
    }
    g_fb = oops_display_get_framebuffer(g_disp);
    g_fb_w = oops_display_get_width(g_disp);
    g_fb_h = oops_display_get_height(g_disp);
    if (!g_fb || g_fb_w < PROBE_W || g_fb_h < PROBE_H) {
        glContextDestroy(ctx);
        oops_display_close(g_disp);
        g_disp = (oops_display_t *)0;
        g_fb = (uint32_t *)0;
        return -1;
    }
    /* The probe's viewport sits at GL's origin, so its region is the bottom PROBE_H rows of the
     * framebuffer - the rasteriser flips y against the full height. */
    g_row0 = g_fb_h - PROBE_H;

    const int n = gl1_probe_case_count();
    for (int i = 0; i < n && i < max; i++) {
        /* Errors are cleared between checks so one failure cannot cascade into the next and
         * make a single bug look like a dozen. */
        (void)glGetError();
        out[i].name = g_cases[i].name;
        out[i].passed = g_cases[i].fn();
    }

    glContextDestroy(ctx);
    oops_display_close(g_disp);
    g_disp = (oops_display_t *)0;
    g_fb = (uint32_t *)0;
    return n < max ? n : max;
}
