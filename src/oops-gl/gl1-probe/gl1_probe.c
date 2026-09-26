/*
 * gl1-probe: does oops-gl actually do OpenGL 1.x?
 *
 * gl-cube answers a narrower question. It is a pinned *oracle* - one measured frame,
 * asserted register by register against a real console - and it is very good at
 * noticing that the path which drew has stopped drawing. What it does not do is
 * exercise breadth: twelve triangles, one texture, lighting, depth, culling, and
 * nothing else. Around a hundred entry points have been added since it was recorded and
 * not one of them is on its path.
 *
 * This is the other half. Each check drives one feature and then **reads the pixels
 * back and decides**, because a GL call that returns without error has proved nothing
 * at all - the whole failure mode this project keeps meeting is a call that succeeds
 * and draws the wrong thing.
 *
 * # How a check is written
 *
 * Every check clears to a known colour, draws, and samples specific pixels. A check
 * must be able to *fail*: where the obvious fixture would pass against a broken
 * implementation - a square symmetric about x=y, a colour with equal channels - the
 * fixture is deliberately skewed. That has caught two real bugs in the oops-gl unit
 * tests already, and the same discipline applies here where there is no mutation
 * testing to fall back on.
 *
 * The suite runs identically on the host software rasteriser and on the console, which
 * is the point: a result that differs between them is a hardware-path bug, and nothing
 * else in the repository can see one.
 */

#include "gl1_probe.h"

#include <GL/gl.h>
#include <GL/glu.h>
#include <oops/display.h>
#ifndef OOPS_HOST_BUILD
#include <oops/system.h> /* OOPS_LOG_DEBUG, which gl1_probe_run asks the GL layer for */
#endif

#ifdef OOPS_HOST_BUILD
#include <string.h>
#else
#include <oops/freestd.h>
#endif

#include "probe_px.h"

static uint32_t *g_fb;
static oops_display_t *g_disp;
/* Kept only when the caller asks, so it can paint the screen after the suite - see
   `gl1_probe_test_card`. Zero for the host self-test, which closes as it always did. */
static void *g_ctx;
int gl1_probe_keep_context;
static unsigned int g_fb_w; /* the display's real width, which is the row stride */
static unsigned int g_fb_h;
static unsigned int g_row0; /* the framebuffer row the probe's logical row 0 sits on */

static const uint32_t *frame(void) {
    return probe_frame(g_fb);
}

/* One pixel, synchronised. A check that blends decides with census_wrong instead: a
 * fault with a period the sample point is in step with is invisible to one pixel. */
static uint32_t px(int x, int y) {
    if (!g_fb || x < 0 || y < 0 || x >= PROBE_W || y >= PROBE_H)
        return 0u;
    const uint32_t *f = frame();
    if (!f)
        return 0u;
    return f[(size_t)(g_row0 + (unsigned int)y) * (size_t)g_fb_w + (size_t)x];
}

/* For checks that draw one picture two ways and require the results to match word for
 * word. frame() is NULL when the display never opened. */
static void frame_snapshot(uint32_t *out) {
    probe_snapshot(frame(), g_row0, g_fb_w, out);
}

static int frame_matches(const uint32_t *want) {
    const uint32_t *f = frame();
    if (!f)
        return 0;
    for (int y = 0; y < PROBE_H; y++) {
        const uint32_t *row = f + (size_t)(g_row0 + (unsigned int)y) * (size_t)g_fb_w;
        for (int x = 0; x < PROBE_W; x++) {
            if (row[x] != want[y * PROBE_W + x])
                return 0;
        }
    }
    return 1;
}

/* The probe's rectangle, synchronised once. A scan reads a snapshot: through px() every
 * pixel would be its own submission and fence wait. */
static uint32_t g_scan[PROBE_W * PROBE_H];

static const uint32_t *scan_frame(void) {
    for (int i = 0; i < PROBE_W * PROBE_H; i++)
        g_scan[i] = 0u;
    frame_snapshot(g_scan);
    return g_scan;
}

/* How many pixels of a rectangle miss the expected colour, from one synchronisation. */
static int census_wrong(int x0, int y0, int w, int h, int r, int g, int b, int tol) {
    const uint32_t *f = frame();
    if (!f)
        return w * h;
    int bad = 0;
    for (int y = y0; y < y0 + h; y++) {
        for (int x = x0; x < x0 + w; x++) {
            if (x < 0 || y < 0 || x >= PROBE_W || y >= PROBE_H)
                continue;
            const uint32_t c =
                f[(size_t)(g_row0 + (unsigned int)y) * (size_t)g_fb_w + (size_t)x];
            if (!near_rgb(c, r, g, b, tol))
                bad++;
        }
    }
    return bad;
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
    glDisable(GL_COLOR_LOGIC_OP);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glShadeModel(GL_SMOOTH);
    glLineWidth(1.0f);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    /* State the checks below drive and that nothing used to put back, so a check that
     * changed it would have silently changed the meaning of every check after it. */
    glViewport(0, 0, PROBE_W, PROBE_H);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glDepthMask(GL_TRUE);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glClearColor(0.125f, 0.125f, 0.125f, 1.0f); /* 0x20 per channel */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    (void)glGetError();
}

/* A filled rectangle in the given colour, through glRectf. Used as the "something drew"
 * probe by several checks. */
static void draw_rect(float x0, float y0, float x1, float y1, float r, float g,
                      float b) {
    glColor3f(r, g, b);
    glRectf(x0, y0, x1, y1);
}

/* The same, at a chosen eye-space z, for the checks that need two surfaces at different
 * depths. glRectf is flat at z = 0, which is exactly no use to them. */
static void draw_quad_z(float x0, float y0, float x1, float y1, float z, float r,
                        float g, float b) {
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
    /* **Not square and not centred**, so a transposed or mirrored rectangle fails
     * rather than landing on itself. */
    draw_rect(-0.75f, -0.25f, 0.25f, 0.75f, 1.0f, 0.0f, 0.0f);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    /* Inside the rectangle: red. The rectangle spans x in [-0.75, 0.25], y in [-0.25,
     * 0.75], so NDC (-0.25, 0.25) is inside and (0.6, -0.6) is outside. */
    uint32_t inside = px(PROBE_W * 3 / 8, PROBE_H * 3 / 8);
    uint32_t outside = px(PROBE_W * 7 / 8, PROBE_H * 7 / 8);
    if (!near_rgb(inside, 255, 0, 0, 8))
        return 0;
    if (outside != PROBE_BG)
        return 0;
    return 1;
}

static int check_scissor(void) {
    reset_view();
    glEnable(GL_SCISSOR_TEST);
    /* A box in the lower-left quarter, in window coordinates. */
    glScissor(0, 0, PROBE_W / 2, PROBE_H / 2);
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f);
    glDisable(GL_SCISSOR_TEST);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    /* GL's origin is bottom-left and the framebuffer's row 0 is the top, so the
     * scissored region is the *bottom* half in window terms - the lower rows of the
     * image. A probe that got the flip wrong would find the green in the wrong half,
     * which is the point of checking both. */
    uint32_t in_box = px(PROBE_W / 4, PROBE_H * 3 / 4);
    uint32_t out_box = px(PROBE_W * 3 / 4, PROBE_H / 4);
    if (!near_rgb(in_box, 0, 255, 0, 8))
        return 0;
    if (out_box != PROBE_BG)
        return 0;
    return 1;
}

/* User clip planes, on oops-gl's own stage.
 *
 * Worth having as a check rather than as a synthetic probe: obSCEne has measured clip
 * planes three times and every one of those runs reconstructed the stage by hand and
 * used the passthrough VGT_SHADER_STAGES_EN (0x02002000), which is not what oops-gl
 * programmes (0x00c12010). This draws through the same path everything else here does.
 *
 * The plane keeps x >= 0 in eye space, so the left half of the viewport goes and the
 * right half stays. Both halves are sampled: a clipper that discards everything and one
 * that discards nothing each pass a check that only looks at one side.
 */
static int check_clip_plane(void) {
    reset_view();
    static const GLdouble keep_positive_x[4] = {1.0, 0.0, 0.0, 0.0};
    glClipPlane(GL_CLIP_PLANE0, keep_positive_x);
    glEnable(GL_CLIP_PLANE0);
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);
    glDisable(GL_CLIP_PLANE0);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    const uint32_t cut = px(PROBE_W / 4, PROBE_H / 2);
    const uint32_t kept = px(PROBE_W * 3 / 4, PROBE_H / 2);
    if (cut != PROBE_BG)
        return 0;
    if (!near_rgb(kept, 255, 0, 255, 8))
        return 0;

    /* Disabled, the same draw covers both halves - so the cut above was the plane and
     * not some other reason the left side was empty. */
    reset_view();
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    if (!near_rgb(px(PROBE_W / 4, PROBE_H / 2), 255, 0, 255, 8))
        return 0;
    return 1;
}

/* Texture coordinate generation.
 *
 * Generation happens on the CPU at vertex assembly, so the coordinate reaches the
 * hardware in the vertex buffer like any other - this ought to pass on the console. The
 * check is here because "ought to" is what the last eight failures all were: a feature
 * implemented on the software path and never once run through the real one.
 *
 * GL_OBJECT_LINEAR with the plane (0.5, 0, 0, 0.5) maps object x in [-1, 1] onto s in
 * [0, 1], so the quad's left edge samples column 0 and its right edge column 1 of a
 * two-column texture.
 */
static int check_texgen(void) {
    reset_view();
    static const GLubyte texels[8] = {
        255, 0, 0, 255, 0, 0, 255, 255, /* one row: red then blue */
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
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-0.9f, -0.9f, 0.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(0.9f, -0.9f, 0.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(0.9f, 0.9f, 0.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-0.9f, 0.9f, 0.0f);
    glEnd();

    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_2D);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &tex);
        return 0;
    }

    const uint32_t left = px(PROBE_W / 4, PROBE_H / 2);
    const uint32_t right = px(PROBE_W * 3 / 4, PROBE_H / 2);
    glDeleteTextures(1, &tex);
    if (!near_rgb(left, 255, 0, 0, 8))
        return 0;
    if (!near_rgb(right, 0, 0, 255, 8))
        return 0;
    return 1;
}

/* Stencil.
 *
 * **This is the measurement of the console's stencil test** (written 2026-09-19, never
 * run). The hardware path binds a STENCIL_8 surface at the depth surface's 64KB_Z_X
 * swizzle and sets the stencil function, operations and reference per draw, from
 * radeonsi's programming - obSCEne could not measure it (REQ-20260917T1845Z-3d5b: its
 * fixture cannot run oops-gl's stage), so this check is where it is settled. It failed
 * on the console before, when there was no stencil surface and the masked draw below
 * was not masked; a pass now means the surface, the clear and the test are right
 * together.
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
    if (glGetError() != GL_NO_ERROR)
        return 0;

    const uint32_t inside = px(PROBE_W / 4, PROBE_H / 2);
    const uint32_t outside = px(PROBE_W * 3 / 4, PROBE_H / 2);
    if (!near_rgb(inside, 0, 255, 0, 8))
        return 0;
    /* The right half was never stamped, so the second draw must have been rejected
     * there and the first draw's grey must still be showing. */
    if (near_rgb(outside, 0, 255, 0, 8))
        return 0;
    return 1;
}

/* glDrawPixels and glBitmap at the raster position.
 *
 * These write the colour buffer from the CPU rather than going through the rasteriser,
 * so what this really checks on hardware is that the frame is flushed first - a pixel
 * rectangle written into a target the GPU is about to draw over would vanish, which is
 * the same class of bug the alpha test had.
 */
static int check_raster_ops(void) {
    reset_view();
    /* An 8x8 block rather than a 2x2, and counted rather than point-sampled.
     *
     * GL's window y runs up from the bottom and this probe's logical row 0 is the top
     * of its region, so the two conventions differ by a pixel at the exact centre - a
     * 2x2 block lands just beside where a centre sample looks. Counting is the honest
     * check anyway: what this asks is "did the rectangle reach the colour buffer", and
     * a point sample answers that only if the arithmetic on both sides already agrees.
     */
    static GLubyte block[8 * 8 * 4];
    for (int i = 0; i < 8 * 8; i++) {
        block[i * 4 + 0] = 255;
        block[i * 4 + 1] = 0;
        block[i * 4 + 2] = 255;
        block[i * 4 + 3] = 255;
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glRasterPos2f(0.0f, 0.0f);
    GLint valid = 0;
    glGetIntegerv(GL_CURRENT_RASTER_POSITION_VALID, &valid);
    if (!valid)
        return 0;
    glDrawPixels(8, 8, GL_RGBA, GL_UNSIGNED_BYTE, block);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    /* One snapshot for both counts - see `scan_frame`. */
    const uint32_t *s = scan_frame();
    int drawn = 0;
    for (int y = 0; y < PROBE_H; y++) {
        for (int x = 0; x < PROBE_W; x++) {
            if (near_rgb(SCAN_PX(s, x, y), 255, 0, 255, 8))
                drawn++;
        }
    }
    if (drawn < 60)
        return 0;

    /* And it landed near the middle, not in a corner - so "it drew" is not the whole
     * claim. */
    int near_middle = 0;
    for (int y = PROBE_H / 4; y < PROBE_H * 3 / 4; y++) {
        for (int x = PROBE_W / 4; x < PROBE_W * 3 / 4; x++) {
            if (near_rgb(SCAN_PX(s, x, y), 255, 0, 255, 8))
                near_middle++;
        }
    }
    if (near_middle < 60)
        return 0;

    /* A position that clipped draws nothing, which is the rule most easily got wrong.
     */
    reset_view();
    glRasterPos2f(5.0f, 5.0f);
    glGetIntegerv(GL_CURRENT_RASTER_POSITION_VALID, &valid);
    if (valid)
        return 0;
    glDrawPixels(8, 8, GL_RGBA, GL_UNSIGNED_BYTE, block);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    s = scan_frame();
    for (int y = 0; y < PROBE_H; y++) {
        for (int x = 0; x < PROBE_W; x++) {
            if (near_rgb(SCAN_PX(s, x, y), 255, 0, 255, 8))
                return 0;
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
    if (glGetError() != GL_NO_ERROR)
        return 0;

    uint32_t c = px(PROBE_W / 2, PROBE_H / 2);
    /* Red and blue written, green left at the clear value of 0x20. */
    if (chan_r(c) < 240 || chan_b(c) < 240)
        return 0;
    if (chan_g(c) > 0x40)
        return 0;
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
    if (glGetError() != GL_NO_ERROR)
        return 0;

    /* Half red, half blue. **Neither channel may be at an extreme** - if blending were
     * ignored the result is pure blue, and if the source alpha were ignored it is pure
     * blue too, so the test is that both channels are in the middle. */
    uint32_t c = px(PROBE_W / 2, PROBE_H / 2);
    if (chan_r(c) < 90 || chan_r(c) > 170)
        return 0;
    if (chan_b(c) < 90 || chan_b(c) > 170)
        return 0;
    return 1;
}

static int check_depth(void) {
    reset_view();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* **The camera looks down -z, so a larger eye z is nearer.** With
     * glOrtho(-1, 1, -1, 1, -1, 1) the mapping is window_z = (1 - eye_z) / 2, which
     * puts eye z = +0.5 at 0.25 (near) and eye z = -0.5 at 0.75 (far). Getting that
     * backwards is easy and gives a check that fails against a correct implementation,
     * which is worse than no check at all.
     *
     * Near first, then far over the top of it: with GL_LESS the far one must lose. That
     * is the case which fails if depth is written but never compared. */
    glColor3f(0.0f, 1.0f, 0.0f); /* near */
    glBegin(GL_QUADS);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glEnd();
    glColor3f(1.0f, 0.0f, 0.0f); /* far, and must not win */
    glBegin(GL_QUADS);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glEnd();
    glDisable(GL_DEPTH_TEST);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    uint32_t c = px(PROBE_W / 2, PROBE_H / 2);
    return near_rgb(c, 0, 255, 0, 8);
}

static int check_vertex_arrays(void) {
    reset_view();
    static const GLfloat verts[12] = {
        -0.6f, -0.6f, 0.0f, 0.6f, -0.6f, 0.0f, 0.6f, 0.6f, 0.0f, -0.6f, 0.6f, 0.0f,
    };
    static const GLfloat cols[16] = {
        1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
    };
    static const GLushort idx[6] = {0, 1, 2, 0, 2, 3};

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, verts);
    glColorPointer(4, GL_FLOAT, 0, cols);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, idx);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 255, 0, 0, 8);
}

/* GL 1.5 buffer objects, which is how nearly all code written since about 2003 draws.
 * The pointer is an *offset* here, and offset zero arrives as NULL - the case a naive
 * array reader mistakes for "no array bound". */
static int check_buffer_objects(void) {
    reset_view();
    static const GLfloat verts[12] = {
        -0.6f, -0.6f, 0.0f, 0.6f, -0.6f, 0.0f, 0.6f, 0.6f, 0.0f, -0.6f, 0.6f, 0.0f,
    };
    static const GLushort idx[6] = {0, 1, 2, 0, 2, 3};

    GLuint vbo = 0, ibo = 0;
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);
    if (vbo == 0u || ibo == 0u)
        return 0;

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
    if (glGetError() != GL_NO_ERROR)
        return 0;

    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 0, 0, 255, 8);
}

static int check_display_list(void) {
    reset_view();
    GLuint list = glGenLists(1);
    if (list == 0u)
        return 0;
    glNewList(list, GL_COMPILE);
    glColor3f(1.0f, 1.0f, 0.0f);
    glRectf(-0.4f, -0.4f, 0.4f, 0.4f);
    glEndList();
    if (glGetError() != GL_NO_ERROR)
        return 0;

    /* Nothing should have been drawn by compiling it. */
    if (px(PROBE_W / 2, PROBE_H / 2) != PROBE_BG)
        return 0;

    glCallList(list);
    glDeleteLists(list, 1);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 255, 255, 0, 8);
}

/*
 * **GL_LUMINANCE and GL_LUMINANCE_ALPHA, which nothing in this suite uploaded until
 * now.**
 *
 * Eighty-odd texture checks and every one of them uploaded RGBA or RGB. The gap is not
 * academic: of Neverball's 292 PNGs, 23 are greyscale and 58 are grey-plus-alpha, so 81
 * of its textures take a path this probe had never exercised - and it passed 83/85
 * while the title drew every surface untextured.
 *
 * The rule under test is the expansion (GL 1.1, table 3.11): a luminance texel samples
 * as (L, L, L, 1), a luminance-alpha texel as (L, L, L, A). So the first half asserts
 * the channels come back **equal** as well as distinct - an implementation that stored
 * L in red and left green and blue at zero would pass a "four different colours" test
 * and fail this one. The second half gives four texels one luminance and four different
 * alphas and blends them over the background, so nothing but the alpha can tell them
 * apart.
 */
static int check_texture_luminance(void) {
    reset_view();

    /* Four luminances, none at 0 or 255: both ends survive a channel being dropped. */
    static const GLubyte lum[4] = {40, 90, 160, 220};
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    /* **Alignment 1 matters here and did not for RGBA.** A 2-wide luminance row is two
     * bytes; the default unpack alignment of 4 would step the second row three bytes
     * late. */
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 2, 2, 0, GL_LUMINANCE,
                 GL_UNSIGNED_BYTE, lum);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor3f(1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-0.8f, -0.8f, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(0.8f, -0.8f, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(0.8f, 0.8f, 0.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-0.8f, 0.8f, 0.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &tex);
        return 0;
    }

    uint32_t q[4];
    q[0] = px(PROBE_W * 5 / 16, PROBE_H * 5 / 16);
    q[1] = px(PROBE_W * 11 / 16, PROBE_H * 5 / 16);
    q[2] = px(PROBE_W * 5 / 16, PROBE_H * 11 / 16);
    q[3] = px(PROBE_W * 11 / 16, PROBE_H * 11 / 16);
    glDeleteTextures(1, &tex);

    for (int i = 0; i < 4; i++) {
        if (q[i] == PROBE_BG)
            return 0;
        /* The replication itself: grey in, grey out. */
        if (chan_r(q[i]) != chan_g(q[i]) || chan_g(q[i]) != chan_b(q[i]))
            return 0;
    }
    if (q[0] == q[1] || q[0] == q[2] || q[0] == q[3] || q[1] == q[2] || q[1] == q[3] ||
        q[2] == q[3])
        return 0;

    /* --- luminance-alpha: one luminance, four alphas, separable only by the blend
     * ---------- */
    reset_view();
    static const GLubyte la[8] = {200, 30, 200, 100, 200, 170, 200, 240};
    GLuint tex2 = 0;
    glGenTextures(1, &tex2);
    glBindTexture(GL_TEXTURE_2D, tex2);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, 2, 2, 0, GL_LUMINANCE_ALPHA,
                 GL_UNSIGNED_BYTE, la);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor3f(1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-0.8f, -0.8f, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(0.8f, -0.8f, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(0.8f, 0.8f, 0.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-0.8f, 0.8f, 0.0f);
    glEnd();
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &tex2);
        return 0;
    }

    uint32_t r[4];
    r[0] = px(PROBE_W * 5 / 16, PROBE_H * 5 / 16);
    r[1] = px(PROBE_W * 11 / 16, PROBE_H * 5 / 16);
    r[2] = px(PROBE_W * 5 / 16, PROBE_H * 11 / 16);
    r[3] = px(PROBE_W * 11 / 16, PROBE_H * 11 / 16);
    glDeleteTextures(1, &tex2);

    for (int i = 0; i < 4; i++) {
        if (r[i] == PROBE_BG)
            return 0; /* even alpha 30 moves it off the clear */
        if (chan_r(r[i]) != chan_g(r[i]) || chan_g(r[i]) != chan_b(r[i]))
            return 0;
    }
    /* One luminance for all four, so anything that separates them came from the alpha.
     * An implementation that expanded luminance-alpha as (L, L, L, 1) draws four
     * identical quadrants and fails here. */
    if (r[0] == r[1] || r[0] == r[2] || r[0] == r[3] || r[1] == r[2] || r[1] == r[3] ||
        r[2] == r[3])
        return 0;
    return 1;
}

static int check_texture(void) {
    reset_view();
    /* A 2x2 texture with four *different* colours, so a transposed or flipped sample
     * lands on a different one. A checkerboard of two colours would not. */
    static const GLubyte texels[16] = {
        255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255,
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
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-0.8f, -0.8f, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(0.8f, -0.8f, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(0.8f, 0.8f, 0.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-0.8f, 0.8f, 0.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &tex);
        return 0;
    }

    /* Four quadrants, four colours. Which corner holds which depends on the
     * orientation, so this asserts only that **all four are present and distinct** - a
     * sampler returning one colour everywhere, or two, fails. Orientation is gl-cube's
     * job, with a measured frame to compare against. */
    uint32_t a = px(PROBE_W * 5 / 16, PROBE_H * 5 / 16);
    uint32_t b = px(PROBE_W * 11 / 16, PROBE_H * 5 / 16);
    uint32_t c = px(PROBE_W * 5 / 16, PROBE_H * 11 / 16);
    uint32_t d = px(PROBE_W * 11 / 16, PROBE_H * 11 / 16);
    /* Deleted *after* the pixels are read. Deleting first is legal GL and is what this
     * used to do, and it is a separate question with a check of its own at the end of
     * the suite - so it is asked once, deliberately, where a failure costs one result
     * instead of every result after it. See `tex-delete-in-frame`. */
    glDeleteTextures(1, &tex);
    if (a == b || a == c || a == d || b == c || b == d || c == d)
        return 0;
    if (a == PROBE_BG || b == PROBE_BG || c == PROBE_BG || d == PROBE_BG)
        return 0;
    return 1;
}

static int check_read_pixels(void) {
    reset_view();
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);

    static GLubyte row[PROBE_W * 4];
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, PROBE_W, 1, GL_RGBA, GL_UNSIGNED_BYTE, row);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    /* Magenta: red and blue high, green low. A channel-order mistake shows here because
     * the three differ. */
    if (row[0] < 240 || row[2] < 240)
        return 0;
    if (row[1] > 16)
        return 0;
    return 1;
}

static int check_matrix_stack(void) {
    reset_view();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(0.5f, 0.0f, 0.0f);
    draw_rect(-0.2f, -0.2f, 0.2f, 0.2f, 0.0f, 1.0f, 1.0f);
    glPopMatrix();
    if (glGetError() != GL_NO_ERROR)
        return 0;

    /* Translated right by 0.5 in NDC: present at x = +0.5, absent at the origin. If the
     * pop failed to restore, the *next* check would drift - so this also draws at the
     * origin after the pop and requires it to land there. */
    uint32_t moved = px(PROBE_W * 3 / 4, PROBE_H / 2);
    uint32_t origin_before = px(PROBE_W / 2, PROBE_H / 2);
    if (!near_rgb(moved, 0, 255, 255, 8))
        return 0;
    if (origin_before != PROBE_BG)
        return 0;

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
    if (glGetError() != GL_NO_ERROR)
        return 0;

    /* Blending must be off again. Drawing the same rectangle twice additively would
     * saturate; with the pop honoured it simply replaces, so the result is the plain
     * colour. */
    draw_rect(-0.5f, -0.5f, 0.5f, 0.5f, 0.25f, 0.0f, 0.0f);
    draw_rect(-0.5f, -0.5f, 0.5f, 0.5f, 0.25f, 0.0f, 0.0f);
    uint32_t c = px(PROBE_W / 2, PROBE_H / 2);
    if (chan_r(c) > 90)
        return 0; /* saturated or doubled means the pop did not take */
    if (chan_r(c) < 40)
        return 0;
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
    if (glGetError() != GL_NO_ERROR)
        return 0;

    /* **Sampled well inside each rectangle, not on its edge.** The left one spans x
     * pixels 32..64 and the right one 64..96, so PROBE_W/4 and PROBE_W*3/4 land exactly
     * on the boundaries where a fill-rule difference decides the result. */
    uint32_t rejected = px(PROBE_W * 3 / 8, PROBE_H / 2);
    uint32_t kept = px(PROBE_W * 5 / 8, PROBE_H / 2);
    if (rejected != PROBE_BG)
        return 0;
    if (!near_rgb(kept, 0, 255, 0, 8))
        return 0;
    return 1;
}

/* **A fragment the alpha test rejected must not have written depth either.**
 *
 * `check_alpha_test` above draws with no depth test, so it only ever asks whether the
 * colour was written - and the colour is masked by the shader's own `exec`, which is
 * the half of this that was always right. The depth block is the other half, and it
 * only hears about a kill through `DB_SHADER_CONTROL.KILL_ENABLE`. Told nothing, it
 * runs early Z: it tests, writes and retires the pixel before the shader has run,
 * because it has been told the shader cannot change the answer. The rejected fragment
 * leaves a depth behind it and the surface further away never appears.
 *
 * The same bit, and the same mistake, as `gl2-probe`'s `discard` - found there on
 * 2026-09-22 because GL 2.0 has a check that puts a kill and a depth test in one draw
 * and GL 1.x did not.
 */
