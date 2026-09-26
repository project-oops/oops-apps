/*
 * mesa-dri-probe - brings GL up through the Gallium DRI frontend and says how far it
 * gets.
 *
 * # What this is for
 *
 * oops-mesa's platform shim has existed since 2026-09-17 and nothing has ever called
 * it. It compiles, it links, and a title packages with the whole frontend in it - but
 * `oops_gl_create` has never run, so every statement about it is a statement about
 * source code (oops-mesa worklog 041 and 042 both say so in as many words). This is the
 * title that changes that.
 *
 * It is the frontend's counterpart to `mesa-winsys-probe`, which walks the winsys path
 * directly. The two are separate titles on purpose: the frontend creates its own
 * screen, so one title doing both would have two screens and no clean attribution for a
 * failure. `mesa-winsys-probe` stays the control.
 *
 * # What it reports, and the order is the point
 *
 * Every step below is a line in the log, and the last line that appears is the answer.
 * The frontend's own failures are reported by the shim, which names the step it stopped
 * on, so this file does not duplicate that - it reports what the shim could not know:
 * whether the handle came back, whether GL then answers, and whether presentation is
 * refused for the reason it is expected to be refused for.
 *
 * It renders only what it can verify, in three steps that each add one thing to the one
 * before:
 *
 * 1. **Fixed function** - a clear to a known colour and a triangle over the centre,
 * read back with glReadPixels (which detiles through Mesa and reads the drawable's own
 * colour buffer, not the display scanout) - one pixel inside the triangle, one in the
 * corner outside it.
 * 2. **The programmable pipeline** - a GLSL 330 vertex and fragment shader drawing a
 *    colour-interpolated triangle from a vertex buffer, which is what fixed function
 * does not reach: the GLSL compiler, program linking, and real vertex attributes.
 * 3. **The frame hash** - FNV-1a over every pixel of the finished frame, which is
 * roadmap unit 6's acceptance gate and the step a single sampled pixel cannot stand in
 * for.
 *
 * The flip half is no longer an open question: presentation works and the frame reaches
 * the panel (oops-mesa worklog 065). A frame that did not retire is a failure and never
 * a fallback to software (CLAUDE.md, principle 4), so nothing here fakes a result - the
 * readback is the check.
 */

#include "oops/system.h"

#include "oops_platform.h"

/*
 * GL's own headers, from the same `mesa/include` a title already compiles against.
 *
 * They are included with the conversion warnings off for the reason mesa-winsys-probe's
 * includes are: this title compiles at `-Wconversion -Wsign-conversion -Werror`, and
 * upstream's headers are not ours to make clean. The suppression covers the includes
 * and nothing after them.
 */
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

static void say(const char *msg) {
    oops_klog("DRI-PROBE", msg);
}

/*
 * How a title finishes here, which is by not finishing.
 *
 * The measurement is obSCEne `REQ-20260917T1450Z-2e71` and it is written up where the
 * shared helper is declared, in `oops-sdk/include/oops/system.h`: no userland call
 * terminates a `big-app` process, `_exit` raises `SIGSYS` for want of permission on
 * syscall 1, and returning faults at `rip: 0x0` because the dynamic linker provides no
 * caller frame. Printing the last line and idling for the host to close the app is the
 * conforming pattern, and it produces no coredump, no crash report and no hung GPU
 * ring.
 *
 * Before this was known, every title here returned and took a crash report at the end
 * of a successful run. It was always *after* the results, so it cost no measurement -
 * but it cost a clean log tail, and it made a good run look like a bad one.
 *
 * The line is said here so it carries this title's tag; the idling is the helper's.
 */
_Noreturn static void park(void) {
    say("idle and finished - close this title from the host");
    oops_system_park_until_closed();
}

/*
 * The extent, and why it is small.
 *
 * 1280x720 is not a display mode here and does not have to be: the extent is the
 * title's to choose and the compositor scales, which is the cheaper path at 4K and is
 * the reason `oops_gl_create` takes it as an argument rather than reading it from the
 * display (oops-mesa D009, and the note on `oops_gl_create`).
 *
 * Small is deliberate for a first run. The drawable's colour buffer is allocated by
 * Mesa through `dri_create_image`, and this is the first time anything will have asked
 * the winsys for GPU memory on this path - worklog 022 established the startup path
 * allocates none. A 3.5 MB buffer failing is easier to read than a 33 MB one, and
 * nothing about the path under test changes with the size.
 */
/* 1920x1080 so the drawable matches the display's own scanout size: the flip half reads
 * the whole drawable back and hands it to the display, which wants an image the
 * framebuffer's size, and the AGC display promotes 1280x720 to 1080p internally - so a
 * 720p drawable would not fill it. */
#define PROBE_WIDTH 1920u
#define PROBE_HEIGHT 1080u

