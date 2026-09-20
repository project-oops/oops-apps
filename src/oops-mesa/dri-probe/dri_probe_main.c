/*
 * dri-probe - brings GL up through the Gallium DRI frontend and says how far it gets.
 *
 * # What this is for
 *
 * oops-mesa's platform shim has existed since 2026-09-17 and nothing has ever called it. It
 * compiles, it links, and a title packages with the whole frontend in it - but `oops_gl_create`
 * has never run, so every statement about it is a statement about source code (oops-mesa worklog
 * 041 and 042 both say so in as many words). This is the title that changes that.
 *
 * It is the frontend's counterpart to `mesa-probe`, which walks the winsys path directly. The two
 * are separate titles on purpose: the frontend creates its own screen, so one title doing both
 * would have two screens and no clean attribution for a failure. `mesa-probe` stays the control.
 *
 * # What it reports, and the order is the point
 *
 * Every step below is a line in the log, and the last line that appears is the answer. The
 * frontend's own failures are reported by the shim, which names the step it stopped on, so this
 * file does not duplicate that - it reports what the shim could not know: whether the handle came
 * back, whether GL then answers, and whether presentation is refused for the reason it is
 * expected to be refused for.
 *
 * It renders only what it can verify: a clear to a known colour and a triangle over the centre,
 * read back with glReadPixels (which detiles through Mesa and reads the drawable's own colour
 * buffer, not the display scanout) - one pixel inside the triangle, one in the corner outside it.
 * That proves the render path without the flip half, which is still behind an unanswered hardware
 * question. A frame that did not retire is a failure and never a fallback to software (CLAUDE.md,
 * principle 4), so nothing here fakes a result - the readback is the check.
 */

#include "oops/system.h"

#include "oops_platform.h"

/*
 * GL's own headers, from the same `mesa/include` a title already compiles against.
 *
 * They are included with the conversion warnings off for the reason mesa-probe's includes are:
 * this title compiles at `-Wconversion -Wsign-conversion -Werror`, and upstream's headers are
 * not ours to make clean. The suppression covers the includes and nothing after them.
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

static void say(const char *msg)
{
    oops_klog("DRI-PROBE", msg);
}

/*
 * How a title finishes here, which is by not finishing.
 *
 * The measurement is obSCEne `REQ-20260917T1450Z-2e71` and it is written up where the shared
 * helper is declared, in `oops-sdk/include/oops/system.h`: no userland call terminates a
 * `big-app` process, `_exit` raises `SIGSYS` for want of permission on syscall 1, and returning
 * faults at `rip: 0x0` because the dynamic linker provides no caller frame. Printing the last
 * line and idling for the host to close the app is the conforming pattern, and it produces no
 * coredump, no crash report and no hung GPU ring.
 *
 * Before this was known, every title here returned and took a crash report at the end of a
 * successful run. It was always *after* the results, so it cost no measurement - but it cost a
 * clean log tail, and it made a good run look like a bad one.
 *
 * The line is said here so it carries this title's tag; the idling is the helper's.
 */
_Noreturn static void park(void)
{
    say("idle and finished - close this title from the host");
    oops_system_park_until_closed();
}

/*
 * The extent, and why it is small.
 *
 * 1280x720 is not a display mode here and does not have to be: the extent is the title's to
 * choose and the compositor scales, which is the cheaper path at 4K and is the reason
 * `oops_gl_create` takes it as an argument rather than reading it from the display
 * (oops-mesa D009, and the note on `oops_gl_create`).
 *
 * Small is deliberate for a first run. The drawable's colour buffer is allocated by Mesa through
 * `dri_create_image`, and this is the first time anything will have asked the winsys for GPU
 * memory on this path - worklog 022 established the startup path allocates none. A 3.5 MB buffer
 * failing is easier to read than a 33 MB one, and nothing about the path under test changes with
 * the size.
 */