static int check_alpha_test_frees_depth(void) {
    reset_view();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);

    /* **Nearer is eye z = +0.5, not -0.5.** `reset_view`'s `glOrtho(..., -1, 1)`
     * negates z on the way to NDC, so the quad drawn at +0.5 lands at window depth 0.25
     * and the one at -0.5 at 0.75. Getting that backwards puts the second quad in front
     * of the first and it covers both halves - which is a failure that looks like the
     * bug this is chasing.
     *
     * Near, at z = +0.5: the left half fails the test, the right half passes and is
     * blue. */
    glColor4f(1.0f, 0.0f, 0.0f, 0.25f);
    glBegin(GL_QUADS);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(0.0f, -0.5f, 0.5f);
    glVertex3f(0.0f, 0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glEnd();
    glColor4f(0.0f, 0.0f, 1.0f, 0.75f);
    glBegin(GL_QUADS);
    glVertex3f(0.0f, -0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(0.0f, 0.5f, 0.5f);
    glEnd();
    glDisable(GL_ALPHA_TEST);

    /* Far, at z = -0.5, across both halves. It can only appear where no depth was
     * written. */
    glColor4f(1.0f, 1.0f, 0.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glEnd();
    glDisable(GL_DEPTH_TEST);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    const uint32_t freed = px(PROBE_W * 3 / 8, PROBE_H / 2);
    const uint32_t held = px(PROBE_W * 5 / 8, PROBE_H / 2);
    /* Where the test rejected, the depth stayed free and the far quad came through. */
    if (!near_rgb(freed, 255, 255, 0, 8))
        return 0;
    /* Where it kept, the near quad's depth is in the way and it is still blue. */
    if (!near_rgb(held, 0, 0, 255, 8))
        return 0;
    return 1;
}

/* Points and lines, which reach the hardware as triangles.
 *
 * **This is the check that tests the claim.** obSCEne measured that a one- or
 * two-vertex primitive stalls the pipe - `fence-hit 0` across five sweeps, one of them
 * on oops-gl's own `VGT_SHADER_STAGES_EN`. The conclusion drawn from that was "points
 * and lines need a stage this does not build". The narrower reading is that the
 * *native* primitive is shut, and that a line drawn as a quad of two triangles asks the
 * hardware for nothing it has not already retired.
 *
 * If the narrower reading is right this passes on the console. If it is wrong - if
 * something about the expansion or the inverse-matrix round trip does not survive the
 * real pipeline - this fails, and it fails here rather than in a title.
 */
static int check_points_and_lines(void) {
    reset_view();
    glLineWidth(4.0f);
    glColor3f(0.0f, 1.0f, 1.0f);
    glBegin(GL_LINES);
    glVertex3f(-0.8f, 0.0f, 0.0f);
    glVertex3f(0.8f, 0.0f, 0.0f);
    glEnd();
    if (glGetError() != GL_NO_ERROR)
        return 0;

    /* The line crosses the middle, so the centre row is lit and the corners are not. */
    if (!near_rgb(px(PROBE_W / 2, PROBE_H / 2), 0, 255, 255, 8))
        return 0;
    if (px(4, 4) != PROBE_BG)
        return 0;

    /* A point of a decent size lands where it was put. */
    reset_view();
    glPointSize(8.0f);
    glColor3f(1.0f, 1.0f, 0.0f);
    glBegin(GL_POINTS);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glEnd();
    if (glGetError() != GL_NO_ERROR)
        return 0;
    if (!near_rgb(px(PROBE_W / 2, PROBE_H / 2), 255, 255, 0, 8))
        return 0;
    if (px(4, 4) != PROBE_BG)
        return 0;

    /* A loop closes: three vertices give three segments, so the third edge is lit where
     * a strip would have left the background. Sampled near the midpoint of the closing
     * edge. */
    reset_view();
    glLineWidth(3.0f);
    glColor3f(1.0f, 0.0f, 1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(-0.7f, -0.7f, 0.0f);
    glVertex3f(0.7f, -0.7f, 0.0f);
    glVertex3f(0.0f, 0.7f, 0.0f);
    glEnd();
    if (glGetError() != GL_NO_ERROR)
        return 0;
    {
        /* The closing edge runs from (0, 0.7) back to (-0.7, -0.7); its midpoint is
         * about
         * (-0.35, 0) in NDC, which is a quarter of the way in from the left at mid
         * height. */
        const uint32_t on_closing_edge = px(PROBE_W * 5 / 16, PROBE_H / 2);
        if (!near_rgb(on_closing_edge, 255, 0, 255, 24))
            return 0;
    }
    glLineWidth(1.0f);
    glPointSize(1.0f);
    return 1;
}

/* Fog.
 *
 * **The console's first fog** (written 2026-09-19, unmeasured): a fog factor computed
 * per vertex on the CPU rides in the texture coordinate's spare z, which the vertex
 * shader already exports, and both pixel shaders blend towards the fog colour by it -
 * no third parameter export needed. This check is fully fogged, a factor of 0, so it
 * proves the blend's direction; `fog-coord`'s half-fogged draw proves the factor is the
 * one interpolated.
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
    if (glGetError() != GL_NO_ERROR)
        return 0;

    if (!near_rgb(px(PROBE_W / 2, PROBE_H / 2), 0, 0, 255, 8))
        return 0;
    return 1;
}

/* GL 1.4's fog coordinate: the same red quad at the eye, where fog by distance leaves
 * it red, fogged fully blue by a glFogCoord of 10 under GL_FOG_COORD_SRC = GL_FOG_COORD
 * - and then half fogged, purple, by a coordinate of 5 on a second quad beside it. The
 * half is what tells the console's fog apart from a pixel shader reading the wrong
 * component: a factor of 0 is also what the texture coordinate's unused w holds, so
 * full fog alone would pass either way. */
static int check_fog_coord(void) {
    reset_view();
    static const GLfloat blue[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    glFogfv(GL_FOG_COLOR, blue);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 0.0f);
    glFogf(GL_FOG_END, 10.0f);
    glFogi(GL_FOG_COORD_SRC, GL_FOG_COORD);
    glFogCoordf(10.0f);
    glEnable(GL_FOG);
    draw_rect(-0.5f, -0.5f, 0.0f, 0.5f, 1.0f, 0.0f, 0.0f);
    glFogCoordf(5.0f);
    draw_rect(0.0f, -0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f);
    glDisable(GL_FOG);
    glFogi(GL_FOG_COORD_SRC, GL_FRAGMENT_DEPTH);
    glFogCoordf(0.0f);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    return near_rgb(px(PROBE_W / 2 - PROBE_W / 8, PROBE_H / 2), 0, 0, 255, 8) &&
           near_rgb(px(PROBE_W / 2 + PROBE_W / 8, PROBE_H / 2), 128, 0, 128, 8);
}

/* glLogicOp through CB_COLOR_CONTROL's ROP3.
 *
 * Exact byte values, because a logic op is bitwise and a result off by one is a wrong
 * answer, not a rounding one. Two opcodes: GL_XOR is the same four-bit table in GL's
 * order and the hardware's, so it cannot catch the mapping being read backwards -
 * GL_AND_REVERSE (s & ~d) can, because read backwards it is GL_AND_INVERTED (~s & d)
 * and gives different bytes.
 *
 * Clear 0x3c 0x3c 0x55, draw 0xf0 0x0f 0xaa. The channel order in the stored word does
 * not matter to a per-channel bitwise op, so this is the one colour check with no
 * swizzle question.
 */
static int check_logic_op(void) {
    reset_view();
    glClearColor(60.0f / 255.0f, 60.0f / 255.0f, 85.0f / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_COLOR_LOGIC_OP);
    glLogicOp(GL_XOR);
    glColor4ub(0xf0, 0x0f, 0xaa, 0xff);
    glRectf(-0.5f, -0.5f, 0.5f, 0.5f);
    glDisable(GL_COLOR_LOGIC_OP);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    const uint32_t x = px(PROBE_W / 2, PROBE_H / 2);
    if (chan_r(x) != 0xcc || chan_g(x) != 0x33 || chan_b(x) != 0xff)
        return 0;

    glClearColor(60.0f / 255.0f, 60.0f / 255.0f, 85.0f / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_COLOR_LOGIC_OP);
    glLogicOp(GL_AND_REVERSE);
    glColor4ub(0xf0, 0x0f, 0xaa, 0xff);
    glRectf(-0.5f, -0.5f, 0.5f, 0.5f);
    glDisable(GL_COLOR_LOGIC_OP);
    glLogicOp(GL_COPY);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    const uint32_t a = px(PROBE_W / 2, PROBE_H / 2);
    return chan_r(a) == 0xc0 && chan_g(a) == 0x03 && chan_b(a) == 0xaa;
}

/*
 * **Which of CB_BLEND_RED..ALPHA did each channel read?** Three draws whose only job is
 * to make the answer a value rather than an argument.
 *
 * `blend-constant` fails on hardware and the check's own colours cannot say why: it
 * sets the constant to (0.5, 1, 0, 0.25), so its green of 1.0 and its alpha of 0.25 are
 * the only two channels that differ, and there is no third value to tell "green read
 * alpha" from "green read something that happens to be 0.25". These three give every
 * channel a value no other channel has, and then move one of them.
 *
 *   colour   constant (0.25, 0.5, 0.75, 1.0), GL_CONSTANT_COLOR - the pixel IS the
 * constant correct          0xff4080c0 green reads A    0xff40ffc0 green reads B
 * 0xff40c0c0 green reads R    0xff4040c0 no-a     the same, with alpha moved to 0.0 and
 * nothing else touched correct          0x004080c0 green reads A    0x004000c0    -
 * green follows alpha, so it is the register c-alpha  constant (0.25, 1.0, 0.75, 0.5),
 * GL_CONSTANT_ALPHA - every channel 0.5 correct          0x80808080 green reads G
 * 0x8080ff80    - the factor selection, not the register rewrite  `colour` again, after
 * another constant has already been written in that frame correct          0xff4080c0
 *            same as colour                 - the packet lands, wherever it sits
 *            right where colour is wrong    - the frame's first write is being lost
 *
 * Run only on the payload, where `gl1_probe_saw` is set: the host self-test prints its
 * own table and these would be three synchronisations for nothing. Run **before** the
 * check's own stages so the failure row after the verdict still means what it has
 * always meant.
 *
 * This lives here rather than on the obSCEne bus because oops-gl is the better fixture.
 * A blended write is a read-modify-write of the render target, and
 * `REQ-20260920T2320Z-4b8d`'s standalone Onion buffer could not take one - all three of
 * its arms returned `0xffffffff` with BLEND_BYPASS set, which measures the bypass and
 * not the question. This probe draws into the display's own scanout buffer and blends
 * there every frame already.
 */
static void blend_constant_diagnose(void) {
    if (!gl1_probe_saw)
        return;

    reset_view();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendColor(0.25f, 0.5f, 0.75f, 1.0f);
    glBlendFunc(GL_CONSTANT_COLOR, GL_ZERO);
    draw_rect(-0.5f, -0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_BLEND);
    gl1_probe_saw("blend-constant/colour", px(PROBE_W / 2, PROBE_H / 2));

    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendColor(0.25f, 0.5f, 0.75f, 0.0f);
    glBlendFunc(GL_CONSTANT_COLOR, GL_ZERO);
    draw_rect(-0.5f, -0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_BLEND);
    gl1_probe_saw("blend-constant/no-a", px(PROBE_W / 2, PROBE_H / 2));

    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendColor(0.25f, 1.0f, 0.75f, 0.5f);
    glBlendFunc(GL_CONSTANT_ALPHA, GL_ZERO);
    draw_rect(-0.5f, -0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_BLEND);
    gl1_probe_saw("blend-constant/c-alpha", px(PROBE_W / 2, PROBE_H / 2));

    /* `colour` again, but with a constant already written **in the same frame** - a
     * throwaway draw under a different one, and no pixel read between them, so both
     * writes are in one submission. Each of the three above is the first
     * CB_BLEND_RED..ALPHA write of its own frame, because reading a pixel submits one.
     * If this row is right where `colour` is wrong, the first write after a frame opens
     * is the thing being lost, not the register. */
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendColor(1.0f, 0.0f, 1.0f, 1.0f);
    glBlendFunc(GL_CONSTANT_COLOR, GL_ZERO);
    draw_rect(-0.9f, -0.9f, -0.8f, -0.8f, 1.0f, 1.0f, 1.0f);
    glBlendColor(0.25f, 0.5f, 0.75f, 1.0f);
    draw_rect(-0.5f, -0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_BLEND);
    gl1_probe_saw("blend-constant/rewrite", px(PROBE_W / 2, PROBE_H / 2));

    glBlendColor(0.0f, 0.0f, 0.0f, 0.0f);
    glBlendFunc(GL_ONE, GL_ZERO);
    (void)glGetError();
}

/* glBlendColor and the constant factors, through CB_BLEND_RED..ALPHA. White scaled per
 * channel by the constant (0.5, 1, 0), then by the constant's alpha 0.25 alone. */
static int check_blend_constant(void) {
    blend_constant_diagnose();
    reset_view();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendColor(0.5f, 1.0f, 0.0f, 0.25f);
    glBlendFunc(GL_CONSTANT_COLOR, GL_ZERO);
    draw_rect(-0.5f, -0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_BLEND);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    if (!near_rgb(px(PROBE_W / 2, PROBE_H / 2), 128, 255, 0, 8))
        return 0;

    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_CONSTANT_ALPHA, GL_ZERO);
    draw_rect(-0.5f, -0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_BLEND);
    glBlendColor(0.0f, 0.0f, 0.0f, 0.0f);
    glBlendFunc(GL_ONE, GL_ZERO);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 64, 64, 64, 8);
}

/* glPolygonMode, and the three things under it that reach the hardware's
 * PA_SU_SC_MODE_CNTL.
 *
 * - An outlined quad: its sides lit, its centre not - so neither filled nor crossed by
 * the diagonal it is triangulated along. Counted, because an outline is thin.
 * - A GL_LINES line with GL_CULL_FACE set to GL_FRONT_AND_BACK: still drawn. The cull
 * bits in that register used to go out for the quads a line becomes, so the hardware
 * culled them.
 * - A flat quad whose fourth vertex is blue and the rest red: no red at all. The colour
 * used to come from each triangle's last vertex, which for the first triangle is the
 * third.
 */
static int check_polygon_mode(void) {
    reset_view();
    glLineWidth(3.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    draw_rect(-0.5f, -0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    if (!near_rgb(px(PROBE_W / 2, PROBE_H / 2), 0x20, 0x20, 0x20, 8))
        return 0;
    int white = 0;
    const uint32_t *s = scan_frame();
    for (int y = 0; y < PROBE_H; y++) {
        for (int x = 0; x < PROBE_W; x++) {
            if (near_rgb(SCAN_PX(s, x, y), 255, 255, 255, 16))
                white++;
        }
    }
    /* The quad is 64x48 pixels: filled it would be ~3000; its outline three pixels wide
     * ~700. */
    if (white < 300 || white > 1500)
        return 0;

    reset_view();
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT_AND_BACK);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_LINES);
    glVertex2f(-0.5f, 0.0f);
    glVertex2f(0.5f, 0.0f);
    glEnd();
    glDisable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    /* The line lies exactly between two rows of pixel centres, so which row it lands on
     * is the rasteriser's tie rule - one row since oops-gl's has one (2026-09-19), and
     * whichever the GPU's picks on the console. Either row will do; that it drew at all
     * is the check. */
    if (!near_rgb(px(PROBE_W / 2, PROBE_H / 2), 255, 255, 255, 16) &&
        !near_rgb(px(PROBE_W / 2, PROBE_H / 2 - 1), 255, 255, 255, 16)) {
        return 0;
    }

    reset_view();
    glShadeModel(GL_FLAT);
    glBegin(GL_QUADS);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex2f(-0.5f, -0.5f);
    glVertex2f(0.5f, -0.5f);
    glVertex2f(0.5f, 0.5f);
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex2f(-0.5f, 0.5f);
    glEnd();
    glShadeModel(GL_SMOOTH);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    int red = 0, blue = 0;
    s = scan_frame(); /* a fresh snapshot: more was drawn since the count above */
    for (int y = 0; y < PROBE_H; y++) {
        for (int x = 0; x < PROBE_W; x++) {
            const uint32_t c = SCAN_PX(s, x, y);
            if (near_rgb(c, 255, 0, 0, 16))
                red++;
            else if (near_rgb(c, 0, 0, 255, 16))
                blue++;
        }
    }
    return red == 0 && blue > 1000;
}

/* Mip levels and completeness, on the path the hardware takes.
 *
 * A red 2x2 base and then a level-1 image: the base must stay red, which it did not
 * while `level` was ignored - the level-1 upload landed on the base image. Then
 * completeness: with a mipmapping filter and a level 1 of the wrong size the texture is
 * incomplete, and GL draws the quad untextured - the vertex colour, white - which on
 * the console means the untextured pixel shader, chosen per draw. Last, mip selection
 * on the hardware through the chain - see the third part below.
 */
static int check_mipmap_levels(void) {
    reset_view();
    static const GLubyte red4[16] = {255, 0, 0, 255, 255, 0, 0, 255,
                                     255, 0, 0, 255, 255, 0, 0, 255};
    static const GLubyte green1[4] = {0, 255, 0, 255};
    static const GLubyte green4[16] = {0, 255, 0, 255, 0, 255, 0, 255,
                                       0, 255, 0, 255, 0, 255, 0, 255};
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, red4);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, green1);
    glEnable(GL_TEXTURE_2D);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-0.5f, -0.5f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(0.5f, -0.5f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(0.5f, 0.5f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-0.5f, 0.5f);
    glEnd();
    if (glGetError() != GL_NO_ERROR) {
        glDisable(GL_TEXTURE_2D);
        glDeleteTextures(1, &t);
        return 0;
    }
    const int kept = near_rgb(px(PROBE_W / 2, PROBE_H / 2), 255, 0, 0, 16);

    /* Incomplete: a mipmapping filter, and a level 1 the size of the base. */
    reset_view();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, green4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-0.5f, -0.5f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(0.5f, -0.5f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(0.5f, 0.5f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-0.5f, 0.5f);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    const int untextured = near_rgb(px(PROBE_W / 2, PROBE_H / 2), 255, 255, 255, 16);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &t);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    /* **Mip selection on the hardware**, through the chain gl_tex_hw_prepare builds: an
     * 8x8 texture whose levels are red, green, blue and yellow, drawn across 4x4 pixels
     * - two texels a pixel, level of detail 1 - must come out green. The chain's layout
     * is addrlib's arithmetic, not a measurement; this is the measurement. Red means
     * the sampler found no levels (the chain is not being used); blue or yellow that it
     * is finding the wrong ones. */
    reset_view();
    static GLubyte lvl[8 * 8 * 4];
    static const GLubyte colours[4][4] = {
        {255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}, {255, 255, 0, 255}};
    GLuint m = 0;
    glGenTextures(1, &m);
    glBindTexture(GL_TEXTURE_2D, m);
    for (int level = 0; level < 4; level++) {
        const int side = 8 >> level;
        for (int i = 0; i < side * side; i++) {
            for (int k = 0; k < 4; k++)
                lvl[i * 4 + k] = colours[level][k];
        }
        glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, side, side, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, lvl);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEnable(GL_TEXTURE_2D);
    glColor3f(1.0f, 1.0f, 1.0f);
    /* 4 pixels each way: the probe area is 128x96 across NDC -1..1. */
    const float hx = 2.0f / (float)(PROBE_W / 2), hy = 2.0f / (float)(PROBE_H / 2);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-hx, -hy);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(hx, -hy);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(hx, hy);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-hx, hy);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &m);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    const int level1 = near_rgb(px(PROBE_W / 2, PROBE_H / 2), 0, 255, 0, 16);
    return kept && untextured && level1;
}

/* Evaluators: a flat Bezier patch over the middle of the area, drawn with glEvalMesh2.
 *
 * Evaluation is CPU arithmetic that ends in ordinary glVertex calls, so what this
 * measures is that those vertices reach the hardware like any others - and the two
 * things evaluation adds on the way. A colour map running red to blue across u colours
 * each vertex (the left of the patch red, the right blue) and leaves the current colour
 * white. GL_AUTO_NORMAL lights the patch with its own normal, +z towards the default
 * light, although the current normal points away.
 */
static int check_evaluators(void) {
    reset_view();
    static const GLfloat patch[12] = {-0.5f, -0.5f, 0.0f, -0.5f, 0.5f, 0.0f,
                                      0.5f,  -0.5f, 0.0f, 0.5f,  0.5f, 0.0f};
    static const GLfloat red_blue[8] = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f};
    glMap2f(GL_MAP2_VERTEX_3, 0.0f, 1.0f, 6, 2, 0.0f, 1.0f, 3, 2, patch);
    glMap2f(GL_MAP2_COLOR_4, 0.0f, 1.0f, 4, 2, 0.0f, 1.0f, 4, 1, red_blue);
    glEnable(GL_MAP2_VERTEX_3);
    glEnable(GL_MAP2_COLOR_4);
    glMapGrid2f(4, 0.0f, 1.0f, 4, 0.0f, 1.0f);
    glEvalMesh2(GL_FILL, 0, 4, 0, 4);
    glDisable(GL_MAP2_COLOR_4);
    GLfloat cur[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glGetFloatv(GL_CURRENT_COLOR, cur);
    if (glGetError() != GL_NO_ERROR) {
        glDisable(GL_MAP2_VERTEX_3);
        return 0;
    }
    /* A quarter of the patch in from each side: u = 1/8 and 7/8. */
    const uint32_t left = px(PROBE_W / 2 - PROBE_W / 8 - PROBE_W / 16, PROBE_H / 2);
    const uint32_t right = px(PROBE_W / 2 + PROBE_W / 8 + PROBE_W / 16, PROBE_H / 2);
    const int coloured = chan_r(left) > 180 && chan_b(left) < 70 &&
                         chan_r(right) < 70 && chan_b(right) > 180;
    const int current_kept = cur[0] == 1.0f && cur[1] == 1.0f && cur[2] == 1.0f;

    reset_view();
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glNormal3f(0.0f, 0.0f, -1.0f);
    glEnable(GL_AUTO_NORMAL);
    glEvalMesh2(GL_FILL, 0, 4, 0, 4);
    glDisable(GL_AUTO_NORMAL);
    glDisable(GL_LIGHT0);
    glDisable(GL_LIGHTING);
    glDisable(GL_MAP2_VERTEX_3);
    glNormal3f(0.0f, 0.0f, 1.0f);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    /* Ambient 0.2 x 0.2 plus diffuse 0.8 facing the light: 0.84, about 214. Facing away
     * it would be the ambient alone, about 10. */
    const int lit = near_rgb(px(PROBE_W / 2, PROBE_H / 2), 214, 214, 214, 16);
    return coloured && current_kept && lit;
}

/* GL_SELECT, the way a program picks: a pick matrix around one pixel, the scene drawn
 * again with a name per object, and the hits read back. Selection is CPU work that
 * reports rather than draws - what this measures on a console is that **a pick pass
 * leaves the frame alone**: its glClear and its quads must not reach the hardware, so
 * the red drawn before it is still there.
 */
static int check_selection(void) {
    reset_view();
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
    GLuint hits[32];
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    glSelectBuffer(32, hits);
    glRenderMode(GL_SELECT);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    /* A pixel in the lower left quarter of the area. */
    gluPickMatrix((GLdouble)(vp[0] + vp[2] / 4), (GLdouble)(vp[1] + vp[3] / 4), 2.0,
                  2.0, vp);
    glMatrixMode(GL_MODELVIEW);
    glInitNames();
    glPushName(0);
    glLoadName(1);
    draw_rect(-1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    glLoadName(2);
    draw_rect(0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f);
    const GLint n = glRenderMode(GL_RENDER);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    const int picked = n == 1 && hits[0] == 1u && hits[3] == 1u;
    const int untouched = near_rgb(px(PROBE_W / 4, PROBE_H / 4), 255, 0, 0, 16) &&
                          near_rgb(px(3 * PROBE_W / 4, 3 * PROBE_H / 4), 255, 0, 0, 16);
    return picked && untouched;
}

/* Pixel transfer on the paths that end on the console: a white texel uploaded with the
 * red scale at zero - sampled by the hardware as cyan - and a white glDrawPixels with
 * the green biased away, written into the frame the GPU drew - magenta. Then a read
 * back of pure blue as luminance, which is R + G + B: 255, where reading R alone gave
 * 0. */
static int check_pixel_transfer(void) {
    reset_view();
    static const GLubyte white[4] = {255, 255, 255, 255};
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelTransferf(GL_RED_SCALE, 0.0f);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glPixelTransferf(GL_RED_SCALE, 1.0f);
    glEnable(GL_TEXTURE_2D);
    draw_rect(-0.5f, -0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &t);
    const int uploaded = near_rgb(px(PROBE_W / 2, PROBE_H / 2), 0, 255, 255, 16);

    reset_view();
    static GLubyte block[8 * 8 * 4];
    for (int i = 0; i < 8 * 8 * 4; i++)
        block[i] = 255;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glRasterPos2f(0.0f, 0.0f);
    glPixelTransferf(GL_GREEN_BIAS, -1.0f);
    glDrawPixels(8, 8, GL_RGBA, GL_UNSIGNED_BYTE, block);
    glPixelTransferf(GL_GREEN_BIAS, 0.0f);
    /* The block covers window (64..71, 48..55); px() counts rows down from the top of
     * the area, so window row 52 is px row 43. */
    const int drawn = near_rgb(px(PROBE_W / 2 + 4, PROBE_H / 2 - 5), 255, 0, 255, 16);

    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    GLubyte lum = 0;
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(PROBE_W / 2, PROBE_H / 2, 1, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, &lum);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    return uploaded && drawn && lum == 255;
}

/* GL 1.4's point attenuation and multi-draw: a size-16 point divided by its eye
 * distance of 4 under GL_POINT_DISTANCE_ATTENUATION (0, 0, 1) - 4 pixels a side, so
 * white at its centre and background 4 pixels out, where the undivided point would
 * reach - and two small quads, one glMultiDrawArrays, one each side. The size is the
 * CPU's before the point becomes two triangles, so this should pass on the console. */
static int check_point_params(void) {
    reset_view();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-(double)PROBE_W / 2.0, (double)PROBE_W / 2.0, -(double)PROBE_H / 2.0,
            (double)PROBE_H / 2.0, -10.0, 10.0);
    glMatrixMode(GL_MODELVIEW);
    static const GLfloat atten[3] = {0.0f, 0.0f, 1.0f}, none[3] = {1.0f, 0.0f, 0.0f};
    glPointSize(16.0f);
    glPointParameterfv(GL_POINT_DISTANCE_ATTENUATION, atten);
    glBegin(GL_POINTS);
    glVertex3f(0.0f, 0.0f, -4.0f);
    glEnd();
    glPointParameterfv(GL_POINT_DISTANCE_ATTENUATION, none);
    glPointSize(1.0f);

    static const GLfloat quads[16] = {-40, -4, -32, -4, -32, 4, -40, 4,
                                      32,  -4, 40,  -4, 40,  4, 32,  4};
    static const GLint first[2] = {0, 4};
    static const GLsizei count[2] = {4, 4};
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, quads);
    glMultiDrawArrays(GL_QUADS, first, count, 2);
    glDisableClientState(GL_VERTEX_ARRAY);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    const int cx = PROBE_W / 2, cy = PROBE_H / 2;
    return near_rgb(px(cx, cy), 255, 255, 255, 16) &&
           near_rgb(px(cx + 4, cy), 0x20, 0x20, 0x20, 16) &&
           near_rgb(px(cx - 36, cy), 255, 255, 255, 16) &&
           near_rgb(px(cx + 36, cy), 255, 255, 255, 16);
}

/* GL 1.4's GL_GENERATE_MIPMAP and level-of-detail bias together: a 4x4 texture, red on
 * the left and blue on the right, whose levels are generated - the 1x1 level their
 * mean, purple - drawn 4x4 pixels, where it would sample level 0 and be red on the
 * left, but biased by 2 so the whole quad samples the 1x1 level. On the console the
 * bias is SQ_IMG_SAMP_WORD2's LOD_BIAS, derived from radeonsi and unmeasured, and the
 * generated levels reach the mip chain `mipmap-levels` measures; this is the
 * measurement of the first. */
static int check_lod_bias(void) {
    reset_view();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (double)PROBE_W, 0.0, (double)PROBE_H, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    static GLubyte img[4 * 4 * 4];
    for (int i = 0; i < 16; i++) {
        const int left = (i % 4) < 2;
        img[i * 4] = left ? 255 : 0;
        img[i * 4 + 1] = 0;
        img[i * 4 + 2] = left ? 0 : 255;
        img[i * 4 + 3] = 255;
    }
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, 2.0f);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    const float x0 = (float)(PROBE_W / 2 - 2), y0 = (float)(PROBE_H / 2 - 2);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(x0, y0);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(x0 + 4.0f, y0);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(x0 + 4.0f, y0 + 4.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(x0, y0 + 4.0f);
    glEnd();
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &t);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    /* The quad's left column, window x PROBE_W / 2 - 2; px counts rows down from the
     * top. */
    return near_rgb(px(PROBE_W / 2 - 2, PROBE_H / 2), 128, 0, 128, 16);
}

/* Projective texturing: a 32-pixel strip whose left edge has (s, q) = (0, 1) and right
 * edge (3, 3), over a two-texel texture, red then green. s/q runs 0 to 1 either way,
 * but GL divides per fragment, after interpolation - 3f / (1 + 2f) at fraction f of the
 * width - so the strip turns green at column 8, not column 16. Column 12 is the
 * witness: green when q is divided per fragment, red when it was divided at the
 * vertices. The console's pixel shader divides per fragment too since 2026-09-19, q in
 * the texture parameter's w - unmeasured; this is the measurement. */
static int check_projective_texture(void) {
    reset_view();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (double)PROBE_W, 0.0, (double)PROBE_H, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    static const GLubyte texels[2 * 4] = {255, 0, 0, 255, 0, 255, 0, 255};
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    const float x0 = (float)(PROBE_W / 2 - 16), y0 = (float)(PROBE_H / 2 - 2);
    glBegin(GL_QUADS);
    glTexCoord4f(0.0f, 0.0f, 0.0f, 1.0f);
    glVertex2f(x0, y0);
    glTexCoord4f(3.0f, 0.0f, 0.0f, 3.0f);
    glVertex2f(x0 + 32.0f, y0);
    glTexCoord4f(3.0f, 0.0f, 0.0f, 3.0f);
    glVertex2f(x0 + 32.0f, y0 + 4.0f);
    glTexCoord4f(0.0f, 0.0f, 0.0f, 1.0f);
    glVertex2f(x0, y0 + 4.0f);
    glEnd();
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &t);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    const int left = PROBE_W / 2 - 16, row = PROBE_H / 2;
    return near_rgb(px(left + 4, row), 255, 0, 0, 16) &&  /* red either way */
           near_rgb(px(left + 12, row), 0, 255, 0, 16) && /* the witness */
           near_rgb(px(left + 24, row), 0, 255, 0, 16);   /* green either way */
}

/* GL 1.5's buffer mapping: a buffer's store mapped, a quad's corners written through
 * the pointer, unmapped, and drawn from it - green at the middle. The store is process
 * memory on both paths and the draw reads it on the CPU, so this should pass on the
 * console. */
static int check_buffer_map(void) {
    reset_view();
    GLuint b = 0;
    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, 8 * (GLsizeiptr)sizeof(GLfloat), NULL,
                 GL_DYNAMIC_DRAW);
    GLfloat *p = (GLfloat *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (!p) {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDeleteBuffers(1, &b);
        return 0;
    }
    static const GLfloat quad[8] = {-0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f};
    for (int i = 0; i < 8; i++)
        p[i] = quad[i];
    const GLboolean kept = glUnmapBuffer(GL_ARRAY_BUFFER);
    glColor3f(0.0f, 1.0f, 0.0f);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, (const GLvoid *)0);
    glDrawArrays(GL_QUADS, 0, 4);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
    glColor3f(1.0f, 1.0f, 1.0f);
    if (glGetError() != GL_NO_ERROR || kept != GL_TRUE)
        return 0;
    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 0, 255, 0, 16);
}

/* GL 1.5's occlusion query around a rectangle of known area, 32x24 pixels. **Either the
 * count is exact or GL_QUERY_COUNTER_BITS is 0** - GL 1.5's declaration that the count
 * carries no information. What would fail is a count that is neither: bits claimed and
 * a wrong number.
 *
 * **The console claims the bits since 2026-09-20**, so this now asks it for an exact
 * number there: oops-gl brackets the query with two ZPASS_DONE events and sums the
 * sixteen render backends. The escape the check still leaves is the honest one, not a
 * loophole - a query whose draws never test depth is not counted on that path, and this
 * one enables the depth test so that it is. A sum that read one backend instead of all
 * of them, or that took the counters' valid bit for count, comes out as a wrong number
 * rather than as no answer. */
static int check_occlusion_query(void) {
    reset_view();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (double)PROBE_W, 0.0, (double)PROBE_H, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    GLuint q = 0;
    glGenQueries(1, &q);
    /* **The depth test on**, which reset_view leaves off and which the console needs:
     * its counters live in the depth block and run only against a bound depth surface.
     * reset_view has just cleared depth to 1.0, so the rectangle at z = 0 passes
     * GL_LESS everywhere and the exact count is still the area. */
    glEnable(GL_DEPTH_TEST);
    glBeginQuery(GL_SAMPLES_PASSED, q);
    glRectf(16.0f, 16.0f, 48.0f, 40.0f);
    glEndQuery(GL_SAMPLES_PASSED);
    glDisable(GL_DEPTH_TEST);
    GLint bits = -1;
    GLuint result = 0, ready = 0;
    glGetQueryiv(GL_SAMPLES_PASSED, GL_QUERY_COUNTER_BITS, &bits);
    glGetQueryObjectuiv(q, GL_QUERY_RESULT_AVAILABLE, &ready);
    glGetQueryObjectuiv(q, GL_QUERY_RESULT, &result);
    glDeleteQueries(1, &q);
    if (glGetError() != GL_NO_ERROR || ready != GL_TRUE)
        return 0;
    return bits == 0 || (bits >= 1 && result == 32u * 24u);
}