/*
 * Report one GL string.
 *
 * `glGetString` is the cheapest call that proves the whole dispatch chain, and it is
 * worth being precise about what a null answer means. The frontend's GL entry points
 * reach the driver through libglapi's **thread-local** dispatch table, so a null here
 * does not say "GL is broken" - it says the table this thread sees has no function in
 * that slot, which on this platform is the thread local storage question worklog 040
 * settled. That is a different failure from a context that never became current, and
 * `oops_gl_create` has already reported the second kind if it happened.
 *
 * So: the string is the interesting result, and null is reported as null rather than
 * smoothed over.
 */
static void report_string(const char *label, GLenum name) {
    const GLubyte *s = glGetString(name);

    if (s == NULL) {
        oops_klog("DRI-PROBE", label);
        say("  ... came back null: this thread's dispatch table has no entry for it");
        return;
    }
    oops_klog("DRI-PROBE", label);
    oops_klog("DRI-PROBE", (const char *)s);
}

void mesa_dri_probe_start(void);

/*
 * The first render on this platform: clear to a known colour, draw a triangle over the
 * centre, and read two pixels back - one inside the triangle, one in the corner outside
 * it.
 *
 * The clear already reads back pixel-exact (worklog 064). The triangle is the next step
 * up: it drives the fixed-function vertex path (which radeonsi lowers to an
 * ACO-compiled shader), rasterisation and the fragment path - the rest of the 3D
 * pipeline. It is drawn in normalised device coordinates, so the default identity
 * projection maps it to the viewport with no matrix setup. glReadPixels detiles through
 * Mesa and reads the drawable's own colour buffer, not the display scanout, so the
 * check holds without the flip half. Two pixels make it a real test: the centre must be
 * the triangle's colour and the corner must still be the clear colour, so neither a
 * missing draw nor a whole-surface fill can pass. No result is faked - the readback is
 * the check (CLAUDE.md principle 4).
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
    say(buf);
}

/*
 * The programmable pipeline: a GLSL vertex + fragment shader drawing a
 * colour-interpolated triangle from a vertex buffer. This is what fixed-function does
 * not exercise - the GLSL compiler (whose builtin tables the .init_array fix
 * initialises), program linking, a VBO with two vertex attributes, and per-vertex
 * colour interpolation in a real fragment shader. The three vertices are red, green and
 * blue, so the centre pixel is a blend of all three - a value neither a clear nor a
 * flat draw could produce, which is the check that the interpolation actually ran. It
 * overwrites the fixed-function frame, so this is what reaches the screen.
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
    say(buf);
}

/*
 * The word the GLSL frame clears to, in the `0xAARRGGBB` form the readback below
 * produces.
 *
 * `glClearColor(0.05, 0.05, 0.08, 1.0)` at 8 bits per channel is `round(0.05 * 255) =
 * 13` (0x0d) for red and green and `round(0.08 * 255) = 20` (0x14) for blue, alpha
 * 0xff. It is a constant here only so `mod-pixels` has something to count against, the
 * way gl1-cube's does; the corner pixel is reported beside it as the *measured* value,
 * so if this constant is ever wrong the log says so instead of quietly miscounting.
 */
#define PROBE_CLEAR_WORD 0xff0d0d14u

/*
 * Unit 6's acceptance gate: a hash of the whole frame.
 *
 * The centre pixel above proves the interpolation ran. It does not prove the *frame* is
 * right - one pixel is one pixel, and a frame that is correct in the middle and torn
 * everywhere else passes it. The gate the roadmap actually sets for unit 6 is "a title
 * draws and hashes a known frame", and this is that hash.
 *
 * # The algorithm is the collection's, deliberately
 *
 * FNV-1a, 32 bits, `0x811c9dc5` basis and `0x01000193` prime, stepped **one 32-bit
 * pixel word at a time** - not one byte at a time, which is a different number over the
 * same buffer. That is what `oops-apps/src/oops-gl/gl1-cube` does and what
 * `oops-sdk/docs/hardware/`'s oracle record defines, and Prosperous D008 says in as
 * many words that this stays six copied lines rather than becoming a shared API. It
 * travels with the same two siblings that record does - `mod-pixels` and the centre
 * pixel - because a bare hash that has changed tells you nothing about *how*.
 *
 * # What this hash is, and what it is not
 *
 * It is **a new oracle for this title's own frame**, not a comparison against
 * gl1-cube's. Those recorded values (`0x9dbfe189`, `0xc51cec32`) are tied to that
 * title's clear colour and its cube; this frame is a different picture entirely, so a
 * matching number would be a coincidence and a differing one means nothing. What makes
 * it a gate is **reproducibility**: the clear, the three vertex colours and the vertex
 * positions are all fixed, so two runs of this binary must produce the same word. The
 * first run establishes it; every run after checks it.
 *
 * Two things about the buffer, both of which change the number and neither of which is
 * a fault:
 *
 * - It is read `GL_BGRA`, so the words are `0xAARRGGBB` - the same order the oracle
 * record uses and the same call `oops_gl_present` makes, rather than a second format to
 * reason about.
 * - `glReadPixels` reads **bottom-up** while the scanout is top-down. This hashes the
 * readback in its own order, so it is a hash of the frame radeonsi rendered, not of
 * what the panel shows. For a render gate that is the right end of the pipe: the
 * row-swap that orients it for scanout happens in the shim afterwards and is not under
 * test here.
 *
 * It is a full-frame pass, stated in the line because gl1-cube also has a 1-in-64
 * sampled mode and the two numbers are not comparable - a reader who mistook one for
 * the other would conclude the frame had changed when it had not.
 */
