/*
 * mesa-dri-probe - brings GL up through the Gallium DRI frontend and says how far it
 * gets. The frontend creates its own screen, so `mesa-winsys-probe` stays the control.
 *
 * Each step is a log line and the last one is the answer; the shim names any frontend
 * step it stops on. After `oops_gl_create` it reports the GL strings, then renders
 * what it can verify by readback: a fixed-function clear and triangle, a GLSL 330
 * colour-interpolated triangle, and an FNV-1a hash of the whole frame. Then it
 * presents.
 * A frame that does not retire is a failure, never a software fallback.
 */

#include "oops/system.h"

#include "oops_platform.h"

/* GL's own headers, with the conversion warnings upstream does not build under
 * suppressed for the includes only. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#pragma clang diagnostic pop

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TAG "DRI-PROBE"

/* A big-app cannot exit; it logs a last line, then idles until the host closes it. */
_Noreturn static void park(void) {
    oops_log_info(TAG, "idle and finished - close this title from the host");
    oops_system_park_until_closed();
}

/* 1920x1080, the display's scanout size: presentation hands the whole drawable to the
 * display, which wants an image the framebuffer's size. */
#define PROBE_WIDTH 1920u
#define PROBE_HEIGHT 1080u

/*
 * Report one GL string. GL calls reach the driver through libglapi's thread-local
 * dispatch table, so a null means this thread's table has no entry - distinct from a
 * context that never became current, which `oops_gl_create` reports.
 */
static void report_string(const char *label, GLenum name) {
    const GLubyte *s = glGetString(name);

    oops_log_info(TAG, "%s", label);
    if (s == NULL) {
        oops_log_info(
            TAG,
            "  ... came back null: this thread's dispatch table has no entry for it");
        return;
    }
    oops_log_info(TAG, "%s", (const char *)s);
}

void mesa_dri_probe_start(void);

/*
 * Fixed function: clear to a known colour, draw a triangle in normalised device
 * coordinates, and read back the centre (the triangle) and a corner (the clear), so
 * neither a missing draw nor a whole-surface fill passes. glReadPixels reads the
 * drawable's own colour buffer, not the scanout.
 */
static void probe_first_render(struct oops_gl *gl) {
    uint32_t w = 0;
    uint32_t h = 0;
    oops_gl_extent(gl, &w, &h);
    glViewport(0, 0, (GLsizei)w, (GLsizei)h);

    glClearColor(0.25f, 0.50f, 0.75f, 1.0f); /* clear -> about 64,128,191 */
    glClear(GL_COLOR_BUFFER_BIT);

    glColor4f(1.0f, 0.0f, 0.0f, 1.0f); /* triangle -> 255,0,0 */
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.5f, -0.5f);
    glVertex2f(0.5f, -0.5f);
    glVertex2f(0.0f, 0.5f);
    glEnd();
    glFinish();
    GLenum draw_err = glGetError();

    unsigned char centre[4] = {0, 0, 0, 0};
    unsigned char corner[4] = {0, 0, 0, 0};
    glReadPixels((GLint)(w / 2u), (GLint)(h / 2u), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                 centre);
    glReadPixels(2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, corner);
    GLenum read_err = glGetError();

    char buf[220];
    snprintf(buf, sizeof buf,
             "first render: draw err=0x%x read err=0x%x | centre R=%u G=%u B=%u "
             "(expect ~255 0 0) "
             "| corner R=%u G=%u B=%u (expect ~64 128 191)",
             (unsigned)draw_err, (unsigned)read_err, (unsigned)centre[0],
             (unsigned)centre[1], (unsigned)centre[2], (unsigned)corner[0],
             (unsigned)corner[1], (unsigned)corner[2]);
    oops_log_info(TAG, "%s", buf);
}

/*
 * The programmable pipeline: GLSL compile and link, a VBO with two attributes, and
 * per-vertex colour interpolation. The vertices are red, green and blue, so the centre
 * is a blend no clear or flat draw produces. This frame is the one presented.
 */