/* GL 1.0's colour-index images in an RGBA context, refused until 2026-09-19: indices 0
 * and 1 - the second as a GL_BITMAP bit - drawn through two-entry I_TO_R/G/B/A maps as
 * blue and red blocks. The index maps are CPU work, so this should pass on the console.
 */
static int check_index_pixels(void) {
    reset_view();
    static const GLfloat r[2] = {0.0f, 1.0f}, g[2] = {0.0f, 0.0f}, b[2] = {1.0f, 0.0f};
    static const GLfloat a[2] = {1.0f, 1.0f}, zero[1] = {0.0f};
    static const GLubyte idx[2] = {0, 1};
    static const GLubyte bit[1] = {0x80};
    glPixelMapfv(GL_PIXEL_MAP_I_TO_R, 2, r);
    glPixelMapfv(GL_PIXEL_MAP_I_TO_G, 2, g);
    glPixelMapfv(GL_PIXEL_MAP_I_TO_B, 2, b);
    glPixelMapfv(GL_PIXEL_MAP_I_TO_A, 2, a);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelZoom(8.0f, 8.0f);
    glWindowPos2i(PROBE_W / 4, PROBE_H / 2);
    glDrawPixels(2, 1, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, idx);
    glWindowPos2i(PROBE_W * 3 / 4, PROBE_H / 2);
    glDrawPixels(1, 1, GL_COLOR_INDEX, GL_BITMAP, bit);
    glPixelZoom(1.0f, 1.0f);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelMapfv(GL_PIXEL_MAP_I_TO_R, 1, zero);
    glPixelMapfv(GL_PIXEL_MAP_I_TO_G, 1, zero);
    glPixelMapfv(GL_PIXEL_MAP_I_TO_B, 1, zero);
    glPixelMapfv(GL_PIXEL_MAP_I_TO_A, 1, zero);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    /* px counts rows down from the top: the blocks are window rows PROBE_H / 2 .. + 7.
     */
    const int row = PROBE_H / 2 - 4;
    return near_rgb(px(PROBE_W / 4 + 4, row), 0, 0, 255, 16) &&
           near_rgb(px(PROBE_W / 4 + 12, row), 255, 0, 0, 16) &&
           near_rgb(px(PROBE_W * 3 / 4 + 4, row), 255, 0, 0, 16);
}

/* GL 1.0's stencil pixel rectangles, refused until 2026-09-19: four stencil indices
 * drawn with glDrawPixels, read back with glReadPixels, and copied with glCopyPixels.
 * On the console the stencil buffer is the GPU's surface, tiled 64KB_Z_X, and oops-gl
 * addresses it through addrlib's vectors. Every step here is the CPU's, so this passes
 * under any consistent addressing, right or wrong. `stencil-readback` is the check that
 * crosses to the GPU. */
static int check_stencil_pixels(void) {
    reset_view();
    static const GLubyte in[4] = {5, 6, 7, 8};
    GLubyte out[4] = {0, 0, 0, 0}, copied[4] = {0, 0, 0, 0};
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
    glWindowPos2i(PROBE_W / 2, PROBE_H / 2);
    glDrawPixels(2, 2, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, in);
    glReadPixels(PROBE_W / 2, PROBE_H / 2, 2, 2, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE,
                 out);
    glWindowPos2i(PROBE_W / 4, PROBE_H / 4);
    glCopyPixels(PROBE_W / 2, PROBE_H / 2, 2, 2, GL_STENCIL);
    glReadPixels(PROBE_W / 4, PROBE_H / 4, 2, 2, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE,
                 copied);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    for (int i = 0; i < 4; i++) {
        if (out[i] != in[i] || copied[i] != in[i])
            return 0;
    }
    return 1;
}

/* The centre pixel of the buffer glReadBuffer names, read with glReadPixels - which
 * reaches the front as well as the back, where px() reads only what the back became. */
static int read_centre(GLenum buffer, GLubyte out[4]) {
    glReadBuffer(buffer);
    glReadPixels(PROBE_W / 2, PROBE_H / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, out);
    glReadBuffer(GL_BACK);
    return glGetError() == GL_NO_ERROR;
}

static int near_bytes(const GLubyte c[4], int r, int g, int b) {
    const int d[3] = {c[0] - r, c[1] - g, c[2] - b};
    for (int i = 0; i < 3; i++) {
        if (d[i] > 8 || d[i] < -8)
            return 0;
    }
    return 1;
}

/* **The front buffer** (GL 1.0; since 2026-09-19): glDrawBuffer(GL_FRONT) draws into a
 * surface of its own. A red quad goes into the front over a blue back, the back must
 * stay blue, and both are read by name. On the console the front is its own colour
 * target, the address in CB_COLOR0_BASE, and glFlush puts it on screen, so the display
 * shows it for a moment. */
static int check_front_buffer(void) {
    reset_view();
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f);
    glDrawBuffer(GL_FRONT);
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
    glDrawBuffer(GL_BACK);
    GLubyte front[4], back[4];
    if (!read_centre(GL_FRONT, front) || !read_centre(GL_BACK, back))
        return 0;
    return near_bytes(front, 255, 0, 0) && near_bytes(back, 0, 0, 255);
}

/* **Both buffers at once** - GL_FRONT_AND_BACK - with a green quad added over a red
 * front and a blue back, each blended against its own pixel, so the front turns yellow
 * and the back cyan. **Expected to pass on both paths since 2026-09-20.** The console
 * binds `CB_COLOR1` to the front, carries MRT1 in `CB_TARGET_MASK`, `CB_SHADER_MASK`
 * and `SPI_SHADER_COL_FORMAT`, and exports twice from both pixel shaders - the
 * registers measured by obSCEne's `REQ-20260919T2258Z-3f62`, the two export words
 * assembled in `tools/shader/mrt1-export.s`. It drew into the back only before that,
 * which this check was expected to catch. */
static int check_front_and_back(void) {
    reset_view();
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f);
    glDrawBuffer(GL_FRONT);
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
    /* **What the two buffers hold before the blended draw**, through the same
     * `read_centre` the verdict uses. `front-buffer` does exactly these two reads and
     * passes on hardware, so these rows should be red and blue; if they are, the read
     * path is sound and whatever goes wrong below belongs to the draw, and if they are
     * not, the verdict was never measuring blending. */
    GLubyte was_front[4], was_back[4];
    if (!read_centre(GL_FRONT, was_front) || !read_centre(GL_BACK, was_back))
        return 0;

    glDrawBuffer(GL_FRONT_AND_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f);
    glDisable(GL_BLEND);
    glDrawBuffer(GL_BACK);
    GLubyte front[4], back[4];
    if (!read_centre(GL_FRONT, front) || !read_centre(GL_BACK, back))
        return 0;
    /* **The same two pixels again, straight away, nothing drawn between.** Two runs of
     * the rows below disagreed about the blue channel and about nothing else - the
     * front's blue came back `0x14` on the 10:46 build and `0x56` on the 11:32 one, the
     * back's `0x00` then `0x46` - and the only difference between those builds is that
     * the second reads each buffer once more beforehand. A value that moves when the
     * number of preceding reads changes is not a wrong blend; it is a read of something
     * still settling. If these two rows differ from the two above, that is what it is,
     * and no amount of staring at `CB_BLEND1_CONTROL` will show it. */
    GLubyte front2[4], back2[4];
    if (!read_centre(GL_FRONT, front2) || !read_centre(GL_BACK, back2))
        return 0;
    /*
     * **Four rows, because the verdict row cannot carry any of this.** `gl1_probe_saw`
     * reads the centre of the probe region through `frame()`, which is the *back*; on
     * 2026-09-20 and on every run of 2026-09-21 it printed `0xff00ffff`, the cyan GL
     * asks for, while the check went on failing. Three runs reported the half that was
     * already right.
     *
     * What the first build to print these measured, on 2026-09-21:
     *
     *   front  0xffffff14   red plus green - **blended**, which it had never been
     * before `CB_BLEND1_CONTROL` was emitted. Fails on blue 20 against a tolerance
     * of 8. back   0xff00ff00   pure green, where `frame()` reads the same pixel as
     * cyan.
     *
     * So the draw reaches both targets and the two read paths disagree about the back.
     * The `was_*` rows above bracket the blended draw with the same reads, because
     * `front-buffer` makes exactly those two and passes: if they come back red and
     * blue, `read_centre` is sound and the blended draw is what breaks it.
     */
    if (gl1_probe_saw) {
#define SAW_BYTES(n, c)                                                                \
    gl1_probe_saw((n), ((uint32_t)(c)[3] << 24) | ((uint32_t)(c)[0] << 16) |           \
                           ((uint32_t)(c)[1] << 8) | (uint32_t)(c)[2])
        SAW_BYTES("front-and-back/was-f", was_front);
        SAW_BYTES("front-and-back/was-b", was_back);
        SAW_BYTES("front-and-back/front", front);
        SAW_BYTES("front-and-back/back", back);
        SAW_BYTES("front-and-back/front2", front2);
        SAW_BYTES("front-and-back/back2", back2);
#undef SAW_BYTES
    }
    return near_bytes(front, 255, 255, 0) && near_bytes(back, 0, 255, 255);
}

/* The two rows the readback checks compare whole, either side of window row 56. On a
 * 1080-line display, the depth and stencil surfaces both change from one row of 64 KiB
 * blocks to the next there: window row y is surface row 1079 - y, and 1024 starts a
 * block row for depth's 128-pixel blocks and for stencil's 256-pixel ones. */
#define PROBE_ZS_ROW_LOW 40
#define PROBE_ZS_ROW_HIGH 70

/* **The depth the GPU drew, read back by the CPU** (since 2026-09-19). On the console,
 * glReadPixels of GL_DEPTH_COMPONENT reads the GPU's depth surface through oops-gl's
 * 64KB_Z_X addressing - the vectors tools/zs-tiling took from addrlib - so this check
 * measures that addressing. A wrong swizzle scatters the depths drawn here, and the
 * rows come back out of order. The draw is a plane over the left three quarters,
 * slanted so that depth rises across x by 1/192 a pixel. The tolerance is under half a
 * step, so neighbours swapped would fail. The last quarter keeps the clear value, and
 * every pixel of two rows is compared. */
static int check_depth_readback(void) {
    reset_view();
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    /* Eye z 0.5 at x = -1 to -0.5 at x = 0.5: window depth (1 - z) / 2, 0.25 to 0.75
     * across columns 0 to 96. */
    glColor3f(0.3f, 0.3f, 0.3f);
    glBegin(GL_QUADS);
    glVertex3f(-1.0f, -1.0f, 0.5f);
    glVertex3f(0.5f, -1.0f, -0.5f);
    glVertex3f(0.5f, 1.0f, -0.5f);
    glVertex3f(-1.0f, 1.0f, 0.5f);
    glEnd();
    glDepthFunc(GL_LESS);
    glDisable(GL_DEPTH_TEST);
    static GLfloat row[2][PROBE_W];
    glReadPixels(0, PROBE_ZS_ROW_LOW, PROBE_W, 1, GL_DEPTH_COMPONENT, GL_FLOAT, row[0]);
    glReadPixels(0, PROBE_ZS_ROW_HIGH, PROBE_W, 1, GL_DEPTH_COMPONENT, GL_FLOAT,
                 row[1]);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    for (int r = 0; r < 2; r++) {
        for (int x = 0; x < PROBE_W; x++) {
            const float want =
                x < PROBE_W * 3 / 4 ? 0.25f + ((float)x + 0.5f) / 192.0f : 1.0f;
            const float d = row[r][x] - want;
            if (d > 0.002f || d < -0.002f)
                return 0;
        }
    }
    return 1;
}

/* **Stencil across the CPU and the GPU, both ways** (since 2026-09-19). First the GPU
 * stamps 3 into the left half, and glReadPixels reads two rows of it back. Then the CPU
 * writes 5 into a 16 x 16 block on the right with glDrawPixels, and a draw tested
 * GL_EQUAL 5 lands there and nowhere else. `stencil` stays on the GPU and
 * `stencil-pixels` on the CPU, so this is the check that measures the stencil surface's
 * 64KB_Z_X addressing on the console. */
static int check_stencil_readback(void) {
    reset_view();
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
    glStencilMask(0xff);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 3, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    draw_rect(-1.0f, -1.0f, 0.0f, 1.0f, 0.2f, 0.2f, 0.2f);
    glDisable(GL_STENCIL_TEST);
    static GLubyte row[2][PROBE_W];
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, PROBE_ZS_ROW_LOW, PROBE_W, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE,
                 row[0]);
    glReadPixels(0, PROBE_ZS_ROW_HIGH, PROBE_W, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE,
                 row[1]);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);

    static GLubyte fives[16 * 16];
    for (int i = 0; i < 16 * 16; i++)
        fives[i] = 5;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glWindowPos2i(PROBE_W * 3 / 4 - 8, PROBE_H / 2 - 8);
    glDrawPixels(16, 16, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, fives);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 5, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f);
    glDisable(GL_STENCIL_TEST);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    for (int r = 0; r < 2; r++) {
        for (int x = 0; x < PROBE_W; x++) {
            if (row[r][x] != (x < PROBE_W / 2 ? 3 : 0))
                return 0;
        }
    }
    /* Exactly the block is green: 256 pixels, all in the right half. */
    int green = 0, green_left = 0;
    const uint32_t *s = scan_frame();
    for (int y = 0; y < PROBE_H; y++) {
        for (int x = 0; x < PROBE_W; x++) {
            if (!near_rgb(SCAN_PX(s, x, y), 0, 255, 0, 8))
                continue;
            green++;
            if (x < PROBE_W / 2)
                green_left++;
        }
    }
    return green == 16 * 16 && green_left == 0;
}

/* GL 1.4's depth texture and shadow comparison: a 2x1 depth texture of 0.25 and 0.75
 * compared against r = 0.5 under GL_LEQUAL across the area - black on the left, where
 * 0.5 > 0.25, white on the right. **Expected to pass on the console since 2026-09-20**,
 * when oops-sdk gained the comparison sample: the image format is 32_FLOAT, the
 * sampler's own DEPTH_COMPARE_FUNC does the comparison, and the pixel shader hands it
 * the clamped reference. It drew untextured before that. The two halves differing is
 * what makes this a real check on hardware - a comparison that always passed, or a
 * reference read from the wrong address register, shows up here as one colour across
 * the whole area. */
static int check_shadow_compare(void) {
    reset_view();
    static const GLfloat depths[2] = {0.25f, 0.75f};
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 2, 1, 0, GL_DEPTH_COMPONENT,
                 GL_FLOAT, depths);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glBegin(GL_QUADS);
    glTexCoord3f(0.0f, 0.0f, 0.5f);
    glVertex2f(-1.0f, -1.0f);
    glTexCoord3f(1.0f, 0.0f, 0.5f);
    glVertex2f(1.0f, -1.0f);
    glTexCoord3f(1.0f, 1.0f, 0.5f);
    glVertex2f(1.0f, 1.0f);
    glTexCoord3f(0.0f, 1.0f, 0.5f);
    glVertex2f(-1.0f, 1.0f);
    glEnd();
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &t);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    return near_rgb(px(PROBE_W / 4, PROBE_H / 2), 0, 0, 0, 16) &&
           near_rgb(px(PROBE_W * 3 / 4, PROBE_H / 2), 255, 255, 255, 16);
}

/* A pixel rectangle's pixels are fragments: a half-alpha red 8x8 image zoomed to 64x48
 * across the middle of a blue area, blended, through a scissor box that keeps only its
 * right half - so the left half stays blue and the right is purple. Both were written
 * straight into the frame until 2026-09-19. On the console these are the CPU's
 * operations on the flushed frame, so this should pass there. */
static int check_pixel_fragments(void) {
    reset_view();
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    static GLubyte img[8 * 8 * 4];
    for (int i = 0; i < 8 * 8; i++) {
        img[i * 4] = 255;
        img[i * 4 + 1] = 0;
        img[i * 4 + 2] = 0;
        img[i * 4 + 3] = 128;
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_SCISSOR_TEST);
    glScissor(PROBE_W / 2, 0, PROBE_W / 2, PROBE_H);
    glPixelZoom((float)(PROBE_W / 2) / 8.0f, (float)(PROBE_H / 2) / 8.0f);
    glWindowPos2i(PROBE_W / 4, PROBE_H / 4);
    glDrawPixels(8, 8, GL_RGBA, GL_UNSIGNED_BYTE, img);
    glPixelZoom(1.0f, 1.0f);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glClearColor(0.125f, 0.125f, 0.125f, 1.0f);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    return near_rgb(px(PROBE_W * 3 / 8, PROBE_H / 2), 0, 0, 255, 16) &&
           near_rgb(px(PROBE_W * 5 / 8, PROBE_H / 2), 128, 0, 127, 16);
}

/* A 3D texture: two slices, red then green, sampled at r = 0.75 - green. **Expected to
 * pass on the console since 2026-09-20**, when the hardware half landed: the
 * descriptor's TYPE is 0xa with the last slice in WORD4, the vertex carries r in its
 * third parameter, and the pixel shader's sample slot divides it by q and samples with
 * three coordinates. It drew untextured - the vertex colour, white - before that. */
static int check_texture_3d(void) {
    reset_view();
    static GLubyte vol[2 * 2 * 2 * 4];
    for (int i = 0; i < 8; i++) {
        const int slice = i / 4;
        vol[i * 4 + 0] = (GLubyte)(slice ? 0 : 255);
        vol[i * 4 + 1] = (GLubyte)(slice ? 255 : 0);
        vol[i * 4 + 2] = 0;
        vol[i * 4 + 3] = 255;
    }
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_3D, t);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, vol);
    glEnable(GL_TEXTURE_3D);
    glColor3f(1.0f, 1.0f, 1.0f);
    glTexCoord3f(0.5f, 0.5f, 0.75f);
    glRectf(-0.5f, -0.5f, 0.5f, 0.5f);
    glDisable(GL_TEXTURE_3D);
    glBindTexture(GL_TEXTURE_3D, 0);
    glDeleteTextures(1, &t);
    glTexCoord4f(0.0f, 0.0f, 0.0f, 1.0f);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 0, 255, 0, 16);
}

/* **A volume's mip chain** (the console since 2026-09-21): a 4x4x4 texture whose level
 * 0 is red and whose level 1 is green, minified hard enough that the sampler must read
 * level 1.
 *
 * The console built no chain for a volume until then - `gl_tex_chain_levels` refused
 * one, because the layout was two-dimensional and a volume's levels halve depth as well
 * - so `LAST_LEVEL` stayed 0, minification read the base level, and this would have
 * come back red while the software rasteriser answered green. Both halves of the
 * three-dimensional layout are measured rather than reasoned: the slice stride by
 * `texture-3d`, which reads slice 1 at `pitch * height` and passes, and the
 * smallest-first level placement by `mipmap-levels`.
 *
 * **The minification is forced by the texture matrix**, not by geometry: the quad is
 * half the probe's width, 64 pixels, and scaling the coordinate by 64 puts 256 texels
 * of a 4-texel texture across them - four texels a pixel, a level of detail of 2, which
 * `GL_TEXTURE_MAX_LEVEL` at 1 clamps to level 1. Overshooting is deliberate: the clamp
 * makes any scale past level 1 give the same answer, so the check does not depend on
 * the exact level the derivative works out to. A first attempt scaled by 8, which is
 * half a texel a pixel and magnifies - the host reference caught it. Geometry small
 * enough to minify by itself would be a few pixels wide and hard to sample. */
static int check_volume_mipmap(void) {
    reset_view();
    static GLubyte lvl0[4 * 4 * 4 * 4];
    static GLubyte lvl1[2 * 2 * 2 * 4];
    for (int i = 0; i < 4 * 4 * 4; i++) {
        lvl0[i * 4 + 0] = 255;
        lvl0[i * 4 + 1] = 0;
        lvl0[i * 4 + 2] = 0;
        lvl0[i * 4 + 3] = 255;
    }
    for (int i = 0; i < 2 * 2 * 2; i++) {
        lvl1[i * 4 + 0] = 0;
        lvl1[i * 4 + 1] = 255;
        lvl1[i * 4 + 2] = 0;
        lvl1[i * 4 + 3] = 255;
    }
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_3D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 lvl0);
    glTexImage3D(GL_TEXTURE_3D, 1, GL_RGBA, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 lvl1);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 1);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEnable(GL_TEXTURE_3D);

    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    glScalef(64.0f, 64.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);

    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord3f(0.0f, 0.0f, 0.5f);
    glVertex2f(-0.5f, -0.5f);
    glTexCoord3f(1.0f, 0.0f, 0.5f);
    glVertex2f(0.5f, -0.5f);
    glTexCoord3f(1.0f, 1.0f, 0.5f);
    glVertex2f(0.5f, 0.5f);
    glTexCoord3f(0.0f, 1.0f, 0.5f);
    glVertex2f(-0.5f, 0.5f);
    glEnd();

    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glDisable(GL_TEXTURE_3D);
    glBindTexture(GL_TEXTURE_3D, 0);
    glDeleteTextures(1, &t);
    glTexCoord4f(0.0f, 0.0f, 0.0f, 1.0f);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    /* Green is level 1. Red is the base level, which is what a path with no chain
     * samples. */
    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 0, 255, 0, 16);
}

/* Packed pixel types and the pixel-store skips, which glTexImage2D and glReadPixels
 * refused until 2026-09-19. A 2x1 texture comes out of the middle of a three-pixel
 * GL_UNSIGNED_SHORT_5_6_5 row (skip one pixel: white, then red, green), is drawn
 * GL_NEAREST across a quad - red left, green right - and read back as floats and as
 * 5_6_5 through a pack skip. The conversion is host code on both paths; what this puts
 * on a console is that the converted texels are the ones the sampler gets, and that
 * readback packs what the colour buffer holds. */
static int check_pixel_types(void) {
    reset_view();
    static const GLushort row[3] = {0xFFFFu, 0xF800u, 0x07E0u}; /* white, red, green */
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 3);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 1, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
                 row);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    const int uploaded = glGetError() == GL_NO_ERROR;

    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-0.8f, -0.8f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(0.8f, -0.8f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(0.8f, 0.8f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-0.8f, 0.8f);
    glEnd();
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &t);
    if (!uploaded || glGetError() != GL_NO_ERROR)
        return 0;
    const int sampled = near_rgb(px(PROBE_W * 5 / 16, PROBE_H / 2), 255, 0, 0, 16) &&
                        near_rgb(px(PROBE_W * 11 / 16, PROBE_H / 2), 0, 255, 0, 16);

    GLfloat f[3] = {0.0f, 0.0f, 0.0f};
    glReadPixels(PROBE_W * 11 / 16, PROBE_H / 2, 1, 1, GL_RGB, GL_FLOAT, f);
    GLushort packed[3] = {0u, 0u, 0u};
    glPixelStorei(GL_PACK_ALIGNMENT, 2);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 2);
    glReadPixels(PROBE_W * 5 / 16, PROBE_H / 2, 1, 1, GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
                 packed);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    const int read_float = f[0] < 0.07f && f[1] > 0.93f && f[2] < 0.07f;
    const int read_packed = packed[0] == 0u && packed[2] == 0xF800u;
    return sampled && read_float && read_packed;
}

/* One 1x1 texture of the given internal format, drawn over a quadrant in the given
 * environment mode and colour. */
static void internal_format_quad(GLuint t, GLint ifmt, const GLubyte texel[4],
                                 GLenum mode, float x0, float y0, float r, float g,
                                 float b, float a) {
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, ifmt, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texel);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (GLint)mode);
    glColor4f(r, g, b, a);
    glTexCoord2f(0.5f, 0.5f);
    glRectf(x0, y0, x0 + 1.0f, y0 + 1.0f);
}

/* Internal formats, which were ignored until 2026-09-19, and the texture environment's
 * words that now depend on them - on a console, the combine slot rewritten between
 * draws of one frame. Four quadrants over a blue clear, each of which drew something
 * else before:
 * - lower left, **an RGB texture under GL_REPLACE keeps the fragment's alpha**
 * (`v_mov_b32 v7, v11`): white at alpha 0 blended away, blue. It was white.
 * - upper left, **an alpha texture leaves the colour alone**: black RGB uploaded as
 * GL_ALPHA, modulating red, is red. It was black.
 * - upper right, **intensity is all four channels**: (128, 0, 0, 0) as GL_INTENSITY
 * modulating white, grey. It was dark red.
 * - lower right, **GL_ADD on the hardware** (`v_add_f32`): green 128 added to red 128,
 * olive. It was the product, black. */
static int check_internal_formats(void) {
    reset_view();
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    GLuint t[4] = {0, 0, 0, 0};
    glGenTextures(4, t);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    static const GLubyte white[4] = {255, 255, 255, 255}, black[4] = {0, 0, 0, 255};
    static const GLubyte dark_red[4] = {128, 0, 0, 0}, green[4] = {0, 128, 0, 255};
    internal_format_quad(t[0], GL_RGB8, white, GL_REPLACE, -1.0f, -1.0f, 1.0f, 1.0f,
                         1.0f, 0.0f);
    glDisable(GL_BLEND);
    internal_format_quad(t[1], GL_ALPHA8, black, GL_MODULATE, -1.0f, 0.0f, 1.0f, 0.0f,
                         0.0f, 1.0f);
    internal_format_quad(t[2], GL_INTENSITY8, dark_red, GL_MODULATE, 0.0f, 0.0f, 1.0f,
                         1.0f, 1.0f, 1.0f);
    internal_format_quad(t[3], GL_RGBA8, green, GL_ADD, 0.0f, -1.0f, 0.5f, 0.0f, 0.0f,
                         1.0f);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(4, t);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    /* px counts rows down from the top. */
    const int lower_left = near_rgb(px(PROBE_W / 4, PROBE_H * 3 / 4), 0, 0, 255, 16);
    const int upper_left = near_rgb(px(PROBE_W / 4, PROBE_H / 4), 255, 0, 0, 16);
    const int upper_right =
        near_rgb(px(PROBE_W * 3 / 4, PROBE_H / 4), 128, 128, 128, 16);
    const int lower_right =
        near_rgb(px(PROBE_W * 3 / 4, PROBE_H * 3 / 4), 128, 128, 0, 16);
    return lower_left && upper_left && upper_right && lower_right;
}

/* One band of a red | green texture with the given s wrap, s running from -1 at the
 * left edge to 2 at the right. */
static void wrap_band(GLuint t, GLenum wrap, float y0, float y1) {
    static const GLubyte rg[8] = {255, 0, 0, 255, 0, 255, 0, 255};
    static const GLfloat blue[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, rg);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)wrap);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, blue);
    glBegin(GL_QUADS);
    glTexCoord2f(-1.0f, 0.5f);
    glVertex2f(-1.0f, y0);
    glTexCoord2f(2.0f, 0.5f);
    glVertex2f(1.0f, y0);
    glTexCoord2f(2.0f, 0.5f);
    glVertex2f(1.0f, y1);
    glTexCoord2f(-1.0f, 0.5f);
    glVertex2f(-1.0f, y1);
    glEnd();
}

/* GL_CLAMP_TO_BORDER and GL_MIRRORED_REPEAT, which were stored and sampled as GL_REPEAT
 * until 2026-09-19 (any wrap value was kept, and the sampler knew two). A blue border
 * is none of the sampler's built-in colours, so the upper band reads it from the border
 * colour table TA_BC_BASE_ADDR points at - the console's verdict on that register and
 * the table's layout. The lower band's second repetition is reflected: green where
 * GL_REPEAT gives red. Pixel x samples s = -1 + 3 (x + 0.5) / PROBE_W: s = -0.5,
 * 0.25, 1.25 and 1.75 at PROBE_W / 6, 5/12, 3/4, 11/12. */
static int check_border_and_mirror(void) {
    reset_view();
    GLuint t[2] = {0, 0};
    glGenTextures(2, t);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    wrap_band(t[0], GL_CLAMP_TO_BORDER, 0.1f, 0.9f);
    wrap_band(t[1], GL_MIRRORED_REPEAT, -0.9f, -0.1f);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(2, t);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    const int up = PROBE_H / 4,
              down = PROBE_H * 3 / 4; /* px counts rows down from the top */
    const int border = near_rgb(px(PROBE_W / 6, up), 0, 0, 255, 16) &&
                       near_rgb(px(PROBE_W * 5 / 12, up), 255, 0, 0, 16) &&
                       near_rgb(px(PROBE_W * 3 / 4, up), 0, 0, 255, 16);
    const int mirror = near_rgb(px(PROBE_W * 5 / 12, down), 255, 0, 0, 16) &&
                       near_rgb(px(PROBE_W * 3 / 4, down), 0, 255, 0, 16) &&
                       near_rgb(px(PROBE_W * 11 / 12, down), 255, 0, 0, 16);
    return border && mirror;
}

/* Vertex arrays of the other GL 1.1 types, which were read as floats until 2026-09-19
 * whatever the pointer named: a GL_SHORT quad in pixel coordinates with a
 * GL_UNSIGNED_SHORT colour array, red, on the left; and GL 1.4's glWindowPos placing a
 * 4x4 white glBitmap on the right. Both are CPU work before anything reaches the GPU,
 * so this should pass on the console. */