/* 1920x1080 so the drawable matches the display's own scanout size: the flip half reads the whole
 * drawable back and hands it to the display, which wants an image the framebuffer's size, and the
 * AGC display promotes 1280x720 to 1080p internally - so a 720p drawable would not fill it. */
#define PROBE_WIDTH  1920u
#define PROBE_HEIGHT 1080u

/*
 * Report one GL string.
 *
 * `glGetString` is the cheapest call that proves the whole dispatch chain, and it is worth being
 * precise about what a null answer means. The frontend's GL entry points reach the driver through
 * libglapi's **thread-local** dispatch table, so a null here does not say "GL is broken" - it says
 * the table this thread sees has no function in that slot, which on this platform is the thread
 * local storage question worklog 040 settled. That is a different failure from a context that
 * never became current, and `oops_gl_create` has already reported the second kind if it happened.
 *
 * So: the string is the interesting result, and null is reported as null rather than smoothed
 * over.
 */
static void report_string(const char *label, GLenum name)
{
    const GLubyte *s = glGetString(name);

    if (s == NULL) {
        oops_klog("DRI-PROBE", label);
        say("  ... came back null: this thread's dispatch table has no entry for it");
        return;
    }
    oops_klog("DRI-PROBE", label);
    oops_klog("DRI-PROBE", (const char *)s);
}

void dri_probe_start(void);

/*
 * The first render on this platform: clear to a known colour, draw a triangle over the centre, and
 * read two pixels back - one inside the triangle, one in the corner outside it.
 *
 * The clear already reads back pixel-exact (worklog 064). The triangle is the next step up: it
 * drives the fixed-function vertex path (which radeonsi lowers to an ACO-compiled shader),
 * rasterisation and the fragment path - the rest of the 3D pipeline. It is drawn in normalised
 * device coordinates, so the default identity projection maps it to the viewport with no matrix
 * setup. glReadPixels detiles through Mesa and reads the drawable's own colour buffer, not the
 * display scanout, so the check holds without the flip half. Two pixels make it a real test: the
 * centre must be the triangle's colour and the corner must still be the clear colour, so neither a
 * missing draw nor a whole-surface fill can pass. No result is faked - the readback is the check
 * (CLAUDE.md principle 4).
 */
static void probe_first_render(struct oops_gl *gl)
{
    uint32_t w = 0;
    uint32_t h = 0;
    oops_gl_extent(gl, &w, &h);
    glViewport(0, 0, (GLsizei)w, (GLsizei)h);

    glClearColor(0.25f, 0.50f, 0.75f, 1.0f); /* clear -> about 64,128,191 */
    glClear(GL_COLOR_BUFFER_BIT);

    glColor4f(1.0f, 0.0f, 0.0f, 1.0f);       /* triangle -> 255,0,0 */
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.5f, -0.5f);
    glVertex2f(0.5f, -0.5f);
    glVertex2f(0.0f, 0.5f);
    glEnd();
    glFinish();
    GLenum draw_err = glGetError();

    unsigned char centre[4] = { 0, 0, 0, 0 };
    unsigned char corner[4] = { 0, 0, 0, 0 };
    glReadPixels((GLint)(w / 2u), (GLint)(h / 2u), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, centre);
    glReadPixels(2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, corner);
    GLenum read_err = glGetError();

    char buf[220];
    snprintf(buf, sizeof buf,
             "first render: draw err=0x%x read err=0x%x | centre R=%u G=%u B=%u (expect ~255 0 0) "
             "| corner R=%u G=%u B=%u (expect ~64 128 191)",
             (unsigned)draw_err, (unsigned)read_err,
             (unsigned)centre[0], (unsigned)centre[1], (unsigned)centre[2],
             (unsigned)corner[0], (unsigned)corner[1], (unsigned)corner[2]);
    say(buf);
}