static void probe_glsl_render(struct oops_gl *gl) {
    static const char *const vs_src =
        "#version 330\n"
        "layout(location=0) in vec2 a_pos;\n"
        "layout(location=1) in vec3 a_col;\n"
        "out vec3 v_col;\n"
        "void main(){ v_col = a_col; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";
    static const char *const fs_src = "#version 330\n"
                                      "in vec3 v_col;\n"
                                      "out vec4 o_col;\n"
                                      "void main(){ o_col = vec4(v_col, 1.0); }\n";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_src, NULL);
    glCompileShader(vs);
    GLint vs_ok = 0;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &vs_ok);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_src, NULL);
    glCompileShader(fs);
    GLint fs_ok = 0;
    glGetShaderiv(fs, GL_COMPILE_STATUS, &fs_ok);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint link_ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &link_ok);
    glUseProgram(prog);

    static const GLfloat verts[] = {
        /* x      y      r     g     b */
        -0.6f, -0.6f, 1.0f, 0.0f, 0.0f, 0.6f, -0.6f, 0.0f,
        1.0f,  0.0f,  0.0f, 0.6f, 0.0f, 0.0f, 1.0f,
    };
    GLuint vao = 0;
    GLuint vbo = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, (GLsizei)(5u * sizeof(GLfloat)),
                          (const void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, (GLsizei)(5u * sizeof(GLfloat)),
                          (const void *)(2u * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    uint32_t w = 0;
    uint32_t h = 0;
    oops_gl_extent(gl, &w, &h);
    glViewport(0, 0, (GLsizei)w, (GLsizei)h);
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glFinish();
    GLenum err = glGetError();

    unsigned char px[4] = {0, 0, 0, 0};
    glReadPixels((GLint)(w / 2u), (GLint)(h / 2u), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);

    char buf[200];
    snprintf(buf, sizeof buf,
             "glsl render: compile vs=%d fs=%d link=%d draw err=0x%x | centre R=%u "
             "G=%u B=%u "
             "(a blend of the three vertex colours)",
             (int)vs_ok, (int)fs_ok, (int)link_ok, (unsigned)err, (unsigned)px[0],
             (unsigned)px[1], (unsigned)px[2]);
    oops_log_info(TAG, "%s", buf);
}

/*
 * The GLSL frame's clear, `glClearColor(0.05, 0.05, 0.08, 1.0)`, as the `0xAARRGGBB`
 * word the readback produces; `mod-pixels` counts against it. The corner pixel is
 * logged beside it as the measured value.
 */
#define PROBE_CLEAR_WORD 0xff0d0d14u

/*
 * A hash of the whole frame: FNV-1a/32 stepped one 32-bit pixel word at a time, as
 * gl1-cube and the oops-sdk oracle record define it (prosperous#D008), logged with
 * `mod-pixels` and the centre pixel. Every input is fixed, so runs of this binary must
 * agree. The words are `GL_BGRA` (`0xAARRGGBB`) in glReadPixels' bottom-up order: the
 * frame radeonsi rendered, before the shim's row swap. The line says full-frame because
 * gl1-cube also logs a sampled hash.
 */
static void probe_frame_hash(struct oops_gl *gl) {
    uint32_t w = 0;
    uint32_t h = 0;
    oops_gl_extent(gl, &w, &h);

    const size_t pixels = (size_t)w * (size_t)h;
    uint32_t *fb = (uint32_t *)malloc(pixels * sizeof *fb);
    if (fb == NULL) {
        /* No hash is a missing measurement, not a passing one. */
        oops_log_info(
            TAG,
            "frame hash: the readback buffer would not allocate; no hash this run");
        return;
    }

    glReadPixels(0, 0, (GLsizei)w, (GLsizei)h, GL_BGRA, GL_UNSIGNED_BYTE, fb);
    GLenum err = glGetError();

    uint32_t frame_hash = 0x811c9dc5u; /* FNV-1a, 32-bit */
    uint32_t mod_pixels = 0;
    for (size_t p = 0; p < pixels; p++) {
        uint32_t v = fb[p];
        if (v != PROBE_CLEAR_WORD)
            mod_pixels++;
        frame_hash ^= v;
        frame_hash *= 0x01000193u;
    }

    /* The centre as a word, to agree with the R/G/B logged above through the other
     * readback format. The corner is the measured clear. */
    const uint32_t centre_pix = fb[(size_t)(h / 2u) * (size_t)w + (size_t)(w / 2u)];
    const uint32_t corner_pix = fb[0];

    free(fb);

    char buf[260];
    snprintf(
        buf, sizeof buf,
        "frame-hash 0x%08x (FNV-1a/32, full-frame, %u x %u words, BGRA bottom-up) | "
        "mod-pixels %u | centre-pixel 0x%08x | corner-pixel 0x%08x (the clear, expect "
        "0x%08x) | read err=0x%x",
        (unsigned)frame_hash, (unsigned)w, (unsigned)h, (unsigned)mod_pixels,
        (unsigned)centre_pix, (unsigned)corner_pix, (unsigned)PROBE_CLEAR_WORD,
        (unsigned)err);
    oops_log_info(TAG, "%s", buf);
}

/*
 * Runs the C++ dynamic initialisers (oops-mesa `abi.c`). No crt start-up object walks
 * `.init_array`, and ACO's opcode table `instr_info` stays zeroed until it runs.
 */
extern void oops_mesa_run_init_array(void);

void mesa_dri_probe_start(void) {
    oops_mesa_run_init_array();

    oops_log_info(TAG,
                  "bringing GL up through the DRI frontend (v" OOPS_APP_VERSION ")");

    struct oops_gl *gl = oops_gl_create(PROBE_WIDTH, PROBE_HEIGHT);

    if (gl == NULL) {
        /* No cause here: `oops_gl_create` logs the step it stopped on. */
        oops_log_info(TAG,
                      "GL did not come up; the shim's last line above names the step");
        oops_log_info(TAG, "done");
        park();
    }

    oops_log_info(
        TAG,
        "oops_gl_create returned a handle: this is the first time that has happened");

    uint32_t w = 0;
    uint32_t h = 0;
    oops_gl_extent(gl, &w, &h);
    if (w == PROBE_WIDTH && h == PROBE_HEIGHT) {
        oops_log_info(TAG, "the drawable reports the extent that was asked for");
    } else {
        oops_log_info(TAG,
                      "the drawable reports a different extent than was asked for");
    }

    /* The first GL calls, whatever they answer. */
    report_string("GL_VERSION:", GL_VERSION);
    report_string("GL_RENDERER:", GL_RENDERER);
    report_string("GL_VENDOR:", GL_VENDOR);

    probe_first_render(gl);

    /* Overwrites the frame above; this is what gets presented. */
    probe_glsl_render(gl);

    /* Hashes the frame about to be presented, after its `glFinish`. */
    probe_frame_hash(gl);

    /*
     * `oops_gl_present` flushes the drawable (`dri_flush_drawable`; `driSwapBuffers` is
     * null for an image loader), reads the frame back, row-swaps it for the top-down
     * scanout and hands it to the display. On failure the shim names the step.
     */
    if (oops_gl_present(gl)) {
        oops_log_info(TAG, "presentation succeeded: the frame is on the display");
        oops_log_info(TAG, "holding it on screen - close this title from the host");
        /*
         * No teardown on success: closing the display releases the scanout buffers and
         * blanks the screen. Parking with it open keeps the last flipped frame visible.
         */
        park();
    }

    oops_log_info(
        TAG, "presentation refused: the flip half did not run - read the shim's lines "
             "above");
    oops_gl_destroy(gl);
    oops_log_info(TAG, "torn down");

    oops_log_info(TAG, "done");
    park();
}