static int check_array_types(void) {
    reset_view();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (double)PROBE_W, 0.0, (double)PROBE_H, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    const GLshort x0 = (GLshort)(PROBE_W / 8), x1 = (GLshort)(PROBE_W * 3 / 8);
    const GLshort y0 = (GLshort)(PROBE_H / 4), y1 = (GLshort)(PROBE_H * 3 / 4);
    const GLshort quad[8] = {x0, y0, x1, y0, x1, y1, x0, y1};
    static const GLushort red[12] = {65535, 0, 0, 65535, 0, 0,
                                     65535, 0, 0, 65535, 0, 0};
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(2, GL_SHORT, 0, quad);
    glColorPointer(3, GL_UNSIGNED_SHORT, 0, red);
    glDrawArrays(GL_QUADS, 0, 4);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);

    static const GLubyte block[4] = {0xf0, 0xf0, 0xf0, 0xf0};
    glColor3f(1.0f, 1.0f, 1.0f);
    glWindowPos2i(PROBE_W * 3 / 4, PROBE_H / 2);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBitmap(4, 4, 0.0f, 0.0f, 0.0f, 0.0f, block);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    /* px counts rows down from the top: window row PROBE_H / 2 + 1 is px row PROBE_H /
     * 2 - 2. */
    return near_rgb(px(PROBE_W / 4, PROBE_H / 2), 255, 0, 0, 16) &&
           near_rgb(px(PROBE_W * 3 / 4 + 1, PROBE_H / 2 - 2), 255, 255, 255, 16);
}

/* GL 1.4's colour sum: a red quad whose blue secondary colour GL_COLOR_SUM adds,
 * magenta - from glSecondaryColor on the left and from a GL_UNSIGNED_BYTE secondary
 * colour array on the right, with the current secondary black there so only the array
 * can supply the blue. Untextured, so the hardware path's per-vertex sum is exact and
 * this should pass on the console; the textured case is separate-specular's, which
 * measures the third interpolant that carries the sum. */
static int check_color_sum(void) {
    reset_view();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (double)PROBE_W, 0.0, (double)PROBE_H, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_COLOR_SUM);
    glColor3f(1.0f, 0.0f, 0.0f);
    glSecondaryColor3f(0.0f, 0.0f, 1.0f);
    glRectf((float)PROBE_W / 8.0f, (float)PROBE_H / 4.0f, (float)PROBE_W * 3.0f / 8.0f,
            (float)PROBE_H * 3.0f / 4.0f);

    const GLshort x0 = (GLshort)(PROBE_W * 5 / 8), x1 = (GLshort)(PROBE_W * 7 / 8);
    const GLshort y0 = (GLshort)(PROBE_H / 4), y1 = (GLshort)(PROBE_H * 3 / 4);
    const GLshort quad[8] = {x0, y0, x1, y0, x1, y1, x0, y1};
    static const GLubyte blue[12] = {0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255};
    glSecondaryColor3f(0.0f, 0.0f, 0.0f);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_SECONDARY_COLOR_ARRAY);
    glVertexPointer(2, GL_SHORT, 0, quad);
    glSecondaryColorPointer(3, GL_UNSIGNED_BYTE, 0, blue);
    glDrawArrays(GL_QUADS, 0, 4);
    glDisableClientState(GL_SECONDARY_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisable(GL_COLOR_SUM);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    return near_rgb(px(PROBE_W / 4, PROBE_H / 2), 255, 0, 255, 16) &&
           near_rgb(px(PROBE_W * 3 / 4, PROBE_H / 2), 255, 0, 255, 16);
}

/* Antialiasing: a white GL_POINT_SMOOTH point of size 6 blended over black. Its centre
 * is white and the corner of the 6x6 square an aliased point fills - 3.5 pixels out
 * both ways, outside the disc - black. **Expected to pass on the console since
 * 2026-09-20**, when the untextured pixel shader gained a coverage slot: the CPU writes
 * each corner's offset from the centre into the texture-coordinate parameter and the
 * shader turns the interpolated offset into GL's coverage. It drew the aliased square,
 * corner and all, before that. The corner is what makes this a real check there - a
 * coverage that came out as one everywhere passes at the centre and fails here. */
static int check_smooth(void) {
    reset_view();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POINT_SMOOTH);
    glPointSize(6.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_POINTS);
    glVertex2f(0.0f, 0.0f);
    glEnd();
    glDisable(GL_POINT_SMOOTH);
    glPointSize(1.0f);
    glDisable(GL_BLEND);
    glClearColor(0.125f, 0.125f, 0.125f, 1.0f);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 255, 255, 255, 16) &&
           near_rgb(px(PROBE_W / 2 + 2, PROBE_H / 2 + 2), 0, 0, 0, 16);
}

/* **GL_POLYGON_SMOOTH** (the console since 2026-09-21): a white triangle blended over
 * black, sampled where its slanted edge crosses a pixel.
 *
 * The console drew this aliased until then - the roadmap gave two reasons and both were
 * true when written. Its coverage is the product of three edge fades where a point or a
 * line has one distance, and the outer half of every fade falls on pixels the hardware
 * rasteriser never raises, because a fragment exists only where the pixel centre is
 * inside the triangle. The slot takes either form now, and the CPU widens the triangle
 * by a pixel about its incenter - the same widening that already turns a point and a
 * line into a quad - so the fragments exist and the shader's kill removes whatever the
 * widening added beyond the fade.
 *
 * **The sample is a row average across the slanted edge, not one pixel.** A single
 * pixel's coverage depends on exactly where the edge falls inside it, which is a
 * rounding argument this check should not be making; the average over a span that the
 * edge crosses is a number both paths agree on, and an aliased edge cannot produce it -
 * aliased, every pixel in the span is either white or black, and the mean lands at one
 * end or the other. A partly-covered span averages in between.
 *
 * The triangle is a right one with its slant across the middle of the probe, so the
 * span sampled sits well away from the two axis-aligned edges and from all three
 * corners, where a product of three fades is doing something more complicated than one.
 */
static int check_polygon_smooth(void) {
    reset_view();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POLYGON_SMOOTH);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.8f, -0.8f);
    glVertex2f(0.8f, -0.8f);
    glVertex2f(-0.8f, 0.8f);
    glEnd();
    glDisable(GL_POLYGON_SMOOTH);
    glDisable(GL_BLEND);
    glClearColor(0.125f, 0.125f, 0.125f, 1.0f);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    /* The slant runs from lower right to upper left through the middle. Walking a row
     * across it and averaging the red channel gives the ramp's mean; an aliased edge
     * steps from 255 to 0 in one pixel and averages to one end of the range. */
    const uint32_t *s = scan_frame();
    if (!s)
        return 0;
    const int y = PROBE_H / 2;
    int lo = -1, hi = -1;
    for (int x = 0; x < PROBE_W; x++) {
        const int r = (int)chan_r(SCAN_PX(s, x, y));
        if (r > 250 && lo < 0)
            lo = x;
        if (r > 250)
            hi = x;
    }
    if (lo < 0 || hi <= lo)
        return 0;
    /* Six pixels either side of where the solid run ends - the edge and its fade are in
     * there. */
    int sum = 0, n = 0;
    for (int x = hi - 2; x <= hi + 4 && x < PROBE_W; x++) {
        if (x < 0)
            continue;
        sum += (int)chan_r(SCAN_PX(s, x, y));
        n++;
    }
    if (n < 5)
        return 0;
    const int mean = sum / n;
    /* Aliased, that span is white up to the edge and black after it, and with the edge
     * at a half-diagonal the mean sits near one extreme. Smoothed, the ramp pulls it to
     * the middle. */
    return mean > 40 && mean < 215;
}

/* **The same point, textured** - `GL_POINT_SMOOTH` with a white 1x1 texture modulating
 * it, which changes nothing about the colour and everything about where the coverage
 * can ride.
 *
 * The console drew this aliased until 2026-09-21: the offset from the centre rode in
 * the texture coordinate, and a textured draw reads all four of its components, so such
 * a primitive got the square and not the disc. It now rides in the *second* texture
 * unit's parameter - spare here, because only one unit is bound - and the textured
 * pixel shader reads it from `attr3`
 * (`oops-sdk/tools/shader/coverage-tex.s`). The draw escalates to four parameters for
 * it.
 *
 * The corner is the whole check, exactly as in `smooth`: a coverage that came out as
 * one everywhere, or an offset read from an interpolant nothing wrote, passes at the
 * centre and fails 2 pixels out. Both are sampled against the same tolerances the
 * untextured check uses, so a difference between the two rows is a difference in the
 * path and not in the measurement. */
static int check_smooth_textured(void) {
    reset_view();
    static const GLubyte white[4] = {255, 255, 255, 255};
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POINT_SMOOTH);
    glPointSize(6.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
    glTexCoord2f(0.5f, 0.5f);
    glBegin(GL_POINTS);
    glVertex2f(0.0f, 0.0f);
    glEnd();
    glDisable(GL_POINT_SMOOTH);
    glPointSize(1.0f);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glDeleteTextures(1, &t);
    glClearColor(0.125f, 0.125f, 0.125f, 1.0f);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 255, 255, 255, 16) &&
           near_rgb(px(PROBE_W / 2 + 2, PROBE_H / 2 + 2), 0, 0, 0, 16);
}

/* GL_COMBINE (GL 1.3): a white texture GL_SUBTRACT a quarter-grey fragment colour,
 * 0.75, on the left; a +x normal-map texel GL_DOT3_RGB a +x light vector, white, on the
 * right. On the console since 2026-09-19 as a program in the pixel shader's longer
 * combine slot, unmeasured - it modulated there before, which reads as 0.25 grey and a
 * pink. */
static int check_combine(void) {
    reset_view();
    static const GLubyte white[4] = {255, 255, 255, 255},
                         normal_x[4] = {255, 128, 128, 255};
    GLuint t[2] = {0, 0};
    glGenTextures(2, t);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glBindTexture(GL_TEXTURE_2D, t[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_SUBTRACT);
    glColor3f(0.25f, 0.25f, 0.25f);
    glRectf(-0.9f, -0.5f, -0.1f, 0.5f);
    glBindTexture(GL_TEXTURE_2D, t[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 normal_x);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_DOT3_RGB);
    glColor3f(1.0f, 0.5f, 0.5f);
    glRectf(0.1f, -0.5f, 0.9f, 0.5f);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(2, t);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    return near_rgb(px(PROBE_W / 4, PROBE_H / 2), 191, 191, 191, 16) &&
           near_rgb(px(PROBE_W * 3 / 4, PROBE_H / 2), 255, 255, 255, 16);
}

/*
 * **Sixteen textures live in one frame, each drawn once** - the one condition every
 * other check here avoids and the one a real port meets immediately.
 *
 * Every texture arm in this suite binds one texture, or two, and draws a handful of
 * times. A fault that needs *many* textures resident in a single submit is invisible to
 * all of them, and that is not a hypothetical: Neverball uses about fourteen textures a
 * frame and renders its floor with another object's image while 118 checks pass on the
 * same hardware. Its descriptors, slots, addresses, dimensions, pitches, formats, wrap
 * modes and filters have all been read back off the console and are all correct. What
 * has never been tested is the count.
 *
 * So this tests the count and as little else as possible. Each texture is a **flat
 * colour**, so the answer cannot depend on filtering, minification, mip selection or
 * texture coordinates - every texel of texture `i` is the same, and any sample of it is
 * `i`'s colour or it is not. `GL_REPLACE` keeps the fragment colour out of it. Sixteen
 * quads in a four-by-four grid, one draw each, one frame.
 *
 * The red channel alone identifies a texture - `8 + 16 * i`, sixteen apart - so a quad
 * wearing the wrong image names *which* image it took, rather than merely failing. That
 * is the difference between this reproducing the port's fault and this just going red.
 *
 * 128x128 rather than 2x2 like the rest of the suite: a texture of a few bytes shares
 * no allocation boundary with anything, and "reads past its own storage into the next
 * texture" is one of the shapes this is looking for.
 */
#define PROBE_MANY_TEX 16

static int check_many_textures(void) {
    reset_view();
    GLuint t[PROBE_MANY_TEX];
    for (int i = 0; i < PROBE_MANY_TEX; i++)
        t[i] = 0;
    glGenTextures(PROBE_MANY_TEX, t);

    /* Flat colours, sixteen apart in red so one channel names the texture. */
    static GLubyte texel[128 * 128 * 3];
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (int i = 0; i < PROBE_MANY_TEX; i++) {
        const GLubyte r = (GLubyte)(8 + 16 * i);
        const GLubyte g = (GLubyte)(248 - 16 * i);
        const GLubyte b = (GLubyte)128;
        for (int p = 0; p < 128 * 128; p++) {
            texel[p * 3 + 0] = r;
            texel[p * 3 + 1] = g;
            texel[p * 3 + 2] = b;
        }
        glBindTexture(GL_TEXTURE_2D, t[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 128, 128, 0, GL_RGB, GL_UNSIGNED_BYTE,
                     texel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor3f(1.0f, 1.0f, 1.0f);

    /* All sixteen in one frame, each its own draw, in the order they were made. */
    for (int i = 0; i < PROBE_MANY_TEX; i++) {
        const float x0 = -0.9f + 0.45f * (float)(i % 4);
        const float y0 = -0.9f + 0.45f * (float)(i / 4);
        const float x1 = x0 + 0.42f, y1 = y0 + 0.42f;
        glBindTexture(GL_TEXTURE_2D, t[i]);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(x0, y0, 0.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(x1, y0, 0.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(x1, y1, 0.0f);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(x0, y1, 0.0f);
        glEnd();
    }
    glDisable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(PROBE_MANY_TEX, t);
        return 0;
    }

    int ok = 1;
    for (int i = 0; i < PROBE_MANY_TEX; i++) {
        const float x0 = -0.9f + 0.45f * (float)(i % 4);
        const float y0 = -0.9f + 0.45f * (float)(i / 4);
        const float cx = x0 + 0.21f, cy = y0 + 0.21f;
        const int pxx = (int)(((cx + 1.0f) * 0.5f) * (float)PROBE_W);
        /* **Rows run top-down**, so a y that is up in clip space is down in the
         * readback. */
        const int pxy = (int)((1.0f - (cy + 1.0f) * 0.5f) * (float)PROBE_H);
        const uint32_t got = px(pxx, pxy);
        const int want_r = 8 + 16 * i;
        const int d = chan_r(got) - want_r;
        if (d > 6 || d < -6)
            ok = 0;
    }
    glDeleteTextures(PROBE_MANY_TEX, t);
    return ok;
}

/* GL_BLEND and GL_DECAL of an RGBA texture - GL 1.0's two texture functions the
 * console's four combine words could not hold, and which modulated there until
 * 2026-09-19.
 *
 * Left, GL_BLEND: texel (0, 0.5, 1) over a green fragment towards a blue environment
 * colour, c (1 - t) + k t = (0, 0.5, 1); modulate would be (0, 0.5, 0). Right,
 * GL_DECAL: a red texel of alpha 0.5 over a blue fragment, c (1 - a) + t a = (0.5, 0,
 * 0.5); modulate would be black. */
static int check_tex_env_blend_decal(void) {
    reset_view();
    static const GLubyte cyanish[4] = {0, 128, 255, 255},
                         half_red[4] = {255, 0, 0, 128};
    static const GLfloat blue[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    GLuint t[2] = {0, 0};
    glGenTextures(2, t);
    glEnable(GL_TEXTURE_2D);
    glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, blue);
    glBindTexture(GL_TEXTURE_2D, t[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 cyanish);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
    glColor3f(0.0f, 1.0f, 0.0f);
    glRectf(-0.9f, -0.5f, -0.1f, 0.5f);
    glBindTexture(GL_TEXTURE_2D, t[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 half_red);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    glColor3f(0.0f, 0.0f, 1.0f);
    glRectf(0.1f, -0.5f, 0.9f, 0.5f);
    static const GLfloat black[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, black);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(2, t);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    return near_rgb(px(PROBE_W / 4, PROBE_H / 2), 0, 127, 255, 16) &&
           near_rgb(px(PROBE_W * 3 / 4, PROBE_H / 2), 128, 0, 127, 16);
}

/* Two texture units (GL 1.3's minimum; oops-gl had one until 2026-09-19): unit 0
 * replaces with a red texel, unit 1 adds a blue one, so the quad is magenta. **Expected
 * to fail on the console**, which applies unit 0 alone until its pixel shader takes a
 * second coordinate - red there, with one log line. */
static int check_multitexture(void) {
    reset_view();
    static const GLubyte red[4] = {255, 0, 0, 255}, blue[4] = {0, 0, 255, 255};
    GLuint t[2] = {0, 0};
    glGenTextures(2, t);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, t[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, red);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, t[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, blue);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
    glEnable(GL_TEXTURE_2D);
    glRectf(-0.5f, -0.5f, 0.5f, 0.5f);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(2, t);
    GLint units = 0;
    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &units);
    if (glGetError() != GL_NO_ERROR || units < 2)
        return 0;
    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 255, 0, 255, 16);
}

/*
 * **Unit 1 textured with unit 0 disabled - which is what Neverball does, and what
 * nothing here asked.**
 *
 * `check_multitexture` above enables *both* units, so it never posed this question, and
 * the console passed 86 checks while Neverball drew its whole world untextured.
 * Neverball's `tex_env_shadow` maps `GL_TEXTURE0` to the shadow stage and `GL_TEXTURE1`
 * to the surface texture, and the shadow stage begins with `glDisable(GL_TEXTURE_2D)`.
 * `tex_env_select` picks that configuration whenever `GL_MAX_TEXTURE_UNITS` is at least
 * 2, which is what oops-gl honestly reports.
 *
 * GL does not require unit 0 to be textured for unit 1 to apply: a disabled unit passes
 * the fragment colour through, so unit 1's `GL_PREVIOUS` is simply the primary colour.
 * The console's hardware path assumes otherwise - `unit1_applied` in `gl_draw.c`
 * requires unit 0 to have a texture - and drops unit 1 from the draw, which it says in
 * the log.
 *
 * `GL_REPLACE` on the only enabled unit means the fragment is the texel. A red primary
 * colour is chosen so the failure is loud: red is what comes through when unit 1 is
 * dropped.
 */
static int check_texture_unit1_alone(void) {
    reset_view();
    static const GLubyte green[4] = {0, 255, 0, 255};
    GLuint t = 0;
    glGenTextures(1, &t);

    /* Unit 0 is deliberately untouched: nothing bound, GL_TEXTURE_2D off. */
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, green);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glEnable(GL_TEXTURE_2D);

    glColor3f(1.0f, 0.0f, 0.0f);
    glRectf(-0.5f, -0.5f, 0.5f, 0.5f);

    glDisable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glDeleteTextures(1, &t);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    /* Green: unit 1 applied. Red: it was dropped and the primary colour came through.
     */
    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 0, 255, 0, 16);
}

/*
 * **Unit 1 as the base, with a texture that has more than one texel and coordinates
 * from an array** - which is how Neverball draws a surface whenever the ball's shadow
 * is off.
 *
 * `check_texture_unit1_alone` above passes on the console and proves less than it
 * looks. Its texture is 1x1, so every coordinate samples the same texel: a draw that
 * lost the coordinate entirely, or read the wrong unit's, would still come out green. A
 * whole level came out flat while that check was passing.
 *
 * Two texels and a coordinate per vertex is the smallest thing that can tell those
 * apart. Unit 0 is left disabled so unit 1 is the base unit, and the coordinates arrive
 * through `glClientActiveTexture(GL_TEXTURE1)` and `glTexCoordPointer` rather than
 * `glMultiTexCoord`, because a vertex array is what `sol_draw` uses and the two reach
 * the vertex by different code.
 *
 * Two quads, each with one coordinate at all four corners, sampling different texels of
 * the same texture: left must be red, right must be green. Both red is a coordinate
 * that never arrived - the texel at the origin is what a zeroed coordinate reads. Both
 * the primary colour is unit 1 dropped.
 */
/* **The point sprite** (GL_ARB_point_sprite, GL 2.0): a point rasterised with s and t
 * generated across its own square rather than interpolated from its single vertex.
 *
 * The distinction is invisible to a one-texel texture, which is how this went
 * unnoticed: without GL_COORD_REPLACE every fragment of the point samples the one texel
 * the vertex named, and a uniform texture looks identical either way. So the texture
 * here is 2x2 with four different colours, drawn as one large point, and the four
 * quadrants are read separately - a sprite that ignored the generated coordinates comes
 * out a single flat colour and fails on three of them.
 *
 * **t runs downward.** GL_POINT_SPRITE_COORD_ORIGIN defaults to GL_UPPER_LEFT, which is
 * the opposite of every other y in GL, so the texel at t=0 lands at the *top* of the
 * point. The expectations below are written that way round on purpose; reading the
 * image with the other convention is what would make a correct implementation look
 * broken.
 *
 * Neverball is why this exists: `solid_draw.c` and `part.c` enable point sprites for
 * every frame that draws particles, and this library refused all of it until
 * 2026-09-22. */
static int check_point_sprite(void) {
    reset_view();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-(double)PROBE_W / 2.0, (double)PROBE_W / 2.0, -(double)PROBE_H / 2.0,
            (double)PROBE_H / 2.0, -10.0, 10.0);
    glMatrixMode(GL_MODELVIEW);
    /* Row-major from the bottom left, as glTexImage2D takes it: (0,0) red, (1,0) green,
       (0,1) blue, (1,1) white. */
    static const GLubyte texels[4][4] = {
        {255, 0, 0, 255},
        {0, 255, 0, 255},
        {0, 0, 255, 255},
        {255, 255, 255, 255},
    };
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glEnable(GL_TEXTURE_2D);

    glEnable(GL_POINT_SPRITE);
    glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
    glPointSize(64.0f);
    /* The vertex names the bottom-left texel. A sprite must ignore it for all but one
       quadrant; a non-sprite point would paint the whole square red. */
    glTexCoord2f(0.25f, 0.25f);
    glBegin(GL_POINTS);
    glVertex3f(0.0f, 0.0f, -4.0f);
    glEnd();
    glPointSize(1.0f);
    glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_FALSE);
    glDisable(GL_POINT_SPRITE);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &t);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    /* A quarter of the way into each quadrant of the 64-pixel square. `px` reads with y
       downward, and so does the generated t, so the two agree: the top half is t<0.5,
       which is texel row 0
       - red and green - and the bottom half is row 1, blue and white. */
    const int cx = PROBE_W / 2, cy = PROBE_H / 2;
    return near_rgb(px(cx - 16, cy - 16), 255, 0, 0, 16) &&
           near_rgb(px(cx + 16, cy - 16), 0, 255, 0, 16) &&
           near_rgb(px(cx - 16, cy + 16), 0, 0, 255, 16) &&
           near_rgb(px(cx + 16, cy + 16), 255, 255, 255, 16);
}

static int check_texture_unit1_coords(void) {
    reset_view();
    /* Row-major from the bottom left: (0,0) red, (1,0) green, and two the draw must not
     * pick. */
    static const GLubyte texels[4][4] = {
        {255, 0, 0, 255},
        {0, 255, 0, 255},
        {0, 0, 255, 255},
        {255, 255, 255, 255},
    };
    static const GLfloat pos_left[8] = {-0.8f, -0.5f, -0.2f, -0.5f,
                                        -0.8f, 0.5f,  -0.2f, 0.5f};
    static const GLfloat pos_right[8] = {0.2f, -0.5f, 0.8f, -0.5f,
                                         0.2f, 0.5f,  0.8f, 0.5f};
    /* One coordinate repeated at all four corners, so the quad is a flat sample of one
     * texel and the verdict is a colour rather than a gradient. */
    static const GLfloat tc_left[8] = {0.25f, 0.25f, 0.25f, 0.25f,
                                       0.25f, 0.25f, 0.25f, 0.25f};
    static const GLfloat tc_right[8] = {0.75f, 0.25f, 0.75f, 0.25f,
                                        0.75f, 0.25f, 0.75f, 0.25f};

    GLuint t = 0;
    glGenTextures(1, &t);

    /* Unit 0 deliberately untouched: disabled, nothing bound, so unit 1 is the base
     * unit. */
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glEnable(GL_TEXTURE_2D);

    /* Magenta: a colour neither texel carries, so "unit 1 dropped" is its own answer.
     */
    glColor3f(1.0f, 0.0f, 1.0f);

    glEnableClientState(GL_VERTEX_ARRAY);
    glClientActiveTexture(GL_TEXTURE1);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glVertexPointer(2, GL_FLOAT, 0, pos_left);
    glTexCoordPointer(2, GL_FLOAT, 0, tc_left);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glVertexPointer(2, GL_FLOAT, 0, pos_right);
    glTexCoordPointer(2, GL_FLOAT, 0, tc_right);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glClientActiveTexture(GL_TEXTURE0);
    glDisableClientState(GL_VERTEX_ARRAY);

    glDisable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glDeleteTextures(1, &t);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    return near_rgb(px(PROBE_W / 4, PROBE_H / 2), 255, 0, 0, 16) &&
           near_rgb(px(3 * PROBE_W / 4, PROBE_H / 2), 0, 255, 0, 16);
}

/*
 * **Neverball's shadow arrangement as it actually runs, with both units textured.**
 *
 * `check_texture_unit1_alone` above was written from `tex_env_conf_shadow`, which opens
 * the shadow stage with `glDisable(GL_TEXTURE_2D)`, and so posed the question as unit 0
 * *disabled*. That is only half of it: `shad_draw_set` runs afterwards and enables
 * `GL_TEXTURE_2D` on that same unit with the ball's shadow image bound. So a surface
 * whose material carries the `shadowed` flag is drawn with **two textured units** -
 * unit 0 the shadow under `GL_COMBINE`, unit 1 the surface's own texture under
 * `GL_MODULATE` - and a surface without that flag is drawn with one.
 *
 * That flag is exactly the split seen on the console: every material in
 * `data/textures/mtrl` whose file says `flags shadowed` came out flat, and the coin (no
 * flags) and the ball (`transparent`) came out textured. So this is the arrangement to
 * measure, and nothing here measured it - `check_multitexture` enables both units but
 * leaves both environments at `GL_MODULATE`, which never exercises a combine feeding a
 * second stage.
 *
 * The combine is Neverball's own, copied from `tex_env_conf_shadow`: colour is
 * `PREVIOUS * (1 - texture.alpha)`, alpha is `REPLACE` from `PREVIOUS`. The shadow
 * texel here has
 * **alpha 0** - the unshadowed case, which is almost all of a level's surface area - so
 * unit 0's stage must pass the primary colour through unchanged, and unit 1 must then
 * modulate it by the surface texture. Its RGB is red, a colour it must *not*
 * contribute: `GL_ONE_MINUS_SRC_ALPHA` reads only the alpha, so red leaking into the
 * result means the operand was ignored.
 *
 * White primary, green surface texture, so the three failures are distinguishable:
 *   green - correct; red - unit 0's texel leaked through its combine;
 *   white - unit 1 was dropped and only the pass-through survived.
 */
/* **The shadow stack, weighted** - what `texture-env-shadow-stack` below cannot see.
 *
 * That check uses a shadow texel of alpha 0, so `GL_ONE_MINUS_SRC_ALPHA` is 1 and unit
 * 0 multiplies by exactly one. A path that dropped unit 0 altogether produces the same
 * green, so it passes either way: it proves the surface texture survives and proves
 * nothing at all about the shadow. That is the same shape of hole `texture-unit1-alone`
 * had against a 1x1 texture, found the same way - by asking what a *wrong*
 * implementation would print.
 *
 * So this one varies the two things that check holds constant, and reads three quads:
 *
 *   left   shadow alpha 0,   surface green  -> green   (unshadowed)
 *   middle shadow alpha 255, surface green  -> black   (fully shadowed)
 *   right  shadow alpha 0,   surface blue   -> blue    (unit 1's own coordinate)
 *
 * Left against middle is the one that matters: identical everywhere except the shadow
 * texel's alpha, so a draw that ignores unit 0 renders both green and fails. Left
 * against right proves unit 1 samples where it was told rather than somewhere fixed.
 * And the shadow texture is red throughout, a colour that appears in no correct output,
 * so leaking its RGB is a visible failure rather than a plausible one.
 *
 * Neverball renders every surface through this arrangement (`tex_env_shadow`,
 * `geom.c:50`). */
static int check_texture_env_shadow_weight(void) {
    reset_view();
    /* Row-major from the bottom left. Red throughout; only the alpha is meant to be
     * read. */
    static const GLubyte shadow[4][4] = {
        {255, 0, 0, 0},
        {255, 0, 0, 255},
        {255, 0, 0, 128},
        {255, 0, 0, 0},
    };
    static const GLubyte surface[4][4] = {
        {0, 255, 0, 255},
        {0, 0, 255, 255},
        {255, 255, 255, 255},
        {255, 255, 255, 255},
    };
    /* Three columns, each a triangle strip of four corners. */
    static const GLfloat pos[3][8] = {
        {-0.90f, -0.5f, -0.60f, -0.5f, -0.90f, 0.5f, -0.60f, 0.5f},
        {-0.15f, -0.5f, 0.15f, -0.5f, -0.15f, 0.5f, 0.15f, 0.5f},
        {0.60f, -0.5f, 0.90f, -0.5f, 0.60f, 0.5f, 0.90f, 0.5f},
    };
    /* s = 0.25 is texel 0, s = 0.75 is texel 1; t = 0.25 stays on the bottom row. */
    static const GLfloat tc_shadow[3][8] = {
        {0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f}, /* alpha 0   */
        {0.75f, 0.25f, 0.75f, 0.25f, 0.75f, 0.25f, 0.75f, 0.25f}, /* alpha 255 */
        {0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f}, /* alpha 0   */
    };
    static const GLfloat tc_surface[3][8] = {
        {0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f}, /* green */
        {0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f}, /* green */
        {0.75f, 0.25f, 0.75f, 0.25f, 0.75f, 0.25f, 0.75f, 0.25f}, /* blue  */
    };
    GLuint t[2] = {0, 0};
    glGenTextures(2, t);

    /* Unit 0, the shadow stage - `tex_env_conf_shadow(TEX_STAGE_SHADOW, 1)` verbatim.
     */
    glActiveTexture(GL_TEXTURE0);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PREVIOUS);
    glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_PREVIOUS);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, t[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, shadow);
    glEnable(GL_TEXTURE_2D);

    /* Unit 1, the surface texture - `tex_env_conf_default(TEX_STAGE_TEXTURE, 1)`. */
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, t[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 surface);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_TEXTURE_2D);

    glColor3f(1.0f, 1.0f, 1.0f);
    glEnableClientState(GL_VERTEX_ARRAY);
    glClientActiveTexture(GL_TEXTURE0);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glClientActiveTexture(GL_TEXTURE1);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    for (int q = 0; q < 3; q++) {
        glVertexPointer(2, GL_FLOAT, 0, pos[q]);
        glClientActiveTexture(GL_TEXTURE0);
        glTexCoordPointer(2, GL_FLOAT, 0, tc_shadow[q]);
        glClientActiveTexture(GL_TEXTURE1);
        glTexCoordPointer(2, GL_FLOAT, 0, tc_surface[q]);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    glClientActiveTexture(GL_TEXTURE1);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glClientActiveTexture(GL_TEXTURE0);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);

    glActiveTexture(GL_TEXTURE1);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDeleteTextures(2, t);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    const int cy = PROBE_H / 2;
    return near_rgb(px(PROBE_W / 8, cy), 0, 255, 0, 16) &&
           near_rgb(px(PROBE_W / 2, cy), 0, 0, 0, 16) &&
           near_rgb(px(7 * PROBE_W / 8, cy), 0, 0, 255, 16);
}