static void probe_frame_hash(struct oops_gl *gl) {
    uint32_t w = 0;
    uint32_t h = 0;
    oops_gl_extent(gl, &w, &h);

    const size_t pixels = (size_t)w * (size_t)h;
    uint32_t *fb = (uint32_t *)malloc(pixels * sizeof *fb);
    if (fb == NULL) {
        /* Said, not smoothed over: no hash this run is a missing measurement, not a
         * passing one. */
        say("frame hash: the readback buffer would not allocate; no hash this run");
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

    /* The centre, as a word, so it can be checked against the R/G/B the line above
     * reported - the same pixel read twice through two different formats should agree,
     * and if it does not, one of the two readback paths is wrong. The corner is the
     * measured clear. */
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
    say(buf);
}

/*
 * Run the C++ dynamic initialisers. This module has no crt start-up object to walk
 * `.init_array`, and Mesa has globals that stay zeroed until it does - ACO's opcode
 * table `instr_info` among them, which left every emitted instruction with opcode 0 and
 * faulted the GPU (oops-mesa worklog 062). Defined in oops-mesa's runtime shim
 * (`abi.c`).
 */
extern void oops_mesa_run_init_array(void);

void mesa_dri_probe_start(void) {
    oops_mesa_run_init_array();

    say("bringing GL up through the DRI frontend (v" OOPS_APP_VERSION ")");

    struct oops_gl *gl = oops_gl_create(PROBE_WIDTH, PROBE_HEIGHT);

    if (gl == NULL) {
        /*
         * States no cause, deliberately. `oops_gl_create` logs the step it stopped on -
         * the winsys, the screen, the config, the drawable, the context or make-current
         * - and that line is the result. Adding a guess here would put two accounts of
         * one failure in the log, and the shim's is the one with the information.
         */
        say("GL did not come up; the shim's last line above names the step");
        say("done");
        park();
    }

    say("oops_gl_create returned a handle: this is the first time that has happened");

    uint32_t w = 0;
    uint32_t h = 0;
    oops_gl_extent(gl, &w, &h);
    if (w == PROBE_WIDTH && h == PROBE_HEIGHT) {
        say("the drawable reports the extent that was asked for");
    } else {
        say("the drawable reports a different extent than was asked for");
    }

    /* The first GL call ever made on this platform, whatever it answers. */
    report_string("GL_VERSION:", GL_VERSION);
    report_string("GL_RENDERER:", GL_RENDERER);
    report_string("GL_VENDOR:", GL_VENDOR);

    /* The first render: a clear to a known colour, verified by reading it back. */
    probe_first_render(gl);

    /* The programmable pipeline: a GLSL colour-interpolated triangle, which overwrites
     * the frame above and is what gets presented. */
    probe_glsl_render(gl);

    /* Unit 6's gate, taken of the frame that is about to be presented and after its
     * `glFinish`, so it hashes finished work rather than a frame still in flight. */
    probe_frame_hash(gl);

    /*
     * Presentation, which is expected to succeed.
     *
     * This comment used to say the opposite, and the reversal is worth keeping rather
     * than editing away: the flip was held up by whether `sceVideoOutRegisterBuffers2`
     * constrains a buffer's address, a question that could not be asked until a surface
     * existed. A surface exists now, the question was answered by asking it, and the
     * frame reaches the panel (oops-mesa worklog 065).
     *
     * `oops_gl_present` does both halves: it flushes the drawable (`dri_flush_drawable`
     * - not `driSwapBuffers`, which is swrast/kopper-only and is null for an image
     * loader, worklog 063), reads the finished frame back, row-swaps it (glReadPixels
     * is bottom-up and scanout is top-down) and hands it to the display. `false` is now
     * the surprise, and the shim's own line above names the step it stopped on if it
     * happens.
     */
    if (oops_gl_present(gl)) {
        say("presentation succeeded: the frame is on the display");
        say("holding it on screen - close this title from the host");
        /*
         * Do not tear down on success. Closing the display releases the scanout buffers
         * and blanks the screen, so a single flip followed by teardown shows the frame
         * for one frame and then black - which is what the first run looked like. The
         * video-out holds the last flipped buffer while the title idles, so parking
         * with the display still open keeps the frame visible. The context and display
         * leak, which is fine for a title that idles until the host closes it.
         */
        park();
    }

    say("presentation refused: the flip half did not run - read the shim's lines "
        "above");
    oops_gl_destroy(gl);
    say("torn down");

    say("done");
    park();
}