/*
 * The programmable pipeline: a GLSL vertex + fragment shader drawing a colour-interpolated triangle
 * from a vertex buffer. This is what fixed-function does not exercise - the GLSL compiler (whose
 * builtin tables the .init_array fix initialises), program linking, a VBO with two vertex
 * attributes, and per-vertex colour interpolation in a real fragment shader. The three vertices are
 * red, green and blue, so the centre pixel is a blend of all three - a value neither a clear nor a
 * flat draw could produce, which is the check that the interpolation actually ran. It overwrites
 * the fixed-function frame, so this is what reaches the screen.
 */
static void probe_glsl_render(struct oops_gl *gl)
{
    static const char *const vs_src =
        "#version 330\n"
        "layout(location=0) in vec2 a_pos;\n"
        "layout(location=1) in vec3 a_col;\n"
        "out vec3 v_col;\n"
        "void main(){ v_col = a_col; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";
    static const char *const fs_src =
        "#version 330\n"
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
        -0.6f, -0.6f, 1.0f, 0.0f, 0.0f,
        0.6f, -0.6f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.6f, 0.0f, 0.0f, 1.0f,
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

    unsigned char px[4] = { 0, 0, 0, 0 };
    glReadPixels((GLint)(w / 2u), (GLint)(h / 2u), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);

    char buf[200];
    snprintf(buf, sizeof buf,
             "glsl render: compile vs=%d fs=%d link=%d draw err=0x%x | centre R=%u G=%u B=%u "
             "(a blend of the three vertex colours)",
             (int)vs_ok, (int)fs_ok, (int)link_ok, (unsigned)err,
             (unsigned)px[0], (unsigned)px[1], (unsigned)px[2]);
    say(buf);
}

/*
 * Run the C++ dynamic initialisers. This module has no crt start-up object to walk `.init_array`,
 * and Mesa has globals that stay zeroed until it does - ACO's opcode table `instr_info` among
 * them, which left every emitted instruction with opcode 0 and faulted the GPU (oops-mesa
 * worklog 062). Defined in oops-mesa's runtime shim (`abi.c`).
 */
extern void oops_mesa_run_init_array(void);

void dri_probe_start(void)
{
    oops_mesa_run_init_array();

    say("bringing GL up through the DRI frontend (v" OOPS_APP_VERSION ")");

    struct oops_gl *gl = oops_gl_create(PROBE_WIDTH, PROBE_HEIGHT);

    if (gl == NULL) {
        /*
         * States no cause, deliberately. `oops_gl_create` logs the step it stopped on - the
         * winsys, the screen, the config, the drawable, the context or make-current - and that
         * line is the result. Adding a guess here would put two accounts of one failure in the
         * log, and the shim's is the one with the information.
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

    /* The programmable pipeline: a GLSL colour-interpolated triangle, which overwrites the frame
     * above and is what gets presented. */
    probe_glsl_render(gl);

    /*
     * Presentation, which is expected to refuse.
     *
     * `oops_gl_present` does the flush half and not the flip half: the flip needs the buffer
     * registered with the display controller, and whether `sceVideoOutRegisterBuffers2`
     * constrains a buffer's address is the open unknown on roadmap unit 6 - a question that cannot
     * be asked until a surface exists, which is what this title is creating.
     *
     * It is called anyway. The flush half runs, which means the frontend finishes the frame and
     * calls back into the shim's `getBuffers`, and that callback is where Mesa allocates the
     * colour buffer. So a refusal here is a *successful* exercise of the callback path, and the
     * shim's own line distinguishes the two. `true` would be the surprise.
     */
    if (oops_gl_present(gl)) {
        say("presentation succeeded: the frame is on the display");
        say("holding it on screen - close this title from the host");
        /*
         * Do not tear down on success. Closing the display releases the scanout buffers and blanks
         * the screen, so a single flip followed by teardown shows the frame for one frame and then
         * black - which is what the first run looked like. The video-out holds the last flipped
         * buffer while the title idles, so parking with the display still open keeps the frame
         * visible. The context and display leak, which is fine for a title that idles until the
         * host closes it.
         */
        park();
    }

    say("presentation refused: the flip half did not run - read the shim's lines above");
    oops_gl_destroy(gl);
    say("torn down");

    say("done");
    park();
}