static int check_texture_env_shadow_stack(void) {
    reset_view();
    /* Alpha 0: unshadowed. Red so that a leak of the operand is visible rather than
     * silent. */
    static const GLubyte shadow[4] = {255, 0, 0, 0};
    static const GLubyte green[4] = {0, 255, 0, 255};
    GLuint t[2] = {0, 0};
    glGenTextures(2, t);

    /* Unit 0, the shadow stage: tex_env_conf_shadow(TEX_STAGE_SHADOW, 1) ... */
    glActiveTexture(GL_TEXTURE0);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PREVIOUS);
    glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_PREVIOUS);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
    /* ... and then shad_draw_set(), which enables the unit and binds the shadow image.
     */
    glBindTexture(GL_TEXTURE_2D, t[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, shadow);
    glEnable(GL_TEXTURE_2D);

    /* Unit 1, the surface texture: tex_env_conf_default(TEX_STAGE_TEXTURE, 1). */
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, t[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, green);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_TEXTURE_2D);

    glColor3f(1.0f, 1.0f, 1.0f);
    glMultiTexCoord2f(GL_TEXTURE0, 0.5f, 0.5f);
    glMultiTexCoord2f(GL_TEXTURE1, 0.5f, 0.5f);
    glRectf(-0.5f, -0.5f, 0.5f, 0.5f);

    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDeleteTextures(2, t);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 0, 255, 0, 16);
}

/* A cube map (GL 1.3): six 1x1 faces, looked up along -z by a texture coordinate and
 * along the eye-space normal by GL_NORMAL_MAP generation - cyan both times. **Expected
 * to pass on both paths since 2026-09-20**, when the console gained the hardware half:
 * the six faces uploaded as one array, `TYPE 0xb` in the descriptor, and the lookup by
 * direction done in the pixel shader with RDNA2's own cube instructions. It drew white
 * there before that, untextured. This check looks along -z and by the normal, so a
 * face-order mistake shows as the wrong colour rather than as no colour. */
static int check_cube_map(void) {
    reset_view();
    static const GLubyte faces[6][4] = {
        {255, 0, 0, 255},   {0, 255, 0, 255},   {0, 0, 255, 255},
        {255, 255, 0, 255}, {255, 0, 255, 255}, {0, 255, 255, 255},
    };
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_CUBE_MAP, t);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (int f = 0; f < 6; f++) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)f, 0, GL_RGBA, 1, 1, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, faces[f]);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glEnable(GL_TEXTURE_CUBE_MAP);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor3f(1.0f, 1.0f, 1.0f);
    glTexCoord3f(0.1f, -0.2f, -1.0f);
    glRectf(-0.9f, -0.5f, -0.1f, 0.5f);
    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_NORMAL_MAP);
    glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_NORMAL_MAP);
    glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_NORMAL_MAP);
    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);
    glEnable(GL_TEXTURE_GEN_R);
    glNormal3f(0.0f, 0.0f, -1.0f);
    glRectf(0.1f, -0.5f, 0.9f, 0.5f);
    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);
    glDisable(GL_TEXTURE_GEN_R);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glTexCoord4f(0.0f, 0.0f, 0.0f, 1.0f);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glDeleteTextures(1, &t);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    return near_rgb(px(PROBE_W / 4, PROBE_H / 2), 0, 255, 255, 16) &&
           near_rgb(px(PROBE_W * 3 / 4, PROBE_H / 2), 0, 255, 255, 16);
}

/* A red 4x4, green 2x2, blue 1x1 chain - or only its red base - in a column of the
 * probe area. */
static void lod_column(GLuint t, int levels, GLenum min_filter, GLenum pname,
                       GLint ival, GLfloat fval, float x0) {
    static GLubyte red[4 * 4 * 4], green[2 * 2 * 4], blue[4];
    for (int i = 0; i < 16; i++) {
        red[i * 4] = 255;
        red[i * 4 + 3] = 255;
    }
    for (int i = 0; i < 4; i++) {
        green[i * 4 + 1] = 255;
        green[i * 4 + 3] = 255;
    }
    blue[2] = 255;
    blue[3] = 255;
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, red);
    if (levels > 1) {
        glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     green);
        glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     blue);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint)min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    if (pname == GL_TEXTURE_MIN_LOD)
        glTexParameterf(GL_TEXTURE_2D, pname, fval);
    else
        glTexParameteri(GL_TEXTURE_2D, pname, ival);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(x0, -0.8f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(x0 + 0.6f, -0.8f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(x0 + 0.6f, 0.8f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(x0, 0.8f);
    glEnd();
}

/* GL 1.2's level-of-detail parameters, refused until 2026-09-19, each on a magnified
 * texture:
 * - left, **GL_TEXTURE_BASE_LEVEL 1** under GL_NEAREST: green. The hardware samples a
 * chain of one level built from level 1, where it sampled level 0's own storage.
 * - middle, **GL_TEXTURE_MAX_LEVEL 0** on a single-level texture under a mipmap filter:
 * red. It was incomplete, and drew in the vertex colour, white.
 * - right, **GL_TEXTURE_MIN_LOD 2** under GL_NEAREST_MIPMAP_NEAREST: blue - the
 * sampler's MIN_LOD field, in 4.8 fixed point, holding a magnified draw at level 2. */
static int check_lod_params(void) {
    reset_view();
    GLuint t[3] = {0, 0, 0};
    glGenTextures(3, t);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    lod_column(t[0], 3, GL_NEAREST, GL_TEXTURE_BASE_LEVEL, 1, 0.0f, -0.95f);
    lod_column(t[1], 1, GL_NEAREST_MIPMAP_NEAREST, GL_TEXTURE_MAX_LEVEL, 0, 0.0f,
               -0.3f);
    lod_column(t[2], 3, GL_NEAREST_MIPMAP_NEAREST, GL_TEXTURE_MIN_LOD, 0, 2.0f, 0.35f);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(3, t);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    /* Column centres at NDC x -0.65, 0 and 0.65. */
    const int y = PROBE_H / 2;
    return near_rgb(px(PROBE_W * 35 / 200, y), 0, 255, 0, 16) &&
           near_rgb(px(PROBE_W / 2, y), 255, 0, 0, 16) &&
           near_rgb(px(PROBE_W * 165 / 200, y), 0, 0, 255, 16);
}

/* A lit quad over the middle of the area, facing the light. */
static void lit_rect(float nz) {
    glNormal3f(0.0f, 0.0f, nz);
    glBegin(GL_QUADS);
    glVertex2f(-0.5f, -0.5f);
    glVertex2f(0.5f, -0.5f);
    glVertex2f(0.5f, 0.5f);
    glVertex2f(-0.5f, 0.5f);
    glEnd();
}

/* One white directional light along +z, diffuse `d` and specular `s`, nothing ambient.
 */
static void one_light(float d, float s) {
    const GLfloat dir[4] = {0.0f, 0.0f, 1.0f, 0.0f}, none[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    const GLfloat dif[4] = {d, d, d, 1.0f}, spec[4] = {s, s, s, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, dir);
    glLightfv(GL_LIGHT0, GL_AMBIENT, none);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
    glLightfv(GL_LIGHT0, GL_SPECULAR, spec);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, none);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, none);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHTING);
}

static void lighting_off(void) {
    static const GLfloat amb[4] = {0.2f, 0.2f, 0.2f, 1.0f},
                         dif[4] = {0.8f, 0.8f, 0.8f, 1.0f};
    static const GLfloat spec[4] = {0.0f, 0.0f, 0.0f, 1.0f},
                         lm[4] = {0.2f, 0.2f, 0.2f, 1.0f};
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, amb);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, dif);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 0.0f);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lm);
    glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SINGLE_COLOR);
}

/* GL 1.2's GL_RESCALE_NORMAL and the normal lengths under it, which lighting ignored
 * until 2026-09-19 by normalising every normal. Lighting is CPU work on both paths, so
 * this should pass on the console: what it measures there is that the lit colours reach
 * the vertices.
 * - glScalef(2) halves a unit normal: diffuse 1 lights it at 0.5, grey 128;
 * - GL_RESCALE_NORMAL undoes the scale: white;
 * - and a shininess of 0, full specular where it was none: a black-diffuse quad lit
 * white. */
static int check_rescale_normal(void) {
    reset_view();
    static const GLfloat white[4] = {1.0f, 1.0f, 1.0f, 1.0f},
                         black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    one_light(1.0f, 0.0f);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, white);
    glScalef(2.0f, 2.0f, 2.0f);
    lit_rect(1.0f);
    const int halved = near_rgb(px(PROBE_W / 2, PROBE_H / 2), 128, 128, 128, 16);
    glEnable(GL_RESCALE_NORMAL);
    lit_rect(1.0f);
    glDisable(GL_RESCALE_NORMAL);
    const int rescaled = near_rgb(px(PROBE_W / 2, PROBE_H / 2), 255, 255, 255, 16);
    glLoadIdentity();
    one_light(0.0f, 1.0f);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, white);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 0.0f);
    lit_rect(1.0f);
    const int specular = near_rgb(px(PROBE_W / 2, PROBE_H / 2), 255, 255, 255, 16);
    lighting_off();
    if (glGetError() != GL_NO_ERROR)
        return 0;
    return halved && rescaled && specular;
}

/* GL_SEPARATE_SPECULAR_COLOR: a white highlight on a black-textured quad, the texture
 * GL_MODULATE. Kept apart, the highlight is added after texturing and the quad is
 * white; summed first, the texture blacks it out. On the console the highlight rides in
 * the third interpolant since 2026-09-19. The textured pixel shader adds it after the
 * combine, through the interface obSCEne measured (REQ-20260919T1745Z-9c3e). This
 * should pass there now, and it is the measurement of that sum. Until then the specular
 * joined the colour per vertex, and the texture blacked it out. */
static int check_separate_specular(void) {
    reset_view();
    static const GLfloat white[4] = {1.0f, 1.0f, 1.0f, 1.0f},
                         black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    static const GLubyte texel[4] = {0, 0, 0, 255};
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texel);
    glEnable(GL_TEXTURE_2D);
    one_light(0.0f, 1.0f);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, white);
    glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
    glTexCoord2f(0.5f, 0.5f);
    lit_rect(1.0f);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &t);
    lighting_off();
    if (glGetError() != GL_NO_ERROR)
        return 0;
    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 255, 255, 255, 16);
}

/* glClear through a scissor box and a colour mask, which it ignored until 2026-09-19.
 * Such a clear is now drawn - a quad at the clear colour, through the scissor registers
 * and CB_TARGET_MASK - where an unscissored, unmasked one is still the DMA fill; this
 * measures the drawn kind on a console. Green into the lower-left quarter only, then
 * blue through a mask keeping red: the upper right stays red and gains blue, magenta.
 */
static int check_scissored_clear(void) {
    reset_view();
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, PROBE_W / 2, PROBE_H / 2);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.125f, 0.125f, 0.125f, 1.0f);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    /* px counts rows down from the top: row PROBE_H - 8 is near the bottom, row 8 near
     * the top. */
    const int lower_left =
        near_rgb(px(8, PROBE_H - 8), 0, 0, 255, 16); /* green cleared by the blue */
    const int upper_right = near_rgb(px(PROBE_W - 8, 8), 255, 0, 255, 16);
    return lower_left && upper_right;
}

/* The accumulation buffer, as motion blur uses it: a red frame and a blue frame added
 * in at half each and returned - half red, half blue. Every read goes through the flush
 * and readback that glReadPixels uses and the return is a CPU write like glDrawPixels',
 * so on a console this measures that the GPU's frames reach the buffer and the result
 * reaches the screen. */
static int check_accumulation(void) {
    reset_view();
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_ACCUM_BUFFER_BIT);
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
    glAccum(GL_ACCUM, 0.5f);
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f);
    glAccum(GL_ACCUM, 0.5f);
    glAccum(GL_RETURN, 1.0f);
    glClearColor(0.125f, 0.125f, 0.125f, 1.0f);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 128, 0, 128, 8);
}

/* Line stipple: a line across the area's 128 columns under 0x00ff at factor 4 - 32
 * columns on, 32 off. The dashes are cut on the CPU and drawn as ordinary quads, so
 * this should pass on the console as it does here. Window row 48, the line's, is px
 * row 47. */
static int check_line_stipple(void) {
    reset_view();
    glLineWidth(3.0f);
    glEnable(GL_LINE_STIPPLE);
    glLineStipple(4, 0x00ff);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_LINES);
    glVertex2f(-1.0f, 0.0f);
    glVertex2f(1.0f, 0.0f);
    glEnd();
    glDisable(GL_LINE_STIPPLE);
    glLineStipple(1, 0xffff);
    glLineWidth(1.0f);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    const int y = PROBE_H - 1 - PROBE_H / 2;
    return near_rgb(px(10, y), 255, 255, 255, 16) &&
           near_rgb(px(40, y), 0x20, 0x20, 0x20, 8) &&
           near_rgb(px(70, y), 255, 255, 255, 16) &&
           near_rgb(px(100, y), 0x20, 0x20, 0x20, 8);
}

/* Polygon stipple: a checkerboard mask over a white rectangle filling the area.
 * **Expected to pass on both paths since 2026-09-20**, when the pixel shaders got the
 * discard: the draw asks for the fragment's window position (`SPI_PS_INPUT_ENA` 0x302,
 * obSCEne's `REQ-20260919T2258Z-c7d4`) and looks the mask up in a 32-row table the CPU
 * writes beside the shaders. It failed on the console before that, the software
 * rasteriser applying the mask and the hardware path ignoring it. Window (0, 0) keeps
 * its fragment, (1, 0) loses it; px row 95 is window 0 - so this also catches the
 * rotation being dropped, which would stipple the polygon with the mask upside down. */
static int check_polygon_stipple(void) {
    reset_view();
    static GLubyte mask[128];
    for (int r = 0; r < 32; r++) {
        for (int c = 0; c < 4; c++)
            mask[r * 4 + c] = (GLubyte)((r & 1) ? 0x55 : 0xaa);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPolygonStipple(mask);
    glEnable(GL_POLYGON_STIPPLE);
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_POLYGON_STIPPLE);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    return near_rgb(px(0, PROBE_H - 1), 255, 255, 255, 16) &&
           near_rgb(px(1, PROBE_H - 1), 0x20, 0x20, 0x20, 8);
}

/* The texture matrix, which reached nothing until 2026-09-19. A red | green texture on
 * a quad whose s runs 0..0.49 - red - drawn again with the texture matrix moving s on
 * by a half, which must turn it green. The coordinates are computed on the CPU and
 * sampled by the console, so this is the hardware's view of the fix. */
static int check_texture_matrix(void) {
    reset_view();
    static const GLubyte texels[8] = {255, 0, 0, 255, 0, 255, 0, 255};
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    glEnable(GL_TEXTURE_2D);
    glColor3f(1.0f, 1.0f, 1.0f);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslatef(0.5f, 0.0f, 0.0f);
    glMatrixMode(GL_MODELVIEW);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-0.5f, -0.5f);
    glTexCoord2f(0.49f, 0.0f);
    glVertex2f(0.5f, -0.5f);
    glTexCoord2f(0.49f, 1.0f);
    glVertex2f(0.5f, 0.5f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-0.5f, 0.5f);
    glEnd();
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &t);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 0, 255, 0, 16);
}

/* A depth range changed between two draws of one frame.
 *
 * `depth-range` cannot see this: it samples between its two halves, and a sample
 * submits the frame, so each half starts a fresh one whose register table already
 * carries the new range. Here nothing samples until both quads are down. Red is drawn
 * with the range 0.5..1, green on top of it with 0..0.5, both at z = 0 - so green lands
 * at window depth 0.25 against red's 0.75 and GL_LESS keeps green. If the second range
 * never reached the hardware, both are at 0.75 and GL_LESS keeps red.
 */
static int check_depth_range_in_frame(void) {
    reset_view();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthRange(0.5, 1.0);
    draw_rect(-0.5f, -0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f);
    glDepthRange(0.0, 0.5);
    draw_rect(-0.5f, -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f);
    glDepthRange(0.0, 1.0);
    glDisable(GL_DEPTH_TEST);
    if (glGetError() != GL_NO_ERROR)
        return 0;
    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 0, 255, 0, 8);
}

/* More triangles in one frame than the vertex ring holds (450).
 *
 * A 20x15 grid of small rectangles, two triangles each - 600 triangles, all in one
 * frame. The first is green, the last red, the rest blue. Every triangle's vertices sit
 * in a ring slot until the frame runs, so if the ring wrapped without submitting,
 * triangles 450 and 451 overwrote the first rectangle's slots and the first rectangle
 * is simply not there: no green pixel anywhere. Counted rather than point-sampled, so
 * the answer does not depend on which way up the rows are.
 */
static int check_many_triangles(void) {
    reset_view();
    const int cols = 20, rows = 15;
    const float cw = 2.0f / (float)cols, ch = 2.0f / (float)rows;
    for (int k = 0; k < cols * rows; k++) {
        const float x0 = -1.0f + (float)(k % cols) * cw + cw * 0.2f;
        const float y0 = -1.0f + (float)(k / cols) * ch + ch * 0.2f;
        const float g = (k == 0) ? 1.0f : 0.0f;
        const float r = (k == cols * rows - 1) ? 1.0f : 0.0f;
        const float b = (k != 0 && k != cols * rows - 1) ? 1.0f : 0.0f;
        draw_rect(x0, y0, x0 + cw * 0.6f, y0 + ch * 0.6f, r, g, b);
    }
    if (glGetError() != GL_NO_ERROR)
        return 0;

    int green = 0, red = 0, blue = 0;
    const uint32_t *s = scan_frame();
    for (int y = 0; y < PROBE_H; y++) {
        for (int x = 0; x < PROBE_W; x++) {
            const uint32_t c = SCAN_PX(s, x, y);
            if (near_rgb(c, 0, 255, 0, 16))
                green++;
            else if (near_rgb(c, 255, 0, 0, 16))
                red++;
            else if (near_rgb(c, 0, 0, 255, 16))
                blue++;
        }
    }
    /* A cell is 6.4 px square and the rectangle 60% of it: about 3x3 pixels each. */
    return green >= 4 && red >= 4 && blue >= 298 * 4;
}

static int check_refusals(void) {
    reset_view();
    /* **The refusals are a feature and are checked like one.**
     *
     * Points and lines used to be checked here as refusals, on the grounds that a build
     * which quietly started accepting them would be claiming something the hardware had
     * denied. The distinction that note missed is what the measurement actually closed:
     * the geometry engine stalls on a one- or two-vertex *primitive*. It says nothing
     * about drawing a line, which is a screen-width quad and therefore triangles - and
     * triangles are the thing the same sweeps showed retiring and drawing.
     *
     * So they are checked as *draws* now, and `check_points_and_lines` below is where
     * that is verified properly. If the expansion is wrong on hardware, that check
     * fails and this one stays quiet, which is the right division of labour. */
    (void)glGetError();
    glBegin(GL_POINTS);
    glEnd();
    if (glGetError() != GL_NO_ERROR)
        return 0;
    glBegin(GL_LINES);
    glEnd();
    if (glGetError() != GL_NO_ERROR)
        return 0;

    /* An enum that is not a primitive mode at all is still refused. */
    glBegin((GLenum)0xdeadbeefu);
    glEnd();
    if (glGetError() != GL_INVALID_ENUM)
        return 0;

    /* A capability this subset does not have is refused rather than dropped. Written as
     * its specification value because oops-gl deliberately does not declare it - D009:
     * an absent feature is an absent symbol. GL_CONVOLUTION_1D belongs to the imaging
     * subset, which is optional and not advertised, so it stays refused; GL_DITHER, the
     * example here until 2026-09-19, is accepted as state now. */
    glEnable(0x8010u /* GL_CONVOLUTION_1D */);
    if (glGetError() != GL_INVALID_ENUM)
        return 0;

    /* And a query it cannot answer refuses rather than leaving the caller's buffer as
     * it was. */
    GLint v = 0x5eed;
    glGetIntegerv(0x8000u /* no GL 1.x query; labelled GL_FOG_HINT here until
                             2026-09-19, which is 0x0C54 */
                  ,
                  &v);
    if (glGetError() != GL_INVALID_ENUM)
        return 0;
    if (v != 0x5eed)
        return 0;
    return 1;
}

/* **Lighting, which gl1-cube exercises and nothing checked independently.**
 *
 * A directional light down -z, and two quads whose normals face towards and away from
 * it. The lit one must be brighter. The fixture matters: a light at (0,0,1) with both
 * normals differing only in sign is the smallest arrangement where a normal that is
 * ignored, normalised wrongly, or transformed by the wrong matrix gives the same
 * brightness for both.
 */
/* **GL_COLOR_MATERIAL**: with it enabled, `glColor` feeds the material rather than the
 * fragment, and the lit result changes colour without a single `glMaterialfv` call.
 *
 * Untested on hardware until now, and on the path of every lit surface Neverball draws.
 * `r_color_mtrl` (`share/solid_draw.c:838`) toggles it per material and **never calls
 * `glColorMaterial`**, so the port depends entirely on the defaults being right:
 * GL_FRONT_AND_BACK and GL_AMBIENT_AND_DIFFUSE. A `glColor` that reached the fragment
 * directly, or a tracking mode that defaulted elsewhere, gives wrong colours on lit
 * geometry with no error anywhere - which is the symptom this is chasing.
 *
 * Three quads under one directional white light, all with the same normal and no
 * material call between them:
 *
 *   left    tracking on,  glColor red    -> red      (glColor became the diffuse
 * material) middle  tracking on,  glColor blue   -> blue     (and it re-tracks when the
 * colour moves) right   tracking off, glColor blue, material green                ->
 * green    (glColor is ignored again)
 *
 * The right quad is what makes this fail against a path that simply routes `glColor` to
 * the fragment: such a path renders it blue. Left against middle catches the opposite
 * mistake, a material captured once and never updated. */
static int check_color_material(void) {
    reset_view();
    static const GLfloat pos[4] = {0.0f, 0.0f, 1.0f, 0.0f}; /* w=0: directional */
    static const GLfloat white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    static const GLfloat dim[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    static const GLfloat green[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, white);
    glLightfv(GL_LIGHT0, GL_AMBIENT, dim);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, dim);
    glNormal3f(0.0f, 0.0f, 1.0f);

    /* Tracking on, and never told what to track - the defaults are the thing under
     * test. */
    glEnable(GL_COLOR_MATERIAL);
    glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex3f(-0.95f, -0.4f, 0.0f);
    glVertex3f(-0.70f, -0.4f, 0.0f);
    glVertex3f(-0.70f, 0.4f, 0.0f);
    glVertex3f(-0.95f, 0.4f, 0.0f);
    glEnd();

    glColor4f(0.0f, 0.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex3f(-0.15f, -0.4f, 0.0f);
    glVertex3f(0.15f, -0.4f, 0.0f);
    glVertex3f(0.15f, 0.4f, 0.0f);
    glVertex3f(-0.15f, 0.4f, 0.0f);
    glEnd();

    /* Tracking off: the material speaks and the colour is ignored. `glColor` stays blue
       on purpose, so a path that leaks it renders this quad blue instead of green. */
    glDisable(GL_COLOR_MATERIAL);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, green);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, dim);
    glBegin(GL_QUADS);
    glVertex3f(0.70f, -0.4f, 0.0f);
    glVertex3f(0.95f, -0.4f, 0.0f);
    glVertex3f(0.95f, 0.4f, 0.0f);
    glVertex3f(0.70f, 0.4f, 0.0f);
    glEnd();

    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, white);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    const int cy = PROBE_H / 2;
    const uint32_t a = px(PROBE_W / 16, cy);
    const uint32_t b = px(PROBE_W / 2, cy);
    const uint32_t c = px(PROBE_W * 15 / 16, cy);
    /* Generous on the lit magnitude, strict on which channel carries it - the question
       here is where the colour came from, not how the light is scaled. */
    return chan_r(a) > 180 && chan_g(a) < 60 && chan_b(a) < 60 && chan_b(b) > 180 &&
           chan_r(b) < 60 && chan_g(b) < 60 && chan_g(c) > 180 && chan_r(c) < 60 &&
           chan_b(c) < 60;
}

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
    glVertex3f(-0.9f, -0.4f, 0.0f);
    glVertex3f(-0.1f, -0.4f, 0.0f);
    glVertex3f(-0.1f, 0.4f, 0.0f);
    glVertex3f(-0.9f, 0.4f, 0.0f);
    glEnd();
    /* Facing away. */
    glNormal3f(0.0f, 0.0f, -1.0f);
    glBegin(GL_QUADS);
    glVertex3f(0.1f, -0.4f, 0.0f);
    glVertex3f(0.9f, -0.4f, 0.0f);
    glVertex3f(0.9f, 0.4f, 0.0f);
    glVertex3f(0.1f, 0.4f, 0.0f);
    glEnd();
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    uint32_t lit = px(PROBE_W / 4, PROBE_H / 2);
    uint32_t unlit = px(PROBE_W * 3 / 4, PROBE_H / 2);
    if (chan_r(lit) < 200)
        return 0; /* the facing quad is bright */
    if (chan_r(unlit) > 60)
        return 0; /* the away-facing one is not */
    return 1;
}

/* Texture environment: GL_REPLACE ignores the vertex colour, GL_MODULATE multiplies by
 * it.
 * **The vertex colour is deliberately not white**, because with white the two modes
 * agree and the check would pass against an implementation that only ever replaced. */
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
    if (glGetError() != GL_NO_ERROR)
        return 0;

    uint32_t replaced = px(PROBE_W / 4, PROBE_H / 2);
    uint32_t modulated = px(PROBE_W * 3 / 4, PROBE_H / 2);
    /* Replace takes the white texel whole; modulate scales it by the dark red. */
    if (chan_r(replaced) < 240 || chan_g(replaced) < 240)
        return 0;
    if (chan_r(modulated) > 90 || chan_g(modulated) > 30)
        return 0;
    return 1;
}

/* Render-to-texture, which is what glCopyTexSubImage2D is for. Draws, copies the
 * framebuffer into a texture, clears, then draws the texture back and checks it
 * survived the round trip. */
static int check_copy_tex(void) {
    reset_view();
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.75f, 1.0f);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 32, 32, 0);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &tex);
        return 0;
    }

    glClear(GL_COLOR_BUFFER_BIT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-0.8f, -0.8f, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(0.8f, -0.8f, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(0.8f, 0.8f, 0.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-0.8f, 0.8f, 0.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glDeleteTextures(1, &tex);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 0, 191, 255, 16);
}

/* `glGetTexImage` reads a texture back. **Exact, not approximate**: no rasteriser is
 * involved, so anything other than the bytes that went in is a bug rather than a
 * rounding difference. */
static int check_get_tex_image(void) {
    reset_view();
    static const GLubyte src[16] = {
        10, 60, 110, 160, 11, 61, 111, 161, 12, 62, 112, 162, 13, 63, 113, 163,
    };
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, src);

    GLubyte back[16];
    for (int i = 0; i < 16; i++)
        back[i] = 0xcd;
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, back);
    glDeleteTextures(1, &tex);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    for (int i = 0; i < 16; i++) {
        if (back[i] != src[i])
            return 0;
    }
    return 1;
}

/* `glArrayElement`, `glDrawRangeElements` and `glInterleavedArrays` all draw the same
 * quad three different ways. **Each is compared against glDrawElements**, so this fails
 * if any one of them disagrees with the path that is already known to work. */
static int check_array_paths(void) {
    static const GLfloat verts[12] = {
        -0.7f, -0.7f, 0.0f, 0.7f, -0.7f, 0.0f, 0.7f, 0.7f, 0.0f, -0.7f, 0.7f, 0.0f,
    };
    static const GLushort idx[6] = {0, 1, 2, 0, 2, 3};
    static uint32_t want[PROBE_W * PROBE_H];

    reset_view();
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, verts);
    glColor3f(1.0f, 0.5f, 0.0f);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, idx);
    frame_snapshot(want);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    /* glDrawRangeElements: the range is a hint, the picture must be identical. */
    reset_view();
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, verts);
    glColor3f(1.0f, 0.5f, 0.0f);
    glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, idx);
    if (!frame_matches(want)) {
        glDisableClientState(GL_VERTEX_ARRAY);
        return 0;
    }

    /* glArrayElement inside glBegin/glEnd: the same six vertices, assembled by hand. */
    reset_view();
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, verts);
    glColor3f(1.0f, 0.5f, 0.0f);
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 6; i++)
        glArrayElement((GLint)idx[i]);
    glEnd();
    if (!frame_matches(want)) {
        glDisableClientState(GL_VERTEX_ARRAY);
        return 0;
    }
    glDisableClientState(GL_VERTEX_ARRAY);

    /* glInterleavedArrays with GL_V3F: the same positions out of one buffer. */
    reset_view();
    glInterleavedArrays(GL_V3F, 0, verts);
    glColor3f(1.0f, 0.5f, 0.0f);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, idx);
    if (!frame_matches(want)) {
        glDisableClientState(GL_VERTEX_ARRAY);
        return 0;
    }
    glDisableClientState(GL_VERTEX_ARRAY);
    return glGetError() == GL_NO_ERROR;
}

/* The other spellings of the attribute calls must reach the same place as the float
 * ones. Drawn twice and compared pixel for pixel. */
static int check_type_variants(void) {
    static uint32_t want[PROBE_W * PROBE_H];

    reset_view();
    glColor3f(1.0f, 0.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex3f(-0.6f, -0.6f, 0.0f);
    glVertex3f(0.6f, -0.6f, 0.0f);
    glVertex3f(0.6f, 0.6f, 0.0f);
    glVertex3f(-0.6f, 0.6f, 0.0f);
    glEnd();
    frame_snapshot(want);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    reset_view();
    glColor3ub(255, 0, 255); /* 255 must divide by 255, not shift by 8 */
    glBegin(GL_QUADS);
    glVertex3d(-0.6, -0.6, 0.0);
    glVertex3d(0.6, -0.6, 0.0);
    glVertex3d(0.6, 0.6, 0.0);
    glVertex3d(-0.6, 0.6, 0.0);
    glEnd();
    if (!frame_matches(want))
        return 0;
    return glGetError() == GL_NO_ERROR;
}

/* The client attribute stack is a separate stack from the server one, and the pop must
 * restore the array state without disturbing anything else. */
static int check_client_attrib(void) {
    reset_view();
    static const GLfloat verts[12] = {
        -0.6f, -0.6f, 0.0f, 0.6f, -0.6f, 0.0f, 0.6f, 0.6f, 0.0f, -0.6f, 0.6f, 0.0f,
    };
    glVertexPointer(3, GL_FLOAT, 0, verts);
    glEnableClientState(GL_VERTEX_ARRAY);

    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    glDisableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 64, NULL);
    glPopClientAttrib();
    if (glGetError() != GL_NO_ERROR)
        return 0;

    /* If the pop worked the array is enabled again and points at `verts`, so this
     * draws. */
    glColor3f(0.0f, 1.0f, 0.5f);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisableClientState(GL_VERTEX_ARRAY);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    return near_rgb(px(PROBE_W / 2 - 8, PROBE_H / 2 + 8), 0, 255, 128, 12);
}

/* `glDepthRange` decides where NDC z lands in the depth buffer, and reversing it
 * reverses which of two fragments wins. **Two draws in the same order with the range
 * flipped** - if the range were ignored, both would give the same answer. */
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
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glEnd();
    uint32_t normal = px(PROBE_W / 2, PROBE_H / 2);

    glDepthRange(1.0, 0.0); /* reversed */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glColor3f(0.0f, 1.0f, 0.0f);
    glRectf(-0.5f, -0.5f, 0.5f, 0.5f);
    glColor3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glEnd();
    uint32_t reversed = px(PROBE_W / 2, PROBE_H / 2);

    glDepthRange(0.0, 1.0);
    glDisable(GL_DEPTH_TEST);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    /* The two must differ; which is which depends on the sign convention, and that is
     * gl1-cube's business because it has a measured frame to compare against. */
    return normal != reversed;
}

/* The per-object queries answer from the same field their setters write. Not a pixel
 * check - these have no visible effect - but a query that disagrees with the state it
 * is querying is exactly the bug this found in glIsEnabled. */
static int check_object_queries(void) {
    reset_view();
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GLint iv = 0;
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &iv);
    glDeleteTextures(1, &tex);
    if (iv != (GLint)GL_CLAMP_TO_EDGE)
        return 0;

    static const GLfloat amb[4] = {0.125f, 0.25f, 0.5f, 0.75f};
    GLfloat fv[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glLightfv(GL_LIGHT1, GL_AMBIENT, amb);
    glGetLightfv(GL_LIGHT1, GL_AMBIENT, fv);
    if (fv[0] != 0.125f || fv[3] != 0.75f)
        return 0;

    /* glIsEnabled must answer from the same list glEnable accepts - these two were
     * absent once and read back as off immediately after being switched on. */
    glEnable(GL_ALPHA_TEST);
    if (glIsEnabled(GL_ALPHA_TEST) != GL_TRUE)
        return 0;
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_POLYGON_OFFSET_FILL);
    if (glIsEnabled(GL_POLYGON_OFFSET_FILL) != GL_TRUE)
        return 0;
    glDisable(GL_POLYGON_OFFSET_FILL);

    return glGetError() == GL_NO_ERROR;
}

/* **Polygon offset**, which moves a filled polygon's depth so coplanar geometry can be
 * drawn over it. Two quads at exactly the same z with GL_LESS: without the offset the
 * second loses, with a negative offset pulling it nearer it wins. */
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
    if (glGetError() != GL_NO_ERROR)
        return 0;

    return near_rgb(px(PROBE_W / 2, PROBE_H / 2), 255, 0, 0, 8);
}

/* **The blend equation, which was stored and used by nothing.**
 *
 * `GL_FUNC_SUBTRACT` against a known destination must come out darker than
 * `GL_FUNC_ADD` does, and `GL_MAX` must ignore the factors entirely. The fixture uses a
 * mid-grey destination and a mid-grey source so that add saturates upward and subtract
 * lands near zero - with a black destination, add and subtract of the same source agree
 * and the check proves nothing.
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
    glColor3f(0.1f, 0.1f,
              0.1f); /* darker than the destination, so MAX must keep the dest */
    glRectf(0.3f, -0.4f, 0.9f, 0.4f);

    glBlendEquation(GL_FUNC_ADD);
    glDisable(GL_BLEND);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    uint32_t added = px(PROBE_W * 3 / 16, PROBE_H / 2);
    uint32_t subtracted = px(PROBE_W * 7 / 16, PROBE_H / 2);
    uint32_t maxed = px(PROBE_W * 12 / 16, PROBE_H / 2);

    if (chan_r(added) < 240)
        return 0; /* 0.5 + 0.5 saturates to white */
    if (chan_r(subtracted) > 16)
        return 0; /* 0.5 - 0.5 is black */
    if (!near_rgb(maxed, 128, 128, 128, 12))
        return 0; /* max(0.1, 0.5) keeps the destination */
    return 1;
}

/* **Two-sided lighting**, which was refused here - this check used to confirm the
 * refusal - and before that set a field nothing read. A clockwise (back-facing) quad on
 * the left and a counter-clockwise one on the right, green emission in front and red
 * behind: with GL_LIGHT_MODEL_TWO_SIDE the left is red and the right green. Lighting is
 * CPU work on both paths, choosing the side by the triangle's winding before any vertex
 * is lit, so this should pass on the console. */
static int check_two_side(void) {
    reset_view();
    static const GLfloat green[4] = {0.0f, 1.0f, 0.0f, 1.0f},
                         red[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    static const GLfloat none[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    one_light(0.0f, 0.0f);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, none);
    glMaterialfv(GL_FRONT, GL_EMISSION, green);
    glMaterialfv(GL_BACK, GL_EMISSION, red);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(-0.9f, -0.5f);
    glVertex2f(-0.9f, 0.5f);
    glVertex2f(-0.1f, 0.5f);
    glVertex2f(-0.1f, -0.5f);
    glVertex2f(0.1f, -0.5f);
    glVertex2f(0.9f, -0.5f);
    glVertex2f(0.9f, 0.5f);
    glVertex2f(0.1f, 0.5f);
    glEnd();
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, none);
    lighting_off();
    if (glGetError() != GL_NO_ERROR)
        return 0;
    return near_rgb(px(PROBE_W / 4, PROBE_H / 2), 255, 0, 0, 16) &&
           near_rgb(px(PROBE_W * 3 / 4, PROBE_H / 2), 0, 255, 0, 16);
}

/* ------------------------------------------------------------------------- */

/* Two triangles of the same shape, left and right, wound opposite ways. */
static void cull_pair(void) {
    glColor3f(1.0f, 1.0f, 0.0f);
    glBegin(GL_TRIANGLES);
    glVertex3f(-0.9f, -0.4f, 0.0f);
    glVertex3f(-0.1f, -0.4f, 0.0f);
    glVertex3f(-0.5f, 0.4f, 0.0f);

    glVertex3f(0.9f, -0.4f, 0.0f);
    glVertex3f(0.1f, -0.4f, 0.0f);
    glVertex3f(0.5f, 0.4f, 0.0f);
    glEnd();
}

#define CULL_LEFT_X (PROBE_W / 4)
#define CULL_RIGHT_X (PROBE_W * 3 / 4)
#define CULL_Y (PROBE_H * 55 / 100)

/*
 * Face culling - the one piece of per-draw state gl1-cube leans on that nothing here
 * touched. `GL_CULL_FACE` appeared exactly once in this file before, in reset_view(),
 * being disabled.
 *
 * **Which winding is front is deliberately not asserted.** Facing is decided from the
 * signed area in window space, and window space here is Y-flipped relative to GL's, so
 * writing down which of the two triangles "should" survive would make this a test of my
 * arithmetic. What the specification fixes is the relationship, and that is what this
 * checks: exactly one of an opposite-wound pair survives, glFrontFace swaps which one,
 * and GL_FRONT_AND_BACK removes both. An implementation that ignores culling fails the
 * first, one that ignores glFrontFace fails the second, one that culls everything fails
 * the control.
 */
static int check_cull_face(void) {
    /* Control first: with culling off both must draw, or the assertions below would
     * pass for the wrong reason. */
    reset_view();
    cull_pair();
    if (px(CULL_LEFT_X, CULL_Y) == PROBE_BG)
        return 0;
    if (px(CULL_RIGHT_X, CULL_Y) == PROBE_BG)
        return 0;

    reset_view();
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    cull_pair();
    const int left_ccw = px(CULL_LEFT_X, CULL_Y) != PROBE_BG;
    const int right_ccw = px(CULL_RIGHT_X, CULL_Y) != PROBE_BG;
    if (left_ccw == right_ccw)
        return 0; /* both survived, or neither */

    reset_view();
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);
    cull_pair();
    if ((px(CULL_LEFT_X, CULL_Y) != PROBE_BG) == left_ccw)
        return 0;
    if ((px(CULL_RIGHT_X, CULL_Y) != PROBE_BG) == right_ccw)
        return 0;

    reset_view();
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT_AND_BACK);
    cull_pair();
    if (px(CULL_LEFT_X, CULL_Y) != PROBE_BG)
        return 0;
    if (px(CULL_RIGHT_X, CULL_Y) != PROBE_BG)
        return 0;

    return glGetError() == GL_NO_ERROR;
}

/*
 * glViewport, which nothing here had ever called - every check drew into the whole
 * window, so a viewport applied as a scale but not an offset, or dropped entirely,
 * looked identical.
 *
 * A quarter-size viewport in the middle: the full-NDC rectangle below has to land
 * inside x [32, 96) and y [24, 72) at this probe's 128x96, and nowhere else. The edge
 * samples sit two pixels either side of the left boundary, so getting the scale right
 * but the offset wrong is a failure rather than a near miss.
 */
static int check_viewport(void) {
    reset_view();
    glViewport(PROBE_W / 4, PROBE_H / 4, PROBE_W / 2, PROBE_H / 2);
    draw_rect(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.6f, 1.0f);
    glViewport(0, 0, PROBE_W, PROBE_H);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    if (px(PROBE_W / 2, PROBE_H / 2) == PROBE_BG)
        return 0; /* centre: drawn */
    if (px(PROBE_W / 8, PROBE_H / 8) != PROBE_BG)
        return 0; /* outside, top left */
    if (px(PROBE_W * 7 / 8, PROBE_H * 7 / 8) != PROBE_BG)
        return 0; /* outside, bottom right */
    if (px(PROBE_W / 4 + 2, PROBE_H / 2) == PROBE_BG)
        return 0; /* just inside the left edge */
    if (px(PROBE_W / 4 - 2, PROBE_H / 2) != PROBE_BG)
        return 0; /* just outside it */
    return 1;
}

/*
 * glDepthMask: colour without depth.
 *
 * Under this projection - glOrtho(-1, 1, -1, 1, -1, 1) - eye z maps to window z as -z,
 * so z = +0.5 is the nearer surface and z = -0.5 the farther one.
 *
 * With writes on, the near quad records its depth and the far quad is rejected. With
 * writes off it never records anything, so the far quad passes against the cleared
 * buffer and takes the pixel. Both outcomes are asserted rather than only that the two
 * differ, so a rasteriser that had the mask inverted fails here too.
 */
static int check_depth_mask(void) {
    reset_view();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDepthMask(GL_TRUE);
    draw_quad_z(-0.6f, -0.6f, 0.6f, 0.6f, 0.5f, 1.0f, 0.0f, 0.0f);  /* near, red */
    draw_quad_z(-0.6f, -0.6f, 0.6f, 0.6f, -0.5f, 0.0f, 0.0f, 1.0f); /* far, blue */
    const uint32_t writes_on = px(PROBE_W / 2, PROBE_H / 2);

    reset_view();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDepthMask(GL_FALSE);
    draw_quad_z(-0.6f, -0.6f, 0.6f, 0.6f, 0.5f, 1.0f, 0.0f, 0.0f);
    glDepthMask(GL_TRUE);
    draw_quad_z(-0.6f, -0.6f, 0.6f, 0.6f, -0.5f, 0.0f, 0.0f, 1.0f);
    const uint32_t writes_off = px(PROBE_W / 2, PROBE_H / 2);

    glDisable(GL_DEPTH_TEST);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    if (!near_rgb(writes_on, 255, 0, 0, 8))
        return 0;
    if (!near_rgb(writes_off, 0, 0, 255, 8))
        return 0;
    return 1;
}

/*
 * Texture wrap modes, sampled rather than queried.
 *
 * `GL_TEXTURE_WRAP_S` was already set and read back through glGetTexParameteriv, which
 * proves the field round-trips and nothing about the sampler. This runs texture
 * coordinates out to s = 2 across the quad and asks where three of them land:
 *
 *   s = 0.25 and s = 0.75 are the two texel columns, in range under either mode.
 *   s = 1.25 is out of range. GL_REPEAT takes its fractional part, 0.25, and lands on
 * the *first* column; GL_CLAMP_TO_EDGE holds it at the *last*.
 *
 * So the same pixel has to agree with a different in-range sample under each mode,
 * which no single wrap implementation can satisfy both ways round.
 */
static int check_texture_wrap(void) {
    static const GLubyte texels[16] = {
        255, 0, 0,   255, 0,   255, 0, 255, /* row 0: red, green */
        0,   0, 255, 255, 255, 255, 0, 255, /* row 1: blue, yellow */
    };
    /* s = 0.25, 0.75 and 1.25 along a quad spanning NDC x [-0.8, 0.8] with s in [0, 2].
     */
    const int x_lo =
        (int)(((-0.8f + 0.25f / 2.0f * 1.6f) + 1.0f) * 0.5f * (float)PROBE_W);
    const int x_hi =
        (int)(((-0.8f + 0.75f / 2.0f * 1.6f) + 1.0f) * 0.5f * (float)PROBE_W);
    const int x_out =
        (int)(((-0.8f + 1.25f / 2.0f * 1.6f) + 1.0f) * 0.5f * (float)PROBE_W);
    const int y = PROBE_H / 2;

    GLuint tex = 0;
    GLenum modes[2] = {GL_REPEAT, GL_CLAMP_TO_EDGE};
    uint32_t lo[2] = {0u, 0u}, hi[2] = {0u, 0u}, out[2] = {0u, 0u};

    for (int m = 0; m < 2; m++) {
        reset_view();
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     texels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)modes[m]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glEnable(GL_TEXTURE_2D);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glColor3f(1.0f, 1.0f, 1.0f);

        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.25f);
        glVertex3f(-0.8f, -0.5f, 0.0f);
        glTexCoord2f(2.0f, 0.25f);
        glVertex3f(0.8f, -0.5f, 0.0f);
        glTexCoord2f(2.0f, 0.25f);
        glVertex3f(0.8f, 0.5f, 0.0f);
        glTexCoord2f(0.0f, 0.25f);
        glVertex3f(-0.8f, 0.5f, 0.0f);
        glEnd();
        glDisable(GL_TEXTURE_2D);
        if (glGetError() != GL_NO_ERROR) {
            glDeleteTextures(1, &tex);
            return 0;
        }

        lo[m] = px(x_lo, y);
        hi[m] = px(x_hi, y);
        out[m] = px(x_out, y);
        glDeleteTextures(1, &tex); /* after the reads - see check_texture */
        if (lo[m] == PROBE_BG || hi[m] == PROBE_BG || out[m] == PROBE_BG)
            return 0;
        if (lo[m] == hi[m])
            return 0; /* the two columns must be telling apart in the first place */
    }

    if (out[0] != lo[0])
        return 0; /* GL_REPEAT wrapped to the first column */
    if (out[1] != hi[1])
        return 0; /* GL_CLAMP_TO_EDGE held the last */
    return 1;
}

/*
 * glTexSubImage2D. The roadmap has listed it as done since it landed and nothing had
 * drawn with it - a sub-image that overwrote the whole level, or landed at the wrong
 * offset, would have been invisible here.
 *
 * A 4x4 of one colour with a 2x2 patch of another written into one quadrant: exactly
 * one of the four quadrants may change, and which one is not asserted for the same
 * reason the texture check does not assert orientation.
 */
static int check_tex_sub_image(void) {
    static GLubyte base[4 * 4 * 4];
    static GLubyte patch[2 * 2 * 4];
    for (int i = 0; i < 4 * 4; i++) {
        base[i * 4 + 0] = 40;
        base[i * 4 + 1] = 80;
        base[i * 4 + 2] = 200;
        base[i * 4 + 3] = 255;
    }
    for (int i = 0; i < 2 * 2; i++) {
        patch[i * 4 + 0] = 250;
        patch[i * 4 + 1] = 200;
        patch[i * 4 + 2] = 20;
        patch[i * 4 + 3] = 255;
    }

    uint32_t quad[2][4];
    for (int pass = 0; pass < 2; pass++) {
        GLuint tex = 0;
        reset_view();
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     base);
        if (pass == 1) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 2, 2, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE,
                            patch);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glEnable(GL_TEXTURE_2D);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glColor3f(1.0f, 1.0f, 1.0f);

        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(-0.8f, -0.8f, 0.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(0.8f, -0.8f, 0.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(0.8f, 0.8f, 0.0f);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(-0.8f, 0.8f, 0.0f);
        glEnd();
        glDisable(GL_TEXTURE_2D);
        if (glGetError() != GL_NO_ERROR) {
            glDeleteTextures(1, &tex);
            return 0;
        }

        quad[pass][0] = px(PROBE_W * 5 / 16, PROBE_H * 5 / 16);
        quad[pass][1] = px(PROBE_W * 11 / 16, PROBE_H * 5 / 16);
        quad[pass][2] = px(PROBE_W * 5 / 16, PROBE_H * 11 / 16);
        quad[pass][3] = px(PROBE_W * 11 / 16, PROBE_H * 11 / 16);
        glDeleteTextures(1, &tex); /* after the reads - see check_texture */
    }

    /* Before the sub-image every quadrant is the base colour. */
    for (int q = 0; q < 4; q++) {
        if (quad[0][q] == PROBE_BG)
            return 0;
        if (quad[0][q] != quad[0][0])
            return 0;
    }

    int changed = 0;
    for (int q = 0; q < 4; q++) {
        if (quad[1][q] != quad[0][q])
            changed++;
    }
    return changed == 1;
}

/*
 * Two lights at once. GL_LIGHT1 appeared here only in a glLightfv/glGetLightfv
 * round-trip, so whether a second light contributes any light was untested - and
 * GL_MAX_LIGHTS is reported as eight a few checks further down.
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

        /* The second light is configured either way, so the two passes differ only in
         * whether it is enabled - not in what it was told. */
        glLightfv(GL_LIGHT1, GL_POSITION, pos1);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, green);
        glLightfv(GL_LIGHT1, GL_AMBIENT, black);
        if (pass == 1)
            glEnable(GL_LIGHT1);
        else
            glDisable(GL_LIGHT1);

        glNormal3f(0.0f, 0.0f, 1.0f);
        glBegin(GL_QUADS);
        glVertex3f(-0.6f, -0.6f, 0.0f);
        glVertex3f(0.6f, -0.6f, 0.0f);
        glVertex3f(0.6f, 0.6f, 0.0f);
        glVertex3f(-0.6f, 0.6f, 0.0f);
        glEnd();

        glDisable(GL_LIGHTING);
        glDisable(GL_LIGHT0);
        glDisable(GL_LIGHT1);
        if (glGetError() != GL_NO_ERROR)
            return 0;
        lit[pass] = px(PROBE_W / 2, PROBE_H / 2);
    }

    /* One light: red, no green. Two: the green one adds to it without taking the red
     * away. */
    if (chan_r(lit[0]) < 60)
        return 0;
    if (chan_g(lit[0]) > 20)
        return 0;
    if (chan_g(lit[1]) < 60)
        return 0;
    if (chan_r(lit[1]) < chan_r(lit[0]) - 8)
        return 0;
    return 1;
}

/* ------------------------------------------------------------------------- */

/*
 * **Deleting a texture that a built frame still references.**
 *
 * Legal GL: the *name* is freed at once, and the *storage* has to outlive the draws
 * that name it. On a deferred hardware path that is not automatic - `glDrawArrays`
 * writes the texture's address into a command buffer and returns, so freeing the memory
 * before the submission hands the sampler unmapped pages.
 *
 * This is not hypothetical and it is why the check is last. On 2026-09-17 the texture
 * checks did this incidentally, and the console answered with a GPU protection fault -
 * `client:TCP(8) access:Read`, `Unmapped page access`, 504 wavefronts with `XNACK_ERROR
 * MEMVIOL`
 * - which reset the GPU, restarted the user interface, and cost every result after
 * check nine. oops-sdk now flushes before releasing texture storage. Asking the
 * question once, at the end, means a regression costs this one row rather than the
 * whole run.
 */
static int check_tex_delete_in_frame(void) {
    static const GLubyte texels[16] = {
        255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255,
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
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-0.8f, -0.8f, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(0.8f, -0.8f, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(0.8f, 0.8f, 0.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-0.8f, 0.8f, 0.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    /* **Before anything is sampled**, which on the hardware path means before the frame
     * has been submitted. This is the whole point of the check. */
    glDeleteTextures(1, &tex);
    if (glGetError() != GL_NO_ERROR)
        return 0;

    /* The draw still has to have happened, and with the texture's own colours: an
     * implementation that quietly dropped the draw rather than faulting would leave the
     * background. */
    uint32_t a = px(PROBE_W * 5 / 16, PROBE_H * 5 / 16);
    uint32_t b = px(PROBE_W * 11 / 16, PROBE_H * 11 / 16);
    if (a == PROBE_BG || b == PROBE_BG)
        return 0;
    if (a == b)
        return 0;
    return 1;
}

static int check_limits_reported(void) {
    reset_view();
    GLint v = 0;
    glGetIntegerv(GL_MAX_LIGHTS, &v);
    if (v < 8)
        return 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &v);
    if (v < 64)
        return 0;
    glGetIntegerv(GL_MAX_MODELVIEW_STACK_DEPTH, &v);
    if (v < 2)
        return 0;
    if (glGetError() != GL_NO_ERROR)
        return 0;

    const GLubyte *ver = glGetString(GL_VERSION);
    if (!ver || ver[0] < '1' || ver[0] > '9')
        return 0;
    return 1;
}

/* **What the sampler actually fetched**, which no other check here asks.
 *
 * `texture` next door draws a 2x2 and requires its four quadrants to come back
 * distinct. That passes for a sampler reading the right texture at the wrong stride,
 * because wrong rows are still four different colours - and a port whose textures
 * render as smooth, wrongly-coloured surfaces is exactly the case it cannot see. The
 * difference between "the bytes in memory are right" and "the sampler read the bytes in
 * memory" has cost several hardware runs, and until now the only instrument for it was
 * a photograph of a television.
 *
 * So each texel is given an identity and the identity is read back out of the
 * framebuffer. Red carries the column and green the row, which separates the two
 * failures that look alike on a screen: a texture addressed at the wrong pitch comes
 * back with the wrong *row* while its columns stay in order, and one addressed at the
 * wrong width comes back with the wrong column.
 *
 * The widths are the ones the 64-pixel rule divides. `ADDR_SW_LINEAR` aligns a row to
 * 256 bytes, so a 4-wide RGBA texture occupies rows 64 texels apart with 60 texels of
 * padding, while a 64-wide one has no padding at all and needs no custom pitch in its
 * descriptor. 64 is therefore the control: if it passes and 4 fails, the pitch is the
 * answer and nothing else is.
 *
 * Orientation is deliberately not asserted - which screen band holds which texel row
 * depends on a convention `texture` also declines to pin down, and gl-cube owns that
 * question. What is asserted is that the mapping is a bijection: four bands, four
 * distinct rows. A sampler that collapses them all onto row 0, which is what reading a
 * 4-wide texture at a 4-texel stride does, fails that however the image is flipped.
 */
/* `modulate` asks the other half of the question. Under GL_REPLACE the texel reaches
 * the framebuffer untouched, so a failure is the sampler's; under GL_MODULATE it is
 * multiplied by the vertex colour first, so a failure that appears only here is the
 * combine's. The reported fault has two halves - surfaces that lost their detail and
 * surfaces that came back the wrong colour - and those are not necessarily the same
 * bug. Running the identical texture both ways is what tells them apart.
 *
 * The vertex colour is (1, 1/2, 1/4): three different factors, so a combine that
 * multiplied the wrong channel together shows up as a specific wrong number rather than
 * as "not what we expected". Blue is the clearest of the three - the texture holds 128
 * everywhere, so the framebuffer must hold 32, and 64 or 128 coming back names which
 * vertex channel reached it. */
/* **The same quad, submitted the three ways a program can submit one.**
 *
 * Every textured check in this suite draws with glBegin/glEnd, and the port draws
 * everything through vertex arrays backed by buffer objects. `vertex-arrays` and
 * `buffer-objects` exist here but neither has ever bound a texture, so "a textured draw
 * through an array" - which is what a real title does for every triangle it has - was
 * untested in both halves at once.
 *
 * The texture coordinate is the reason to care. `glTexCoordPointer(2, ...)` supplies s
 * and t and leaves the array path to default q to 1, and the textured shader divides s
 * and t by q at every fragment. A q that arrives as zero makes a coordinate that
 * explodes per pixel, which is a surface of fine noise rather than a stretched image -
 * and immediate mode, where glTexCoord2f sets q itself, would never show it.
 */
enum { QUAD_IMMEDIATE = 0, QUAD_ARRAY = 1, QUAD_VBO = 2 };

/* Raised for one check only - see `check_tex_state_leak`. */
static int g_readback_dirty;
static void dirty_then_restore_tex_state(void);

static const GLfloat g_quad_pos[12] = {
    -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f,
};
static const GLfloat g_quad_tc[8] = {
    0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
};

static void draw_unit_quad(int via) {
    if (via == QUAD_IMMEDIATE) {
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(-1.0f, -1.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(1.0f, -1.0f, 0.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(1.0f, 1.0f, 0.0f);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(-1.0f, 1.0f, 0.0f);
        glEnd();
        return;
    }
    if (via == QUAD_ARRAY) {
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, g_quad_pos);
        glTexCoordPointer(2, GL_FLOAT, 0,
                          g_quad_tc); /* two components: q is the array's to default */
        glDrawArrays(GL_QUADS, 0, 4);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
        return;
    }
    /* The same again out of buffer objects, which is what the port actually does - the
     * pointer arguments below are offsets into the bound buffer, not addresses. */
    GLuint vb = 0, tb = 0;
    glGenBuffers(1, &vb);
    glGenBuffers(1, &tb);
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(g_quad_pos), g_quad_pos,
                 GL_STATIC_DRAW);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, (const GLvoid *)0);
    glBindBuffer(GL_ARRAY_BUFFER, tb);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(g_quad_tc), g_quad_tc,
                 GL_STATIC_DRAW);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, (const GLvoid *)0);
    glDrawArrays(GL_QUADS, 0, 4);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &vb);
    glDeleteBuffers(1, &tb);
}

static int readback_at_width(const char *name, const char *name_rgb, int w,
                             int modulate, int via) {
    reset_view();

    /* Red spread across the full range so neighbouring columns cannot be confused at 8
     * bits; green on 64-unit centres, which decodes by a shift and tolerates any
     * rounding a blend or a format conversion could introduce; blue constant, as the
     * witness that this is our texture being read at all rather than the background or
     * whatever the allocator left. */
    static GLubyte texels[64 * 4 * 4];
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < w; x++) {
            GLubyte *t = texels + (((size_t)y * (size_t)w) + (size_t)x) * 4u;
            t[0] = (GLubyte)((x * 252) / (w - 1));
            t[1] = (GLubyte)(32 + y * 64);
            t[2] = 128;
            t[3] = 255;
        }
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    /* NEAREST both ways and clamped: every sample must land on one texel and return it
     * whole, so that a value between two texels is a failure rather than a filter doing
     * its job. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glEnable(GL_TEXTURE_2D);
    /* GL_REPLACE puts the texel in the framebuffer unmultiplied, so a wrong pixel is
     * the sampler's doing and nothing else's. GL_MODULATE adds the vertex colour, which
     * is the only difference between the two runs - so a texel that survives the first
     * and not the second was sampled correctly and combined wrongly. */
    if (modulate) {
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glColor3f(1.0f, 0.5f, 0.25f);
    } else {
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glColor3f(1.0f, 1.0f, 1.0f);
    }

    /* Set by `check_tex_state_leak` only: dirty the coordinate machinery and put it
       back, so the draw below runs against state that is the default by the
       specification but arrived there rather than starting there. */
    if (g_readback_dirty)
        dirty_then_restore_tex_state();

    draw_unit_quad(via);
    glDisable(GL_TEXTURE_2D);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &tex);
        return 0;
    }

    /* Three columns and four bands, each sampled at its centre: the quad covers the
     * viewport, so band b spans t in [b/4, (b+1)/4) and its centre lands squarely
     * inside texel row b. */
    /* Where the row codes land once the combine has had them. GL_REPLACE leaves them at
     * 32, 96, 160 and 224; GL_MODULATE halves them to 16, 48, 80 and 112. The tolerance
     * stays under half the step either way, so a value falling between two rows is
     * refused rather than rounded into one of them. Blue is the constant witness: 128
     * in the texture, so 128 back under REPLACE and 32 under MODULATE, and any other
     * value names which vertex channel reached it. */
    const int g_base = modulate ? 16 : 32;
    const int g_step = modulate ? 32 : 64;
    const int b_want = modulate ? 32 : 128;
    const int g_tol = modulate ? 10 : 16;

    int row_of_band[4];
    int r_first[4], r_last[4];
    int blue_ok = 1;
    for (int b = 0; b < 4; b++) {
        const int y = (b * 2 + 1) * PROBE_H / 8;
        int g_seen = -1, ok = 1;
        int rs[3];
        for (int k = 0; k < 3; k++) {
            const uint32_t c = px((k * 2 + 1) * PROBE_W / 6, y);
            rs[k] = chan_r(c);
            if (chan_b(c) < b_want - 12 || chan_b(c) > b_want + 12)
                blue_ok = 0;
            const int g = chan_g(c);
            /* Decode the row this texel came from, and refuse a green that is not one
             * of the four the texture holds - an interpolated or invented value is not
             * a row. */
            const int idx = (g - g_base + g_step / 2) / g_step;
            const int centre_of_idx = g_base + idx * g_step;
            if (g < g_base - g_step / 2 || idx < 0 || idx > 3) {
                ok = 0;
            } else if (g - centre_of_idx > g_tol || centre_of_idx - g > g_tol) {
                ok = 0;
            } else if (g_seen < 0) {
                g_seen = idx;
            } else if (g_seen != idx) {
                ok = 0;
            } /* the row changed along a row: a skewed read */
        }
        row_of_band[b] = ok ? g_seen : 0xf;
        r_first[b] = rs[0];
        r_last[b] = rs[2];
    }
    /* Read before the texture goes, so the colour reported on a failure is the one
     * drawn. */
    const uint32_t centre = px(PROBE_W / 2, PROBE_H / 2);
    glDeleteTextures(1, &tex);

    /* One word carrying the whole result, so a failure on the console says what it saw
     * instead of only that it failed: a nibble per band holding the texture row that
     * band sampled. 0x0123 and 0x3210 are the two correct answers; 0x0000 is every band
     * reading row 0, which is what a 4-wide texture read at a 4-texel stride gives; 0xf
     * marks a green that was not a row. */
    if (gl1_probe_saw) {
        gl1_probe_saw(
            name, ((uint32_t)row_of_band[0] << 12) | ((uint32_t)row_of_band[1] << 8) |
                      ((uint32_t)row_of_band[2] << 4) | (uint32_t)row_of_band[3]);
        /* And the colour itself, which the row map cannot carry. Under MODULATE this is
         * the one number that says what the combine did: the centre texel is a known
         * row and column, so its three channels are known before the multiply and known
         * after it. */
        gl1_probe_saw(name_rgb, centre);
    }

    if (!blue_ok)
        return 0;
    /* Red is multiplied by 1, so under MODULATE it must come back at full strength. The
     * texture's rightmost sampled column holds 212 at this width; a half or a quarter
     * of that is 106 or 53, so a single threshold separates "red was left alone" from
     * "red was scaled by the wrong vertex channel" without depending on exactly which
     * texel the sampler picked. */
    if (modulate) {
        int red_max = 0;
        for (int b = 0; b < 4; b++) {
            if (r_first[b] > red_max)
                red_max = r_first[b];
            if (r_last[b] > red_max)
                red_max = r_last[b];
        }
        if (red_max < 180)
            return 0;
    }
    /* Four bands, four different rows. */
    for (int b = 0; b < 4; b++) {
        if (row_of_band[b] > 3)
            return 0;
        for (int o = b + 1; o < 4; o++) {
            if (row_of_band[b] == row_of_band[o])
                return 0;
        }
    }
    /* And the columns run one way along every band. A flip reverses all four together,
     * so the direction is not asserted, only that there is one and that it is shared.
     */
    const int dir = (r_last[0] > r_first[0]) ? 1 : -1;
    for (int b = 0; b < 4; b++) {
        const int d = r_last[b] - r_first[b];
        if (d < 24 && d > -24)
            return 0; /* flat: no column information survived */
        if ((d > 0 ? 1 : -1) != dir)
            return 0; /* scrambled: bands disagree */
    }
    return 1;
}

static int check_tex_readback_w4(void) {
    return readback_at_width("tex-readback/w4", "tex-readback/w4-rgb", 4, 0,
                             QUAD_IMMEDIATE);
}
static int check_tex_readback_w16(void) {
    return readback_at_width("tex-readback/w16", "tex-readback/w16-rgb", 16, 0,
                             QUAD_IMMEDIATE);
}
static int check_tex_readback_w64(void) {
    return readback_at_width("tex-readback/w64", "tex-readback/w64-rgb", 64, 0,
                             QUAD_IMMEDIATE);
}
/* **The same texture and the same verification, submitted the way a title submits.**
 * Width 4 deliberately: the immediate-mode run at this width already passes on
 * hardware, so a failure here is the submission path and cannot be the pitch. It is
 * also the width of every gradient the port stretches across its sky, which is the
 * surface that renders as noise. */
static int check_tex_array_draw(void) {
    return readback_at_width("tex-array/w4", "tex-array/w4-rgb", 4, 0, QUAD_ARRAY);
}
static int check_tex_vbo_draw(void) {
    return readback_at_width("tex-vbo/w4", "tex-vbo/w4-rgb", 4, 0, QUAD_VBO);
}

/* **A bug class this suite cannot catch by construction.**
 *
 * Every check here begins with `reset_view` and tests one feature from a clean slate. A
 * real title never has a clean slate: it draws hundreds of things in sequence and each
 * inherits whatever the last one left set. So a feature that works when switched on and
 * fails to switch *off* passes every check here and corrupts every frame of a port -
 * and the surface that shows it first is whichever one is drawn with the fewest
 * settings of its own, because it is the one relying most on the defaults being
 * defaults.
 *
 * In the port being chased that is the sky, which turns off the depth test, culling and
 * lighting and then draws a four-pixel gradient with nothing else set at all.
 *
 * So this sets the texture-coordinate machinery to something wrong, puts it back the
 * way GL says it starts, and then runs the ordinary width-4 readback. The expected
 * answer is identical to `tex-readback-w4`, which passes - so any difference is state
 * that did not come back.
 */
static void dirty_then_restore_tex_state(void) {
    /* A texture matrix that would scale coordinates far past the image, which is what a
     * sky of fine noise instead of a stretched gradient would look like if it survived.
     */
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glScalef(64.0f, 64.0f, 1.0f);
    glTranslatef(0.375f, 0.125f, 0.0f);

    /* Generated coordinates with planes that are not the defaults, on both axes. */
    static const GLfloat sp[4] = {3.0f, 1.0f, 0.0f, 0.25f};
    static const GLfloat tp[4] = {0.0f, 5.0f, 1.0f, 0.5f};
    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glTexGenfv(GL_S, GL_OBJECT_PLANE, sp);
    glTexGenfv(GL_T, GL_OBJECT_PLANE, tp);
    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);

    /* And a wrap and filter that are not the ones the check will ask for. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    /* Now put every one of them back to what GL says it starts as. From here the state
     * is the default by the specification, and a draw must not be able to tell this
     * happened. */
    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
}

/* **The port's sky, as nearly as a check can be it.**
 *
 * Every readback here asks for GL_NEAREST, because a nearest sample returns one texel
 * whole and that is what made those checks decidable. Nothing has ever asked this GL to
 * *magnify* a texture, and magnifying is the whole of what the surface that renders
 * wrongly does: the sky is a four-pixel-wide gradient stretched across nineteen
 * hundred, with the default GL_LINEAR filter, and it comes back as fine noise instead
 * of a smooth ramp.
 *
 * Four by a hundred and twenty-eight exactly, because that is the shape every `back/`
 * image in the port has, and because it is the shape where the row padding is largest:
 * a 4-wide RGBA texture occupies rows 64 texels wide, so sixty of every sixty-four
 * texels a row contains are zeroes that no nearest sample can reach and an
 * interpolating one can.
 *
 * The verdict is not a colour but a shape. Red rises across the four texels, so a
 * magnified row must rise smoothly from left to right; what is asserted is that it
 * rises, and that no two neighbouring samples differ by more than a stretched gradient
 * could. Noise fails the second of those however pretty its colours are.
 */
static int check_tex_linear_stretch(void) {
    reset_view();

    static GLubyte img[4 * 128 * 4];
    for (int y = 0; y < 128; y++) {
        for (int x = 0; x < 4; x++) {
            GLubyte *t = img + (((size_t)y * 4u) + (size_t)x) * 4u;
            t[0] = (GLubyte)(x * 85); /* 0, 85, 170, 255 across the width */
            t[1] = 64;
            t[2] = 192;
            t[3] = 255;
        }
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    /* GL_LINEAR both ways, and clamped so the edges do not wrap - the port's own
     * setting, and the one no check here has used. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor3f(1.0f, 1.0f, 1.0f);
    /* The sky's own state, so that if any of it matters this has it too. */
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDepthMask(GL_FALSE);

    draw_unit_quad(QUAD_IMMEDIATE);

    glDepthMask(GL_TRUE);
    glDisable(GL_TEXTURE_2D);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &tex);
        return 0;
    }

    /* Sixteen samples across the middle row. The texture is four texels wide over
     * PROBE_W pixels, so between neighbouring samples red can climb by at most a
     * quarter of its range plus a margin; anything larger is not a gradient. */
    int r[16];
    for (int i = 0; i < 16; i++) {
        r[i] = chan_r(px((i * 2 + 1) * PROBE_W / 32, PROBE_H / 2));
    }
    glDeleteTextures(1, &tex);

    int rises = 0, jumped = 0;
    for (int i = 1; i < 16; i++) {
        if (r[i] > r[i - 1])
            rises++;
        const int d = r[i] - r[i - 1];
        if (d > 48 || d < -48)
            jumped = 1;
    }
    if (gl1_probe_saw) {
        /* The first, middle and last of the row, which is the gradient in one word if
           it is one: low at the left, mid in the middle, high at the right. */
        gl1_probe_saw("tex-linear/ends",
                      ((uint32_t)r[0] << 16) | ((uint32_t)r[8] << 8) | (uint32_t)r[15]);
    }
    if (jumped)
        return 0; /* neighbouring samples far apart: noise, not a ramp */
    if (rises < 10)
        return 0; /* not climbing left to right at all */
    return 1;
}

/* **The sky draw, reproduced down to the texture unit.**
 *
 * `texture-unit1-alone` has covered unit 1 for a long time, with a 1x1 texture under
 * GL_REPLACE - a texture of one texel cannot show a per-pixel artifact, because every
 * texel in it is the same one. So the unit-1 path has only ever been exercised in the
 * single configuration where the fault being chased is invisible by construction.
 *
 * The port draws its sky through unit 1 with unit 0 *disabled*, which is what a
 * two-stage texture environment looks like: the first stage is staged but off, and the
 * real texture is on the second. Everything else here is that draw's own state, taken
 * from the log of it - a 16x128 gradient, GL_LINEAR, GL_MODULATE, a white vertex
 * colour, blending on, and no depth test, culling or lighting. On screen it comes back
 * as two images interleaved a column at a time.
 *
 * The verdict is the same shape one `tex-linear-stretch` uses, and for the same reason:
 * a magnified gradient must not change abruptly between neighbouring pixels.
 * Interleaved columns fail that on every pair.
 */
static int check_tex_unit1_stretch(void) {
    reset_view();

    static GLubyte img[16 * 128 * 4];
    for (int y = 0; y < 128; y++) {
        for (int x = 0; x < 16; x++) {
            GLubyte *t = img + (((size_t)y * 16u) + (size_t)x) * 4u;
            t[0] = (GLubyte)(x * 17); /* 0..255 across the width */
            t[1] = 64;
            t[2] = (GLubyte)(y * 2);
            t[3] = 255;
        }
    }

    /* Unit 0: a texture environment set up and then switched off, exactly as the port's
     * first stage is - so unit 1 is reached the way the port reaches it rather than on
     * its own. */
    glActiveTexture(GL_TEXTURE0);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glDisable(GL_TEXTURE_2D);

    glActiveTexture(GL_TEXTURE1);
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glColor3f(1.0f, 1.0f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDepthMask(GL_FALSE);

    /* The coordinate goes to unit 1, because that is the unit being sampled. */
    glBegin(GL_QUADS);
    glMultiTexCoord2f(GL_TEXTURE1, 0.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f, 0.0f);
    glMultiTexCoord2f(GL_TEXTURE1, 1.0f, 0.0f);
    glVertex3f(1.0f, -1.0f, 0.0f);
    glMultiTexCoord2f(GL_TEXTURE1, 1.0f, 1.0f);
    glVertex3f(1.0f, 1.0f, 0.0f);
    glMultiTexCoord2f(GL_TEXTURE1, 0.0f, 1.0f);
    glVertex3f(-1.0f, 1.0f, 0.0f);
    glEnd();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &tex);
        return 0;
    }

    /* Twenty neighbouring pixels across the middle. Sixteen texels over PROBE_W pixels
     * is eight pixels to a texel, so red climbs by about two per pixel; a jump of more
     * than forty is a different image, not a gradient. */
    int r[20];
    for (int i = 0; i < 20; i++) {
        r[i] = chan_r(px(PROBE_W / 4 + i, PROBE_H / 2));
    }
    glDeleteTextures(1, &tex);

    int worst = 0;
    for (int i = 1; i < 20; i++) {
        int d = r[i] - r[i - 1];
        if (d < 0)
            d = -d;
        if (d > worst)
            worst = d;
    }
    if (gl1_probe_saw) {
        gl1_probe_saw("tex-unit1/first3",
                      ((uint32_t)r[0] << 16) | ((uint32_t)r[1] << 8) | (uint32_t)r[2]);
        gl1_probe_saw("tex-unit1/worst-step", (uint32_t)worst);
    }
    return worst <= 40;
}

/* **A triangle wound the other way, which almost nothing ever draws.**
 *
 * The port's sky is the only surface it draws under a mirroring transform:
 * `glScalef(-BACK_DIST, BACK_DIST, -BACK_DIST)` negates two axes, which reverses the
 * winding of every triangle in the dome - and `back_draw` is also the one place that
 * turns culling off, so those reversed triangles are the only ones in the frame that
 * reach the rasteriser at all. Everywhere else in the title they would be discarded
 * before coverage was computed.
 *
 * That is exactly the population a sign error in an edge test would single out, and the
 * fault looks like one: whole polygons filled on every other column, bounded by their
 * own straight edges, while the surfaces around them fill solid.
 *
 * So this draws the same magnified gradient twice - once wound as every other check
 * here winds it, once reversed under a negative scale, both with culling off - and
 * requires the two to agree. Anything the rasteriser does differently to a
 * negative-area triangle shows up as the second one disagreeing with the first.
 */
static int winding_row(int reversed, int *out, int n) {
    static GLubyte img[16 * 128 * 4];
    for (int y = 0; y < 128; y++) {
        for (int x = 0; x < 16; x++) {
            GLubyte *t = img + (((size_t)y * 16u) + (size_t)x) * 4u;
            t[0] = (GLubyte)(x * 17);
            t[1] = 64;
            t[2] = (GLubyte)(y * 2);
            t[3] = 255;
        }
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor3f(1.0f, 1.0f, 1.0f);
    /* The sky's state, culling included - without this the reversed quad is discarded
     * and the check measures nothing at all. */
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDepthMask(GL_FALSE);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    if (reversed)
        glScalef(-1.0f, 1.0f, -1.0f); /* two axes negated, as the port's sky is */
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.0f, -1.0f, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.0f, 1.0f, 0.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.0f, 1.0f, 0.0f);
    glEnd();
    glPopMatrix();

    glDepthMask(GL_TRUE);
    glDisable(GL_TEXTURE_2D);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &tex);
        return 0;
    }
    for (int i = 0; i < n; i++)
        out[i] = chan_r(px(PROBE_W / 4 + i, PROBE_H / 2));
    glDeleteTextures(1, &tex);
    return 1;
}

static int check_tex_winding(void) {
    int fwd[20], rev[20];
    reset_view();
    if (!winding_row(0, fwd, 20))
        return 0;
    reset_view();
    if (!winding_row(1, rev, 20))
        return 0;

    /* Neither row may jump between neighbours, and the reversed one must not differ
     * from the forward one - a mirrored quad covering the same pixels should carry the
     * same gradient, because the scale negates x and the coordinates are symmetric
     * about the centre. What is being asked is not which texel lands where but whether
     * every pixel was covered. */
    int worst_f = 0, worst_r = 0, holes = 0;
    for (int i = 1; i < 20; i++) {
        int d = fwd[i] - fwd[i - 1];
        if (d < 0)
            d = -d;
        if (d > worst_f)
            worst_f = d;
        int e = rev[i] - rev[i - 1];
        if (e < 0)
            e = -e;
        if (e > worst_r)
            worst_r = e;
    }
    /* A column the reversed draw did not cover reads as the background it was drawn
     * over. */
    for (int i = 0; i < 20; i++) {
        if (rev[i] == 0 && fwd[i] != 0)
            holes++;
    }
    if (gl1_probe_saw) {
        gl1_probe_saw("tex-wind/worst-fwd", (uint32_t)worst_f);
        gl1_probe_saw("tex-wind/worst-rev", (uint32_t)worst_r);
        gl1_probe_saw("tex-wind/holes", (uint32_t)holes);
    }
    if (holes > 0)
        return 0;
    return worst_f <= 40 && worst_r <= 40;
}

/* **A textured quad blended over a textured quad**, which nothing here has ever drawn.
 *
 * `blend` covers blending with `glRectf` and no texture at all, and every textured
 * check draws one quad onto a cleared background. So the combination a real title uses
 * constantly - a texture blended over what a texture already put there - is untested in
 * both halves at once.
 *
 * It is also what separates the surfaces that render correctly in the port from the
 * ones that do not. Its planet and its glyphs are opaque draws and come back right; its
 * starfield, its menu panels and its floor are blended over what is behind them and
 * come back washed green.
 *
 * The arithmetic is made exact rather than approximate: both textures are flat, the
 * environment is GL_REPLACE so the texel reaches the blender untouched including its
 * alpha, and the source alpha is 128. The answer is therefore one number per channel
 * and any drift from it is the blender reading a destination that is not what was
 * drawn.
 */
static int check_blend_over_texture(void) {
    reset_view();

    /* Destination: opaque, and nothing like the source in any channel. */
    static GLubyte dst[4 * 4 * 4];
    for (int i = 0; i < 16; i++) {
        dst[i * 4 + 0] = 200;
        dst[i * 4 + 1] = 60;
        dst[i * 4 + 2] = 40;
        dst[i * 4 + 3] = 255;
    }
    /* Source: half transparent, and the destination's opposite - high where it is low.
     */
    static GLubyte src[4 * 4 * 4];
    for (int i = 0; i < 16; i++) {
        src[i * 4 + 0] = 40;
        src[i * 4 + 1] = 80;
        src[i * 4 + 2] = 220;
        src[i * 4 + 3] = 128;
    }

    GLuint tex[2] = {0, 0};
    glGenTextures(2, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     i ? src : dst);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    /* The destination, unblended - what a title's opaque geometry leaves behind. */
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, tex[0]);
    draw_unit_quad(QUAD_IMMEDIATE);

    /* And the source over it, blended - what its panels and its starfield are. */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, tex[1]);
    draw_unit_quad(QUAD_IMMEDIATE);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(2, tex);
        return 0;
    }

    /* src * 128/255 + dst * 127/255, a channel at a time. */
    const int want_r = (40 * 128 + 200 * 127) / 255; /* 119 */
    const int want_g = (80 * 128 + 60 * 127) / 255;  /* 69  */
    const int want_b = (220 * 128 + 40 * 127) / 255; /* 130 */
    /* **Counted, not sampled.** This read one centre pixel and passed on hardware while
       the region around it was three-quarters wrong - the centre is an even/even pixel
       and a combining blend is correct only there. */
    const uint32_t got = px(PROBE_W / 2, PROBE_H / 2);
    const int bad = census_wrong(16, 16, 64, 48, want_r, want_g, want_b, 12);
    glDeleteTextures(2, tex);

    if (gl1_probe_saw) {
        gl1_probe_saw("blend-tex/got", got);
        gl1_probe_saw("blend-tex/want", ((uint32_t)want_r << 16) |
                                            ((uint32_t)want_g << 8) | (uint32_t)want_b);
        gl1_probe_saw("blend-tex/wrong-of-3072", (uint32_t)bad);
    }
    return bad == 0;
}

/* **A lit textured surface, and a verdict shaped like the artifact.**
 *
 * `lighting`, `two-lights` and `color-material` all light untextured geometry, and
 * every textured check here draws with lighting off. So a lit *textured* surface -
 * which is every piece of level geometry in every 3D title - has never been drawn by
 * this suite at all.
 *
 * It is also the axis that separates what renders correctly in the port from what does
 * not. Its planet and its glyphs are unlit and come back clean; its floor is lit level
 * geometry and comes back with alternate pixel columns brighter by a constant 72 on
 * every channel, measured twice from a console capture. A uniform monochrome addition
 * is what a lighting term looks like.
 *
 * So the verdict is parity rather than smoothness: sample a run of neighbouring pixels,
 * average the even ones and the odd ones, and require the two to agree. A surface
 * shaded by a light must not care which column a pixel is in, and nothing else here
 * would notice if it did.
 */
static int check_lit_texture_parity(void) {
    reset_view();

    static GLubyte img[32 * 32 * 4];
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            GLubyte *t = img + (((size_t)y * 32u) + (size_t)x) * 4u;
            t[0] = 180;
            t[1] = 180;
            t[2] = 200;
            t[3] = 255; /* flat, so any variation is ours */
        }
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    /* One light and a material with a specular term, because a constant added to a
     * surface is what the specular stage contributes and it is the part that reaches
     * the colour sum. */
    static const GLfloat lpos[4] = {0.0f, 0.0f, 1.0f, 0.0f};
    static const GLfloat white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    static const GLfloat grey[4] = {0.4f, 0.4f, 0.4f, 1.0f};
    /* **Half-strength diffuse, so the total cannot come out at one.** With a white
       light this check's own numbers summed to exactly 1.0 - ambient 0.04 plus diffuse
       0.8 plus a specular of 0.4 x 0.4 at normal incidence - and returned the texel
       unchanged, which reads exactly like lighting having done nothing. It had done
       something; the something was invisible. At 0.5 the shade lands near 0.6 of the
       texel and cannot be confused with an unlit one. */
    static const GLfloat half[4] = {0.5f, 0.5f, 0.5f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lpos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, half);
    glLightfv(GL_LIGHT0, GL_SPECULAR, grey);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, grey);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 16.0f);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);

    glBegin(GL_QUADS);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.0f, -1.0f, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.0f, 1.0f, 0.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.0f, 1.0f, 0.0f);
    glEnd();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    glDisable(GL_TEXTURE_2D);
    /* **Put the material and the light back**, which the first version did not: running
       this check first made `evaluators` fail, because a specular term and a shininess
       left behind change every lit surface drawn after them. `reset_view` restores the
       enables and not these. The defaults are GL 1.x's own - material specular black,
       shininess zero, and LIGHT0's specular and diffuse white. */
    static const GLfloat black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 0.0f);
    glLightfv(GL_LIGHT0, GL_SPECULAR, white);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, white);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &tex);
        return 0;
    }

    int even = 0, odd = 0;
    for (int i = 0; i < 32; i++) {
        const int v = chan_g(px(PROBE_W / 4 + i, PROBE_H / 2));
        if (i & 1)
            odd += v;
        else
            even += v;
    }
    glDeleteTextures(1, &tex);
    even /= 16;
    odd /= 16;
    int d = even - odd;
    if (d < 0)
        d = -d;
    if (gl1_probe_saw) {
        gl1_probe_saw("lit-tex/even-odd", ((uint32_t)even << 8) | (uint32_t)odd);
        gl1_probe_saw("lit-tex/delta", (uint32_t)d);
    }
    /* **The light has to have done something, or this measured nothing.**
     *
     * The first run returned 180 and 180 - exactly the texture's own green, which is
     * also what an unlit modulate by a white primary colour returns. That looked like
     * the light having been ignored, and it was not: with a white light the terms
     * summed to one, ambient 0.04 plus diffuse 0.8 plus specular 0.16 at normal
     * incidence, so a correctly lit surface returned the texel unchanged. The light
     * above is half strength now for that reason.
     *
     * The assertion stays, because the reading it guards against is real even if that
     * instance of it was not: a parity verdict over a surface no light reached would
     * pass for the wrong reason, and three checks in this suite have already been found
     * passing for exactly that kind of reason. A result at full texel strength is
     * reported as this check failing rather than as the hardware passing. */
    if (even >= 176) {
        if (gl1_probe_saw)
            gl1_probe_saw("lit-tex/unlit-result", (uint32_t)even);
        return 0;
    }
    return d <= 8;
}

/* **The starfield's own draw, taken from a recording of it.**
 *
 * A capture of the port's frame, decoded, says exactly how the surface that comes back
 * green is drawn - and it is a combination nothing here has ever used:
 *
 *     ENABLE GL_CULL_FACE, ENABLE GL_DEPTH_TEST, DEPTH_MASK 0, LIGHTING off
 *     BIND_TEXTURE GL_TEXTURE_2D 79
 *     BLEND_FUNC GL_SRC_ALPHA, GL_ONE
 *     BEGIN GL_TRIANGLE_STRIP        x65, every coordinate to GL_TEXTURE1
 *
 * `GL_SRC_ALPHA, GL_ONE` is additive with an alpha-scaled source, and it is not tested
 * here: `blend` and `blend-over-texture` use ONE_MINUS_SRC_ALPHA, and the two checks
 * that do use GL_ONE as a destination factor pair it with GL_ONE as the source and so
 * never scale by alpha at all. Additive accumulates rather than replaces, which is the
 * shape of the artifact being chased - alternate columns of the port's floor brighter
 * by a constant 72 on every channel.
 *
 * Sixty-five overlapping primitives is the other half. One additive quad tests the
 * arithmetic; two test whether it accumulates the way the specification says, which is
 * what a field of overlapping stars relies on. The numbers are chosen so neither pass
 * clamps: a destination of (40, 60, 80), a source of (100, 120, 140) at alpha 128, so
 * one pass gives (90, 120, 150) and two give (140, 180, 220).
 */
static int check_blend_additive_strip(void) {
    reset_view();

    static GLubyte dst[4 * 4 * 4];
    for (int i = 0; i < 16; i++) {
        dst[i * 4 + 0] = 40;
        dst[i * 4 + 1] = 60;
        dst[i * 4 + 2] = 80;
        dst[i * 4 + 3] = 255;
    }
    static GLubyte src[4 * 4 * 4];
    for (int i = 0; i < 16; i++) {
        src[i * 4 + 0] = 100;
        src[i * 4 + 1] = 120;
        src[i * 4 + 2] = 140;
        src[i * 4 + 3] = 128;
    }
    GLuint tex[2] = {0, 0};
    glGenTextures(2, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     i ? src : dst);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    /* The destination, opaque and unblended. */
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, tex[0]);
    draw_unit_quad(QUAD_IMMEDIATE);

    /* And the additive passes, in the state the recording shows: culled, depth tested,
     * depth writes off, as a triangle strip. Counter-clockwise, so the front face
     * survives culling - a back-facing strip would be discarded and this would measure
     * the destination twice. */
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBindTexture(GL_TEXTURE_2D, tex[1]);
    for (int pass = 0; pass < 2; pass++) {
        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(-1.0f, -1.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(1.0f, -1.0f, 0.0f);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(-1.0f, 1.0f, 0.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(1.0f, 1.0f, 0.0f);
        glEnd();
    }
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(2, tex);
        return 0;
    }

    /* dst + 2 * src * 128/255, a channel at a time. */
    const int want_r = 40 + 2 * (100 * 128 / 255); /* 140 */
    const int want_g = 60 + 2 * (120 * 128 / 255); /* 180 */
    const int want_b = 80 + 2 * (140 * 128 / 255); /* 220 */
    /* Counted, for the reason `blend-over-texture` is: additive is a combining blend,
       and a combining blend on this part is right at one pixel in four. */
    const uint32_t got = px(PROBE_W / 2, PROBE_H / 2);
    const int bad = census_wrong(16, 16, 64, 48, want_r, want_g, want_b, 12);
    glDeleteTextures(2, tex);

    if (gl1_probe_saw) {
        gl1_probe_saw("blend-add/got", got);
        gl1_probe_saw("blend-add/want", ((uint32_t)want_r << 16) |
                                            ((uint32_t)want_g << 8) | (uint32_t)want_b);
        gl1_probe_saw("blend-add/wrong-of-3072", (uint32_t)bad);
    }
    return bad == 0;
}

static int check_tex_state_leak(void) {
    g_readback_dirty = 1;
    const int ok =
        readback_at_width("tex-leak/w4", "tex-leak/w4-rgb", 4, 0, QUAD_IMMEDIATE);
    g_readback_dirty = 0;
    return ok;
}
/* **At 64 on purpose**, which is the width whose rows are already 256-byte aligned and
 * whose descriptor therefore carries no custom pitch. Running the combine over the one
 * texture shape that cannot have a stride fault means a failure here is the combine's
 * and nothing else's - and with `tex-readback-w64` passing over the identical image
 * under GL_REPLACE, the pair says which of the two halves of the reported fault is
 * real. */
static int check_tex_readback_modulate(void) {
    return readback_at_width("tex-readback/mod", "tex-readback/mod-rgb", 64, 1,
                             QUAD_IMMEDIATE);
}

/* **Past the end of both rings, which is where this port lives and the suite does
 * not.**
 *
 * Every check here draws a handful of quads with one or two textures and a state that
 * barely moves. Neverball has eighty-five textures and changes the texture environment
 * between most of its draws, and the two caches that absorb that are sized 64 and 6.
 * Nothing in this suite has ever reached either limit, so the wrap has never been
 * tested by anything except the port that reports the bug - and a cache that returns
 * the wrong entry when it wraps looks exactly like a texture being sampled wrongly,
 * which is what sent this investigation at the sampler for a day.
 *
 * Both are laid out as a grid of cells with one draw each, so a wrong answer says
 * *which* draw was wrong rather than only that the frame was.
 */

#define CHURN_TEX 80 /* > 64: OOPS_GL_DESC_RING_SLOTS is 63 plus slot 0 */
#define CHURN_COLS 10
#define CHURN_ROWS 8

static int check_tex_churn_ring(void) {
    reset_view();
    glClearColor(((float)((PROBE_BG >> 16) & 0xffu)) / 255.0f,
                 ((float)((PROBE_BG >> 8) & 0xffu)) / 255.0f,
                 ((float)(PROBE_BG & 0xffu)) / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    /* Eighty textures, each a flat colour that names itself: red counts up with the
     * index and green counts down, so a cell showing another texture's colour
     * identifies which one it got rather than merely being wrong. */
    static GLuint tex[CHURN_TEX];
    glGenTextures(CHURN_TEX, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (int i = 0; i < CHURN_TEX; i++) {
        GLubyte texels[4 * 4 * 4];
        for (int t = 0; t < 16; t++) {
            texels[t * 4 + 0] = (GLubyte)(i * 3);
            texels[t * 4 + 1] = (GLubyte)(255 - i * 3);
            texels[t * 4 + 2] = 128;
            texels[t * 4 + 3] = 255;
        }
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     texels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor3f(1.0f, 1.0f, 1.0f);

    /* All eighty in one frame, which is the point: the ring holds 64 and must submit
     * partway through rather than quietly reusing a slot a queued draw still names. */
    for (int i = 0; i < CHURN_TEX; i++) {
        const int c = i % CHURN_COLS;
        const int r = i / CHURN_COLS;
        const float x0 = -1.0f + 2.0f * (float)c / (float)CHURN_COLS;
        const float x1 = -1.0f + 2.0f * (float)(c + 1) / (float)CHURN_COLS;
        const float y1 = 1.0f - 2.0f * (float)r / (float)CHURN_ROWS;
        const float y0 = 1.0f - 2.0f * (float)(r + 1) / (float)CHURN_ROWS;
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(x0, y0, 0.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(x1, y0, 0.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(x1, y1, 0.0f);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(x0, y1, 0.0f);
        glEnd();
    }
    glDisable(GL_TEXTURE_2D);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(CHURN_TEX, tex);
        return 0;
    }

    int bad = -1, saw_r = 0;
    for (int i = 0; i < CHURN_TEX && bad < 0; i++) {
        const int c = i % CHURN_COLS;
        const int r = i / CHURN_COLS;
        const uint32_t p = px((c * 2 + 1) * PROBE_W / (CHURN_COLS * 2),
                              (r * 2 + 1) * PROBE_H / (CHURN_ROWS * 2));
        if (!near_rgb(p, i * 3, 255 - i * 3, 128, 10)) {
            bad = i;
            saw_r = chan_r(p);
        }
    }
    glDeleteTextures(CHURN_TEX, tex);
    if (bad >= 0) {
        /* Which cell, and which texture's colour turned up in it - red divided by three
         * is the index that was actually sampled, so the two numbers together say how
         * far the ring slipped rather than only that it did. */
        if (gl1_probe_saw) {
            gl1_probe_saw("tex-churn/first-bad", (uint32_t)bad);
            gl1_probe_saw("tex-churn/saw-index", (uint32_t)(saw_r / 3));
        }
        return 0;
    }
    return 1;
}

/* Eight distinct textured shaders against six ring slots, drawn twice through so a
 * variant that was evicted has to be rebuilt and is checked again after it was.
 *
 * The alpha test is what varies, because its eight comparisons are eight different
 * patches of the same shader and the result of each is not a colour to be measured but
 * a question of whether the quad is there at all. A fragment alpha of 0.75 against a
 * reference of 0.5 makes the first four comparisons reject and the last four pass, with
 * no float equality anywhere near the boundary. */
static int check_ps_ring_churn(void) {
    reset_view();
    glClearColor(((float)((PROBE_BG >> 16) & 0xffu)) / 255.0f,
                 ((float)((PROBE_BG >> 8) & 0xffu)) / 255.0f,
                 ((float)(PROBE_BG & 0xffu)) / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    static const GLenum func[8] = {
        GL_NEVER,   GL_LESS,     GL_EQUAL,  GL_LEQUAL, /* reject at 0.75 vs 0.5 */
        GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS, /* pass */
    };
    GLubyte texels[4 * 4 * 4];
    for (int t = 0; t < 16; t++) {
        texels[t * 4 + 0] = 204;
        texels[t * 4 + 1] = 51;
        texels[t * 4 + 2] = 102;
        texels[t * 4 + 3] = 191; /* 191/255 = 0.749 */
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor3f(1.0f, 1.0f, 1.0f);
    glEnable(GL_ALPHA_TEST);

    for (int i = 0; i < 16; i++) {
        const int c = i % 4;
        const int r = i / 4;
        const float x0 = -1.0f + 2.0f * (float)c / 4.0f;
        const float x1 = -1.0f + 2.0f * (float)(c + 1) / 4.0f;
        const float y1 = 1.0f - 2.0f * (float)r / 4.0f;
        const float y0 = 1.0f - 2.0f * (float)(r + 1) / 4.0f;
        glAlphaFunc(func[i % 8], 0.5f);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(x0, y0, 0.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(x1, y0, 0.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(x1, y1, 0.0f);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(x0, y1, 0.0f);
        glEnd();
    }
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &tex);
        return 0;
    }

    /* One bit per cell, in the order drawn: 1 where the quad is there and 0 where the
     * test rejected it. The two passes must agree, and both must be 0x0f - four rejects
     * then four passes - so a single word carries the whole shape of the failure. */
    uint32_t bits = 0u;
    int ok = 1;
    for (int i = 0; i < 16; i++) {
        const int c = i % 4;
        const int r = i / 4;
        const uint32_t p = px((c * 2 + 1) * PROBE_W / 8, (r * 2 + 1) * PROBE_H / 8);
        const int drew = near_rgb(p, 204, 51, 102, 10);
        const int blank = near_rgb(p, 32, 32, 32, 10);
        if (!drew && !blank)
            ok = 0; /* neither the texture nor the background: a third answer */
        if (drew)
            bits |= 1u << i;
        if (drew != ((i % 8) >= 4))
            ok = 0;
    }
    glDeleteTextures(1, &tex);
    if (!ok && gl1_probe_saw)
        gl1_probe_saw("ps-ring/cells", bits);
    return ok;
}

/* **Which stage of the blender is wrong.**
 *
 * Everything measured so far says one thing in one shape: a blend whose result
 * *combines* both terms is right at one pixel in every 2x2 quad and wrong at the other
 * three, a blend whose result is a single operand is right everywhere, and an unblended
 * write is right everywhere across four thousand pixels. That is a single fact about
 * "blending" - and blending is three separable stages: the **read** of the destination,
 * the **multiply** of each term by its factor, and the **add** that combines them.
 * `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA` uses all three, so a verdict on it cannot say
 * which one is at fault, and every register tried against it so far has moved the count
 * by nothing or by everything.
 *
 * So this draws the same two colours under eight factor pairs chosen to use the stages
 * one at a time, and censuses each over the same 3072 pixels:
 *
 *   zero-one   result is the destination unchanged  - the read, alone
 *   one-zero   result is the source unchanged       - the export, alone
 *   one-one    source plus destination              - the add, with both factors
 * trivial sa-zero    source scaled by its own alpha       - the source multiply, alone
 *   zero-sa    destination scaled by source alpha   - the destination multiply, alone
 *   isa-zero   source scaled by one minus alpha     - the inverted factor, alone
 *   sa-one     the port's starfield
 *   sa-isa     the port's panels and floor
 *
 * **Read the counts, not the verdict.** `zero-one` wrong means the destination is not
 * being read correctly and nothing after it can be trusted. The two multiply rows wrong
 * with `one-one` clean means the factor arithmetic. `one-one` wrong with every
 * single-operand row clean means both operands arrive intact and the add is where they
 * are lost - which is a different repair, in a different register, from either of the
 * others.
 *
 * Each row also reports the two lanes of one quad: `/even` is (64,48), the lane that
 * works, and
 * `/odd` is (65,48), a lane that does not. What the broken lane *contains* narrows it
 * further - the source alone, the destination alone, or neither - and a count without
 * that value has cost a hardware run more than once.
 */
static int check_blend_factor_matrix(void) {
    /* **No expected channel may clamp and no two rows may expect the same triple**, or
     * a row cannot be told from its neighbour in the log. Source alpha is 0.25 rather
     * than 0.5 so that `GL_SRC_ALPHA` and `GL_ONE_MINUS_SRC_ALPHA` are two different
     * numbers instead of the same number twice - at 0.5 the `isa-zero` row would be a
     * copy of `sa-zero` and would prove nothing about the subtraction. */
    static const float D[3] = {0.20f, 0.40f, 0.70f};
    static const float S[3] = {0.60f, 0.30f, 0.10f};
    static const float A = 0.25f;

    static const struct {
        const char *name;
        GLenum sf, df;
        float fs, fd; /* what those factors are worth here, for the expectation */
    } cases[] = {
        {"zero-one", GL_ZERO, GL_ONE, 0.00f, 1.00f},
        {"one-zero", GL_ONE, GL_ZERO, 1.00f, 0.00f},
        {"one-one", GL_ONE, GL_ONE, 1.00f, 1.00f},
        {"sa-zero", GL_SRC_ALPHA, GL_ZERO, 0.25f, 0.00f},
        {"zero-sa", GL_ZERO, GL_SRC_ALPHA, 0.00f, 0.25f},
        {"isa-zero", GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, 0.75f, 0.00f},
        {"sa-one", GL_SRC_ALPHA, GL_ONE, 0.25f, 1.00f},
        {"sa-isa", GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, 0.25f, 0.75f},
    };

    int bad_total = 0;

    for (unsigned i = 0u; i < sizeof(cases) / sizeof(cases[0]); i++) {
        /* The destination is laid down unblended, so whatever the previous row left is
           gone and the only thing the blended draw can be reading is this rectangle. */
        reset_view();
        glDisable(GL_BLEND);
        draw_rect(-0.9f, -0.9f, 0.9f, 0.9f, D[0], D[1], D[2]);

        glEnable(GL_BLEND);
        glBlendFunc(cases[i].sf, cases[i].df);
        glColor4f(S[0], S[1], S[2], A);
        glRectf(-0.9f, -0.9f, 0.9f, 0.9f);
        glDisable(GL_BLEND);
        if (glGetError() != GL_NO_ERROR)
            return 0;

        int want[3];
        for (int c = 0; c < 3; c++) {
            float v = S[c] * cases[i].fs + D[c] * cases[i].fd;
            if (v < 0.0f)
                v = 0.0f;
            if (v > 1.0f)
                v = 1.0f;
            want[c] = (int)(v * 255.0f + 0.5f);
        }

        const int bad = census_wrong(16, 16, 64, 48, want[0], want[1], want[2], 12);
        bad_total += bad;

        if (gl1_probe_saw) {
            /* `saw` copies the name before it returns, so one buffer serves every row -
               and it keeps 24 characters, which is what holds these names to a short
               prefix. */
            /* **All four lanes of one quad, not two.** The first run of this check
               reported the base lane and the lane at x+1 only, and could not explain
               why `isa-zero` came back 1536 wrong where every other engaging row came
               back 2304: half the region rather than three quarters means two lanes of
               that quad were right, and which two is not a question two samples can
               answer. */
            static const char *const suffix[6] = {"/wrong", "/want", "/even",
                                                  "/odd",   "/oddy", "/oddxy"};
            for (int k = 0; k < 6; k++) {
                char n[32];
                int at = 0;
                n[at++] = 'b';
                n[at++] = 'f';
                n[at++] = '-';
                for (int j = 0; cases[i].name[j] && at < 20; j++)
                    n[at++] = cases[i].name[j];
                for (int j = 0; suffix[k][j] && at < 30; j++)
                    n[at++] = suffix[k][j];
                n[at] = '\0';

                uint32_t v;
                if (k == 0) {
                    v = (uint32_t)bad;
                } else if (k == 1) {
                    v = ((uint32_t)want[0] << 16) | ((uint32_t)want[1] << 8) |
                        (uint32_t)want[2];
                } else if (k == 2) {
                    v = px(PROBE_W / 2, PROBE_H / 2); /* both even - the base lane */
                } else if (k == 3) {
                    v = px(PROBE_W / 2 + 1, PROBE_H / 2); /* x odd */
                } else if (k == 4) {
                    v = px(PROBE_W / 2, PROBE_H / 2 + 1); /* y odd */
                } else {
                    v = px(PROBE_W / 2 + 1, PROBE_H / 2 + 1); /* both odd */
                }
                gl1_probe_saw(n, v);
            }

            /* **Sixteen consecutive words, for the last row only.** Four samples of one
             * quad established that the three non-base lanes get their middle two bytes
             * and nothing else; they cannot show whether the bytes that go missing turn
             * up in a neighbour, which is the difference between a displaced write and
             * a partial one. Two runs of eight across four whole quads can, and this is
             * the port's own blend
             * (`GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`), so the dump is of the case that
             * matters. Every pixel here wants the same colour, so any byte that differs
             * between columns came from somewhere it should not have. */
            if (i == sizeof(cases) / sizeof(cases[0]) - 1u) {
                for (int yy = 0; yy < 2; yy++) {
                    for (int xx = 0; xx < 8; xx++) {
                        char n[32];
                        int at = 0;
                        const char *p = "bf-dump/y";
                        for (int j = 0; p[j]; j++)
                            n[at++] = p[j];
                        n[at++] = (char)('0' + yy);
                        n[at++] = 'x';
                        n[at++] = (char)('0' + (60 + xx) / 10);
                        n[at++] = (char)('0' + (60 + xx) % 10);
                        n[at] = '\0';
                        gl1_probe_saw(n, px(60 + xx, PROBE_H / 2 + yy));
                    }
                }
            }
        }
    }

    /* The colour is left where the rest of the suite expects to find it; `check_blend`
       not doing this is why `evaluators` once depended on what ran before it. */
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    return bad_total == 0;
}

/* ------------------------------------------------------------------------- */

static const gl1_probe_case_t g_cases[] = {
    /* **First, because the log is what this is read through.** A parked title emits its
     * burst once and then goes silent, and a follow window that closes early loses
     * whatever had not been printed - three runs of this check were lost that way, at
     * around two hundred lines. A check whose result is the reason for the run goes
     * where the window certainly reaches. */
    /* First of all, because it is the reason for the run: it is the only check that can
       say which stage of the blender is wrong, and its thirty-two rows have to clear
       the window. */
    {"blend-factor-matrix", check_blend_factor_matrix},
    {"lit-texture-parity", check_lit_texture_parity},
    /* Second, for the same reason as the first: the recording of the port's frame says
     * its starfield is drawn this way, and no check here had drawn one. */
    {"blend-additive-strip", check_blend_additive_strip},
    {"clear-and-rect", check_clear_and_rect},
    {"scissor", check_scissor},
    {"clip-plane", check_clip_plane},
    {"texgen", check_texgen},
    {"stencil", check_stencil},
    {"raster-ops", check_raster_ops},
    {"points-and-lines", check_points_and_lines},
    {"fog", check_fog},
    {"fog-coord", check_fog_coord},
    {"logic-op", check_logic_op},
    {"blend-constant", check_blend_constant},
    {"depth-range-in-frame", check_depth_range_in_frame},
    {"polygon-mode", check_polygon_mode},
    {"mipmap-levels", check_mipmap_levels},
    {"evaluators", check_evaluators},
    {"selection", check_selection},
    {"texture-matrix", check_texture_matrix},
    {"pixel-transfer", check_pixel_transfer},
    {"pixel-fragments", check_pixel_fragments},
    {"point-params", check_point_params},
    {"lod-bias", check_lod_bias},
    {"projective-texture", check_projective_texture},
    {"stencil-pixels", check_stencil_pixels},
    {"depth-readback", check_depth_readback},
    {"stencil-readback", check_stencil_readback},
    {"front-buffer", check_front_buffer},
    {"front-and-back", check_front_and_back},
    {"index-pixels", check_index_pixels},
    {"buffer-map", check_buffer_map},
    {"occlusion-query", check_occlusion_query},
    {"shadow-compare", check_shadow_compare},
    {"line-stipple", check_line_stipple},
    {"polygon-stipple", check_polygon_stipple},
    {"accumulation", check_accumulation},
    {"scissored-clear", check_scissored_clear},
    {"texture-3d", check_texture_3d},
    {"pixel-types", check_pixel_types},
    {"internal-formats", check_internal_formats},
    {"border-and-mirror", check_border_and_mirror},
    {"lod-params", check_lod_params},
    {"cube-map", check_cube_map},
    {"combine", check_combine},
    {"many-textures", check_many_textures},
    {"tex-env-blend-decal", check_tex_env_blend_decal},
    {"smooth", check_smooth},
    {"array-types", check_array_types},
    {"colour-sum", check_color_sum},
    {"rescale-normal", check_rescale_normal},
    {"separate-specular", check_separate_specular},
    {"many-triangles", check_many_triangles},
    {"colour-mask", check_colour_mask},
    {"blend", check_blend},
    {"depth-test", check_depth},
    {"vertex-arrays", check_vertex_arrays},
    {"buffer-objects", check_buffer_objects},
    {"display-list", check_display_list},
    {"texture-2d", check_texture},
    {"texture-luminance", check_texture_luminance},
    {"read-pixels", check_read_pixels},
    {"matrix-stack", check_matrix_stack},
    {"attrib-stack", check_attrib_stack},
    {"alpha-test", check_alpha_test},
    {"alpha-test-depth", check_alpha_test_frees_depth},
    {"lighting", check_lighting},
    {"tex-env-modes", check_tex_env_modes},
    {"copy-tex", check_copy_tex},
    {"get-tex-image", check_get_tex_image},
    {"array-paths", check_array_paths},
    {"type-variants", check_type_variants},
    {"client-attrib", check_client_attrib},
    {"depth-range", check_depth_range},
    {"object-queries", check_object_queries},
    {"polygon-offset", check_polygon_offset},
    {"blend-equation", check_blend_equation},
    {"two-side", check_two_side},
    {"cull-face", check_cull_face},
    {"viewport", check_viewport},
    {"depth-mask", check_depth_mask},
    {"texture-wrap", check_texture_wrap},
    {"tex-sub-image", check_tex_sub_image},
    {"two-lights", check_two_lights},
    {"refusals", check_refusals},
    {"limits", check_limits_reported},
    /* The three widths together, and in this order: 64 is the control that needs no
     * custom pitch, so a run where it passes and the other two fail has named the fault
     * outright. */
    {"tex-readback-w64", check_tex_readback_w64},
    {"tex-readback-w16", check_tex_readback_w16},
    {"tex-readback-w4", check_tex_readback_w4},
    /* The same image again with the combine switched on, so the pair separates a
     * texture that was sampled wrongly from one that was sampled correctly and coloured
     * wrongly. */
    {"tex-readback-modulate", check_tex_readback_modulate},
    /* The two caches nothing else here reaches the end of, and the port does on every
       frame. */
    /* The same textured quad an array and a buffer object at a time - the way every
     * triangle in a real title arrives, and the one submission path no textured check
     * here had used.
     *
     * **Ahead of the two churn checks, not after them.** `ps-ring-churn` hung on
     * 2026-09-23 having passed on the run before it, waiting on a fence that never
     * signalled, and took these two unrun with it - which is the same way
     * `multitexture` cost thirty-nine rows on 2026-09-20. A check that has hung once is
     * a check everything else goes in front of. */
    {"tex-array-draw", check_tex_array_draw},
    {"tex-vbo-draw", check_tex_vbo_draw},
    /* State that was set and then unset, which every other check here starts clean of.
     */
    {"tex-state-leak", check_tex_state_leak},
    /* A magnified 4x128 gradient under GL_LINEAR: the port's sky, which no check here
       had. */
    {"tex-linear-stretch", check_tex_linear_stretch},
    /* The same magnified gradient through unit 1 with unit 0 off - the port's own
     * arrangement, and the one the existing unit-1 check cannot see a per-pixel fault
     * in. */
    {"tex-unit1-stretch", check_tex_unit1_stretch},
    /* A mirrored, reverse-wound triangle with culling off - the one population the
     * port's sky belongs to and nothing else in a frame does. */
    {"tex-winding", check_tex_winding},
    /* A texture blended over a texture - what the port's panels, starfield and floor
     * are, and what separates them from the planet and the glyphs that come back
     * correct. */
    {"blend-over-texture", check_blend_over_texture},
    {"tex-churn-ring", check_tex_churn_ring},
    {"ps-ring-churn", check_ps_ring_churn},
    /* **Last on purpose**, both of them: a check that can take the GPU down costs its
     * own row and every row after it, because the fault kills the process and the suite
     * stops there.
     *
     * - `tex-delete-in-frame` has done it before - see the check itself.
     * - `multitexture` did it on 2026-09-20, on the first console run that got past the
     *   `raster-ops` stall: `ILLEGAL_INST` on two waves at one PC, then
     *   `GPU_FAULT_WAVEFRONT_ERROR_ASYNC` and a GPU reset. It sat between
     *   `tex-env-blend-decal` and `smooth` and took thirty-nine unrun checks with it,
     * which is the whole reason for this ordering. Moving it is not a fix and does not
     * pretend to be one; the fault is real and the run that resolves it wants the other
     * rows as well.
     */
    {"smooth-textured", check_smooth_textured},
    {"polygon-smooth", check_polygon_smooth},
    {"volume-mipmap", check_volume_mipmap},
    {"multitexture", check_multitexture},
    {"texture-unit1-alone", check_texture_unit1_alone},
    {"point-sprite", check_point_sprite},
    {"color-material", check_color_material},
    {"texture-env-shadow-weight", check_texture_env_shadow_weight},
    {"texture-unit1-coords", check_texture_unit1_coords},
    {"texture-env-shadow-stack", check_texture_env_shadow_stack},
    {"tex-delete-in-frame", check_tex_delete_in_frame},
};

_Static_assert(
    sizeof(g_cases) / sizeof(g_cases[0]) <= GL1_PROBE_MAX_CASES,
    "more checks than GL1_PROBE_MAX_CASES: the callers' result arrays would drop some");

/* NULL unless a caller wants a running commentary - the payload does, the host
 * self-test does not. See the header for why a hang made this necessary. */
void (*gl1_probe_trace)(const char *name, int verdict) = (void (*)(const char *, int))0;

/* NULL unless a caller wants the pixel behind a failure - see the header. */
void (*gl1_probe_saw)(const char *name,
                      uint32_t centre) = (void (*)(const char *, uint32_t))0;

int gl1_probe_case_count(void) {
    return (int)(sizeof(g_cases) / sizeof(g_cases[0]));
}

const char *gl1_probe_case_name(int i) {
    if (i < 0 || i >= gl1_probe_case_count())
        return "?";
    return g_cases[i].name;
}

int gl1_probe_run(gl1_probe_result_t *out, int max) {
    /* **A diagnostic asks for the diagnostics.** The per-frame and per-submit counters
       are off by default since 2026-09-24, because a title submitting thirty times a
       frame writes them thousands of times a second and buries everything else in the
       log - which is how an evening went looking for a title's own messages under them.
       This suite is the tool those counters exist for, so it turns them on. */
#ifndef OOPS_HOST_BUILD
    oops_gl_set_log_level((int)OOPS_LOG_DEBUG);
#endif
    g_disp =
        oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, PROBE_DISPLAY_W, PROBE_DISPLAY_H);
    if (!g_disp)
        return -1;
    /* **A display that could not open is returned, not hidden** - `oops/display.h` says
     * so, and this did not ask. On hardware that meant a NULL framebuffer that looked
     * like a working one until the first check read it. gl1-cube has always checked;
     * this now does too. */
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
    /* The probe's viewport sits at GL's origin, so its region is the bottom PROBE_H
     * rows of the framebuffer - the rasteriser flips y against the full height. */
    g_row0 = g_fb_h - PROBE_H;

    const int n = gl1_probe_case_count();
    for (int i = 0; i < n && i < max; i++) {
        /* Errors are cleared between checks so one failure cannot cascade into the next
         * and make a single bug look like a dozen. */
        (void)glGetError();
        out[i].name = g_cases[i].name;
        /* Named on the way in, so a check that never returns is still named - see
         * `gl1_probe_trace` in the header for the run that made this necessary. */
        if (gl1_probe_trace)
            gl1_probe_trace(g_cases[i].name, -1);
        out[i].passed = g_cases[i].fn();
        if (gl1_probe_trace)
            gl1_probe_trace(g_cases[i].name, out[i].passed);
        /* After the verdict, so the row reads name, verdict, value - and only on a
         * failure, one pixel, because reading one costs a full synchronisation. See the
         * header. */
        if (!out[i].passed && gl1_probe_saw) {
            gl1_probe_saw(g_cases[i].name, px(PROBE_W / 2, PROBE_H / 2));
        }
    }

    /* **Kept open when the caller is going to paint the screen**, because the context
       and the display are what it would paint with and closing them here takes both
       away. The payload parks after the card and never returns, so nothing is leaked
       that outlives the process; the host self-test leaves the flag alone and closes as
       it always did. */
    if (gl1_probe_keep_context) {
        g_ctx = ctx;
        return n < max ? n : max;
    }
    glContextDestroy(ctx);
    oops_display_close(g_disp);
    g_disp = (oops_display_t *)0;
    g_fb = (uint32_t *)0;
    return n < max ? n : max;
}

/* **A picture for the one instrument this suite has never used: the television.**
 *
 * Every check here decides by reading pixels back, and on hardware they now all pass -
 * the sampler fetches the right texel at every width and the combine multiplies it
 * correctly. The port those checks were written for still renders wrongly on screen.
 * Both of those can only be true at once if what reaches the display is not what the
 * checks read back, and no check can see that, because a check and the display read
 * through different paths.
 *
 * So this paints something whose correct appearance needs no judgement. Flat bands in
 * the primaries and in mid grey, full width, because vertical banding shows against a
 * flat fill and a colour cast shows against grey; and a black-to-white ramp, because
 * quantisation shows in a gradient and nowhere else. Then it reads one pixel out of
 * each band and logs it.
 *
 * The two halves are the measurement. If the log says a band holds 0x808080 and the
 * screen shows it tinted or striped, the frame is correct in memory and the fault is
 * between there and the panel - which is a different subsystem from the one this suite
 * covers, and would explain why every check passes while the port looks broken.
 */
/* What the frame holds, read back the way every check reads. One sample per flat band,
 * taken at a quarter width so it is clear of any edge, and three across the ramp.
 *
 * **Row 0 is the top here**, which is the opposite of what this first assumed. The
 * first run reported the bands in reverse with the ramp where the grey should have been
 * - every value exactly right and every label wrong, which is what an inverted row
 * index looks like. Worth keeping as a note rather than just a corrected line, because
 * the same inversion is what a frame arriving on the panel upside down would also
 * produce, and the next reader deserves to know which of the two was ruled out here.
 */
static void card_report(unsigned int w, unsigned int h) {
    if (!gl1_probe_saw)
        return;
    const uint32_t *f = frame();
    if (!f)
        return;
    static const char *const names[5] = {
        "card/grey", "card/red", "card/green", "card/blue", "card/white",
    };
    for (int i = 0; i < 5; i++) {
        const unsigned int y = (unsigned int)(((float)i + 0.5f) * (float)h / 6.0f);
        gl1_probe_saw(names[i], f[(size_t)y * (size_t)g_fb_w + (size_t)(w / 4u)]);
    }
    const unsigned int ry = (unsigned int)(5.5f * (float)h / 6.0f);
    gl1_probe_saw("card/ramp-25", f[(size_t)ry * (size_t)g_fb_w + (size_t)(w / 4u)]);
    gl1_probe_saw("card/ramp-50", f[(size_t)ry * (size_t)g_fb_w + (size_t)(w / 2u)]);
    gl1_probe_saw("card/ramp-75",
                  f[(size_t)ry * (size_t)g_fb_w + (size_t)(w * 3u / 4u)]);
}

void gl1_probe_test_card(void) {
    if (!g_ctx || !g_disp)
        return;

    /* The whole panel, not the 128x96 corner the checks work in: a defect that repeats
       every tile or every so many pixels needs the full width to be visible as a
       repeat. */
    const unsigned int w = g_fb_w;
    const unsigned int h = g_fb_h;
    glViewport(0, 0, (GLsizei)w, (GLsizei)h);
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

    static const float band[5][3] = {
        {0.502f, 0.502f, 0.502f}, /* 128: a cast shows here and nowhere better */
        {1.0f, 0.0f, 0.0f},       {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},       {1.0f, 1.0f, 1.0f},
    };
    /* **Painted into both scanout buffers**, because a swap presents one and leaves the
       other holding the frame before last. One pass would put the card on screen and
       leave whatever was there behind it, and anything that presents again afterwards
       would show that instead. The readings are taken on the first pass, before its
       swap, so they describe the frame this drew rather than whatever the second pass
       found. */
    for (int pass = 0; pass < 2; pass++) {
        /* **The two passes are deliberately different now.** Painting both buffers with
           the same image was meant to stop a stale one showing through, and it also
           made this card unable to see the one thing a two-buffer display can get
           wrong: presenting both at once. The port's buffers differ on every frame and
           its sky comes back as two complete images interleaved a column at a time,
           which is what that would look like; this card's did not differ at all, so it
           could not have shown it. Pass 0 paints the bands as described, pass 1 paints
           their complement. One of them is on screen afterwards and it must be *one* -
           any mixture of the two, in columns or otherwise, is the display serving both.
         */
        for (int i = 0; i < 5; i++) {
            const float y1 = 1.0f - (float)i * (2.0f / 6.0f);
            const float y0 = 1.0f - (float)(i + 1) * (2.0f / 6.0f);
            const float r = pass ? 1.0f - band[i][0] : band[i][0];
            const float g = pass ? 1.0f - band[i][1] : band[i][1];
            const float b = pass ? 1.0f - band[i][2] : band[i][2];
            draw_rect(-1.0f, y0, 1.0f, y1, r, g, b);
        }
        /* The ramp, as one smooth-shaded quad rather than steps - a band boundary this
           draws itself would be indistinguishable from one the display introduced. */
        glShadeModel(GL_SMOOTH);
        glBegin(GL_QUADS);
        glColor3f(0.0f, 0.0f, 0.0f);
        glVertex3f(-1.0f, -1.0f, 0.0f);
        glColor3f(1.0f, 1.0f, 1.0f);
        glVertex3f(1.0f, -1.0f, 0.0f);
        glColor3f(1.0f, 1.0f, 1.0f);
        glVertex3f(1.0f, -1.0f + (2.0f / 6.0f), 0.0f);
        glColor3f(0.0f, 0.0f, 0.0f);
        glVertex3f(-1.0f, -1.0f + (2.0f / 6.0f), 0.0f);
        glEnd();
        if (pass == 0)
            card_report(w, h);
        /* **And put it on the panel**, which the first version did not. Every check in
           this suite decides by reading the render target back, so nothing here had
           ever needed to present a frame - the probe has run its whole life with the
           display showing nothing, and a black screen was correct behaviour rather than
           a symptom. The card is the one thing here whose purpose is to be looked at,
           so it is the one thing that must swap. */
        glSwapBuffers();
    }
}
