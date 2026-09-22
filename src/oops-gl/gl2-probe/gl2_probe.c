/*
 * gl2-probe: OpenGL 2.0 measured against the specification, check by check.
 *
 * gl1-probe does this for the fixed-function pipeline. This is the programmable one: the shader
 * and program objects, the generic vertex attributes, and - the part that matters - what a
 * shader actually computes when it runs.
 *
 * # What each check is allowed to assume
 *
 * Nothing from the check before it. Every check begins with `reset_view`, which puts the state
 * back, drops whatever program the last one left current and clears the frame to a colour no
 * check draws - so "unchanged" is distinguishable from "drawn black" and from "drawn white".
 *
 * # Why so many of them end in a pixel
 *
 * Because that is the half that can differ between the reference and the console. A check that
 * asks `glGetProgramiv` for GL_LINK_STATUS is measuring a variable in a struct and will answer
 * the same on both; a check that asks what colour a fragment shader produced is measuring an
 * implementation. The object-model checks here are the minimum that proves the API is wired at
 * all, and everything after them draws.
 *
 * # The two places these run
 *
 * The host software reference and a console, from one table of checks. That is why nothing here
 * is host-specific and why every sample goes through `frame()`, exactly as gl1-probe does: a
 * check that reached the framebuffer its own way would measure the two paths differently and
 * the comparison - which is the whole value of running twice - would mean nothing.
 */

#include "gl2_probe.h"

#include <GL/gl.h>
#include <oops/display.h>

#ifdef OOPS_HOST_BUILD
#include <string.h>
#else
#include <oops/freestd.h>
#endif

/*
 * The area a check works in - a corner of a full-size display, not a display of its own.
 * `libSceVideoOut` will not register a buffer of 128x96, and gl1-probe's first hardware run
 * died on the NULL framebuffer that produced. The viewport sits at GL's origin, the
 * bottom-left, so only `scan_frame()` has to know where the region starts.
 */
#define PROBE_W 128
#define PROBE_H 96
#define PROBE_DISPLAY_W 1920
#define PROBE_DISPLAY_H 1080

#define PROBE_BG 0xff202020u

static uint32_t *g_fb;
static oops_display_t *g_disp;
static unsigned int g_fb_w;
static unsigned int g_fb_h;
static unsigned int g_row0;

/* The program the current check built. Deleted by the next `reset_view`, so forty checks do not
 * need forty program slots - and so a check cannot accidentally draw with the one before it. */
static GLuint g_prog;

void (*gl2_probe_trace)(const char *name, int verdict);
void (*gl2_probe_saw)(const char *name, uint32_t centre, unsigned int err);

/*
 * Where a check's pixels come from. On the host the software rasteriser writes the target
 * directly; on a console nothing has touched it until the stream is submitted and its
 * end-of-pipe fence comes back, and the copy worth reading is the one the command processor
 * makes into cached memory. `glFinish()` with nothing pending is a no-op.
 */
static const uint32_t *frame(void) {
    glFinish();
#ifndef OOPS_HOST_BUILD
    const GLuint *rb = glGetFrameReadback();
    if (rb) return (const uint32_t *)rb;
#endif
    return (const uint32_t *)g_fb;
}

/* **A scan takes one snapshot and reads that**, and it is the only way this file reads a pixel.
 *
 * There was a single-pixel `px()` beside it until 2026-09-22. Two things were wrong with it and
 * the second is why it is gone rather than merely unused. A pixel at a time would ask for 12,288
 * synchronisations, which on console hardware is about two hours - gl1-probe measured that, and
 * from the outside it was indistinguishable from a hang. And each one calls `glFinish`: on a
 * console oops-gl draws straight into the rotating scanout buffers, so a read taken *after* a
 * check had already scanned landed on the next buffer round - cleared, never drawn into - and
 * reported the reset colour whatever the check had produced. A helper that is wrong only on the
 * target, only when called a second time, is worse than no helper. */
static uint32_t g_scan[PROBE_W * PROBE_H];

static const uint32_t *scan_frame(void) {
    const uint32_t *f = frame();
    for (int i = 0; i < PROBE_W * PROBE_H; i++) g_scan[i] = 0u;
    if (!f) return g_scan;
    for (int y = 0; y < PROBE_H; y++) {
        const uint32_t *row = f + (size_t)(g_row0 + (unsigned int)y) * (size_t)g_fb_w;
        for (int x = 0; x < PROBE_W; x++) g_scan[y * PROBE_W + x] = row[x];
    }
    return g_scan;
}

#define SCAN_PX(s, x, y) ((s)[(y) * PROBE_W + (x)])

static int chan_r(uint32_t c) { return (int)((c >> 16) & 0xffu); }
static int chan_g(uint32_t c) { return (int)((c >> 8) & 0xffu); }
static int chan_b(uint32_t c) { return (int)(c & 0xffu); }

static int near_rgb(uint32_t c, int r, int g, int b, int tol) {
    int dr = chan_r(c) - r, dg = chan_g(c) - g, db = chan_b(c) - b;
    if (dr < 0) dr = -dr;
    if (dg < 0) dg = -dg;
    if (db < 0) db = -db;
    return dr <= tol && dg <= tol && db <= tol;
}

static int near_chan(int got, int want, int tol) {
    const int d = got - want;
    return (d < 0 ? -d : d) <= tol;
}

/* -------------------------------------------------------------------------
 * The state every check starts from
 * ------------------------------------------------------------------------- */

static void reset_view(void) {
    /* **The program goes first.** Dropping it after clearing would clear through whatever
     * fragment shader the last check left bound, which is a picture nothing asked for. */
    glUseProgram(0);
    if (g_prog) {
        glDeleteProgram(g_prog);
        g_prog = 0u;
    }
    for (GLuint i = 0; i < 16u; i++) glDisableVertexAttribArray(i);

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
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glShadeModel(GL_SMOOTH);
    glViewport(0, 0, PROBE_W, PROBE_H);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glClearColor(0.125f, 0.125f, 0.125f, 1.0f); /* 0x20 per channel */
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    (void)glGetError();
}

/* -------------------------------------------------------------------------
 * Building a program
 *
 * The shaders are deleted the moment the program has linked, which is what every GL 2.0 program
 * does and what the object model has to survive - so the whole suite leans on deferred deletion
 * rather than one check testing it in isolation.
 * ------------------------------------------------------------------------- */

static GLuint make_shader(GLenum type, const char *src) {
    const GLchar *strings[1];
    strings[0] = src;
    GLuint sh = glCreateShader(type);
    if (!sh) return 0u;
    glShaderSource(sh, 1, strings, (const GLint *)0);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glDeleteShader(sh);
        return 0u;
    }
    return sh;
}

/* Links a program and makes it current, returning 0 on any failure. `fs` may be NULL, which is
 * a program with only a vertex stage - legal, and the fixed-function fragment stage runs. */
static GLuint use_program(const char *vs_src, const char *fs_src) {
    GLuint vs = make_shader(GL_VERTEX_SHADER, vs_src);
    if (!vs) return 0u;
    GLuint fs = 0u;
    if (fs_src) {
        fs = make_shader(GL_FRAGMENT_SHADER, fs_src);
        if (!fs) {
            glDeleteShader(vs);
            return 0u;
        }
    }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    if (fs) glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        glDeleteProgram(prog);
        return 0u;
    }
    g_prog = prog;
    glUseProgram(prog);
    return prog;
}

/* The identity, column-major. */
static void mat_identity(float m[16]) {
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

/* -------------------------------------------------------------------------
 * Geometry the checks draw
 *
 * Immediate mode with `glVertexAttrib` for the current value, and arrays where the check is
 * about arrays. `glVertex` is what pushes the vertex; the shaders take their position from a
 * generic attribute, so the two are independent and a check that measures one does not depend
 * on the other.
 * ------------------------------------------------------------------------- */

/* A rectangle in NDC through generic slot `loc`, as two triangles. */
static void attrib_rect(GLint loc, float x0, float y0, float x1, float y1, float z) {
    if (loc < 0) return;
    const float xs[6] = {x0, x1, x1, x0, x1, x0};
    const float ys[6] = {y0, y0, y1, y0, y1, y1};
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 6; i++) {
        glVertexAttrib3f((GLuint)loc, xs[i], ys[i], z);
        glVertex3f(xs[i], ys[i], z);
    }
    glEnd();
}

/* The same, but with a second attribute varying per corner - for the checks that need a value
 * that differs across the primitive. */
static void attrib_rect2(GLint pos, GLint extra, float x0, float y0, float x1, float y1,
                         const float corner[4][4]) {
    if (pos < 0) return;
    const int idx[6] = {0, 1, 2, 0, 2, 3};
    const float xs[4] = {x0, x1, x1, x0};
    const float ys[4] = {y0, y0, y1, y1};
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 6; i++) {
        const int c = idx[i];
        if (extra >= 0) {
            glVertexAttrib4f((GLuint)extra, corner[c][0], corner[c][1], corner[c][2],
                             corner[c][3]);
        }
        glVertexAttrib3f((GLuint)pos, xs[c], ys[c], 0.0f);
        glVertex3f(xs[c], ys[c], 0.0f);
    }
    glEnd();
}

/* The shader every check that is not about the vertex stage uses: position straight through. */
static const char *const VS_PASSTHROUGH =
    "attribute vec3 pos;\n"
    "void main() { gl_Position = vec4(pos, 1.0); }\n";

/* Where a full-region rectangle's middle is, and a pixel outside one. */
#define MID_X (PROBE_W / 2)
#define MID_Y (PROBE_H / 2)

/* -------------------------------------------------------------------------
 * The object model
 * ------------------------------------------------------------------------- */

static int check_version_gating(void) {
    reset_view();
    /* **A context has the entry points its version defines and no others.** This suite claimed
     * 2.0 at start-up; narrowing to 1.5 here has to take the whole programmable surface away,
     * and widening again has to bring it back - which is what makes the claim a property of the
     * context rather than a one-way switch.
     *
     * On a desktop driver the same discipline comes from the linker, and a port that develops
     * against a context which gives it everything finds out later, on hardware that does not. */
    if (!glContextSetVersion(1, 5)) return 0;
    (void)glGetError();

    /* Each answers the value it returns on failure, and each records GL_INVALID_OPERATION. */
    int ok = (glCreateShader(GL_VERTEX_SHADER) == 0u);
    ok = ok && glGetError() == GL_INVALID_OPERATION;
    ok = ok && glCreateProgram() == 0u;
    ok = ok && glGetError() == GL_INVALID_OPERATION;
    ok = ok && glIsProgram(1u) == GL_FALSE;
    ok = ok && glGetError() == GL_INVALID_OPERATION;
    ok = ok && glGetUniformLocation(1u, "x") == -1;
    ok = ok && glGetError() == GL_INVALID_OPERATION;
    glUseProgram(0);
    ok = ok && glGetError() == GL_INVALID_OPERATION;
    glEnableVertexAttribArray(0u);
    ok = ok && glGetError() == GL_INVALID_OPERATION;

    /* **An enumerant a later version added is GL_INVALID_ENUM**, which is the different thing
     * it is - and the query leaves its destination alone. */
    GLint v = 1234;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &v);
    ok = ok && glGetError() == GL_INVALID_ENUM && v == 1234;

    /* **GL 1.x is untouched.** Every 1.2-1.5 feature here is advertised as an ARB or EXT
     * extension, and an extension is available whatever the core version - so a narrowed
     * context still has buffer objects and multitexture, and refusing them would be a rule
     * about spelling rather than about capability. */
    GLuint buf = 0u;
    glGenBuffers(1, &buf);
    ok = ok && buf != 0u;
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glActiveTexture(GL_TEXTURE0);
    glSecondaryColor3f(1.0f, 0.0f, 0.0f);
    ok = ok && glGetError() == GL_NO_ERROR;
    glDeleteBuffers(1, &buf);

    /* And back, because everything after this check needs it. */
    ok = ok && glContextSetVersion(2, 0) == GL_TRUE;
    const GLuint sh = glCreateShader(GL_FRAGMENT_SHADER);
    ok = ok && sh != 0u;
    if (sh) glDeleteShader(sh);
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_name_space(void) {
    reset_view();
    const GLuint a = glCreateShader(GL_VERTEX_SHADER);
    const GLuint p = glCreateProgram();
    const GLuint b = glCreateShader(GL_FRAGMENT_SHADER);
    int ok = a && p && b && a != p && p != b && a != b;
    /* A shader call on a program name is GL_INVALID_OPERATION - the object exists and is the
     * wrong kind - where a name nothing owns is GL_INVALID_VALUE. */
    (void)glGetError();
    glCompileShader(p);
    ok = ok && glGetError() == GL_INVALID_OPERATION;
    glCompileShader(0x7fffffffu);
    ok = ok && glGetError() == GL_INVALID_VALUE;
    ok = ok && glIsShader(a) && !glIsProgram(a) && glIsProgram(p) && !glIsShader(p);
    glDeleteShader(a);
    glDeleteShader(b);
    glDeleteProgram(p);
    return ok;
}

static int check_compile_status(void) {
    reset_view();
    GLuint good = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar *ok_src[1] = {"void main() { gl_FragColor = vec4(1.0); }\n"};
    glShaderSource(good, 1, ok_src, (const GLint *)0);
    glCompileShader(good);
    GLint st = 0;
    glGetShaderiv(good, GL_COMPILE_STATUS, &st);
    int ok = (st == GL_TRUE);
    GLint log_len = -1;
    glGetShaderiv(good, GL_INFO_LOG_LENGTH, &log_len);
    ok = ok && log_len == 0;

    /* A type error the grammar accepts happily: `vec3 * mat4` has dimensions that do not meet.
     * **A front end that accepted it would pass every drawing check below** and be wrong about
     * the language. */
    GLuint bad = glCreateShader(GL_VERTEX_SHADER);
    const GLchar *bad_src[1] = {
        "attribute vec3 pos;\n"
        "void main() { gl_Position = vec4(pos * mat4(1.0), 1.0); }\n"};
    glShaderSource(bad, 1, bad_src, (const GLint *)0);
    glCompileShader(bad);
    glGetShaderiv(bad, GL_COMPILE_STATUS, &st);
    ok = ok && st == GL_FALSE;
    glGetShaderiv(bad, GL_INFO_LOG_LENGTH, &log_len);
    ok = ok && log_len > 1;

    glDeleteShader(good);
    glDeleteShader(bad);
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_link_interface(void) {
    reset_view();
    if (!use_program("uniform mat4 mvp;\n"
                     "uniform float scale;\n"
                     "attribute vec3 pos;\n"
                     "attribute vec3 tint;\n"
                     "varying vec3 vtint;\n"
                     "void main() { vtint = tint * scale; gl_Position = mvp * vec4(pos, 1.0); }\n",
                     "varying vec3 vtint;\n"
                     "void main() { gl_FragColor = vec4(vtint, 1.0); }\n")) {
        return 0;
    }
    GLint n = 0;
    glGetProgramiv(g_prog, GL_ACTIVE_UNIFORMS, &n);
    int ok = (n == 2);
    glGetProgramiv(g_prog, GL_ACTIVE_ATTRIBUTES, &n);
    ok = ok && n == 2;

    GLint size = 0;
    GLenum type = 0;
    char name[32];
    glGetActiveUniform(g_prog, 0, (GLsizei)sizeof(name), (GLsizei *)0, &size, &type, name);
    ok = ok && size == 1 && (type == GL_FLOAT_MAT4 || type == GL_FLOAT);

    const GLint mvp = glGetUniformLocation(g_prog, "mvp");
    const GLint scale = glGetUniformLocation(g_prog, "scale");
    ok = ok && mvp >= 0 && scale >= 0 && mvp != scale;
    /* A name nothing declares is -1 and **not an error**, which is what lets a program stop
     * branching on a uniform the linker removed. */
    ok = ok && glGetUniformLocation(g_prog, "absent") == -1 && glGetError() == GL_NO_ERROR;
    /* And the language's own names are not the program's attributes. */
    ok = ok && glGetAttribLocation(g_prog, "gl_Vertex") == -1;
    return ok;
}

static int check_link_refuses(void) {
    reset_view();
    /* A varying the fragment shader reads and the vertex shader never writes. A link that let
     * this through would interpolate zeros and draw a black picture with no diagnostic. */
    GLuint vs = make_shader(GL_VERTEX_SHADER, VS_PASSTHROUGH);
    GLuint fs = make_shader(GL_FRAGMENT_SHADER,
                            "varying vec3 missing;\n"
                            "void main() { gl_FragColor = vec4(missing, 1.0); }\n");
    if (!vs || !fs) return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint linked = -1;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    int ok = (linked == GL_FALSE);
    GLint log_len = 0;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &log_len);
    ok = ok && log_len > 1;
    /* A program that did not link cannot be made current, and the failure does not drop the
     * caller back to fixed function behind its back. */
    (void)glGetError();
    glUseProgram(prog);
    ok = ok && glGetError() == GL_INVALID_OPERATION;

    /* A vertex shader that never writes gl_Position is undefined by the specification. A blank
     * screen is the worst diagnostic there is, so it is a link error here. */
    GLuint vs2 = make_shader(GL_VERTEX_SHADER,
                             "attribute vec3 pos;\n"
                             "varying vec3 v;\n"
                             "void main() { v = pos; }\n");
    GLuint prog2 = glCreateProgram();
    glAttachShader(prog2, vs2);
    glLinkProgram(prog2);
    glGetProgramiv(prog2, GL_LINK_STATUS, &linked);
    ok = ok && linked == GL_FALSE;

    glDeleteShader(vs);
    glDeleteShader(fs);
    glDeleteShader(vs2);
    glDeleteProgram(prog);
    glDeleteProgram(prog2);
    return ok;
}

static int check_deferred_delete(void) {
    reset_view();
    GLuint vs = make_shader(GL_VERTEX_SHADER, VS_PASSTHROUGH);
    GLuint fs = make_shader(GL_FRAGMENT_SHADER, "void main() { gl_FragColor = vec4(1.0); }\n");
    if (!vs || !fs) return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        glDeleteProgram(prog);
        return 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    /* **`glIsShader` says no and `glGetShaderiv` still answers.** Both are required and an
     * implementation with only one of them passes half the tests that exist for this. */
    int ok = (glIsShader(vs) == GL_FALSE);
    GLint flagged = -1;
    (void)glGetError();
    glGetShaderiv(vs, GL_DELETE_STATUS, &flagged);
    ok = ok && flagged == GL_TRUE && glGetError() == GL_NO_ERROR;
    /* And the program still works, which is the point of the deferral. */
    glUseProgram(prog);
    ok = ok && glGetError() == GL_NO_ERROR;
    g_prog = prog;
    return ok;
}

static int check_uniform_type_match(void) {
    reset_view();
    if (!use_program("uniform float scale;\n"
                     "uniform int count;\n"
                     "attribute vec3 pos;\n"
                     "void main() { gl_Position = vec4(pos, 1.0) * scale * float(count); }\n",
                     "void main() { gl_FragColor = vec4(1.0); }\n")) {
        return 0;
    }
    const GLint scale = glGetUniformLocation(g_prog, "scale");
    const GLint count = glGetUniformLocation(g_prog, "count");
    if (scale < 0 || count < 0) return 0;
    (void)glGetError();

    glUniform1f(scale, 2.0f);
    int ok = (glGetError() == GL_NO_ERROR);
    GLfloat got = 0.0f;
    glGetUniformfv(g_prog, scale, &got);
    ok = ok && got > 1.99f && got < 2.01f;

    /* **The command has to match the declared type.** `glUniform1i` on a float is an error, not
     * a conversion - which is what stops an `i` form meant for a sampler quietly setting a float
     * somewhere else. */
    glUniform1i(scale, 3);
    ok = ok && glGetError() == GL_INVALID_OPERATION;
    glUniform1f(count, 3.0f);
    ok = ok && glGetError() == GL_INVALID_OPERATION;
    glUniform1i(count, 3);
    ok = ok && glGetError() == GL_NO_ERROR;
    /* A width that does not match is the same kind of error. */
    glUniform2f(scale, 1.0f, 2.0f);
    ok = ok && glGetError() == GL_INVALID_OPERATION;
    /* **A location of -1 is silently ignored**, by definition. */
    glUniform1f(-1, 5.0f);
    ok = ok && glGetError() == GL_NO_ERROR;
    return ok;
}

static int check_uniform_arrays(void) {
    reset_view();
    if (!use_program("uniform vec4 palette[3];\n"
                     "uniform float after;\n"
                     "attribute vec3 pos;\n"
                     "void main() { gl_Position = vec4(pos, 1.0) * palette[2] * after; }\n",
                     "void main() { gl_FragColor = vec4(1.0); }\n")) {
        return 0;
    }
    const GLint base = glGetUniformLocation(g_prog, "palette");
    if (base < 0) return 0;
    /* The unbracketed name is element zero, the bracketed forms are consecutive, and past the
     * end is -1 rather than a location into whatever comes next. */
    int ok = glGetUniformLocation(g_prog, "palette[0]") == base;
    ok = ok && glGetUniformLocation(g_prog, "palette[2]") == base + 2;
    ok = ok && glGetUniformLocation(g_prog, "palette[3]") == -1;
    /* **The uniform after the array must not collide with its elements**, which is exactly what
     * numbering locations per uniform rather than per element would do. */
    const GLint after = glGetUniformLocation(g_prog, "after");
    ok = ok && after >= base + 3;

    const GLfloat three[12] = {1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1};
    (void)glGetError();
    glUniform4fv(base, 3, three);
    ok = ok && glGetError() == GL_NO_ERROR;
    GLfloat got[4] = {0, 0, 0, 0};
    glGetUniformfv(g_prog, base + 1, got);
    ok = ok && got[1] > 0.99f;
    /* A count above one on something that is not an array is an error. */
    glUniform1fv(after, 2, three);
    ok = ok && glGetError() == GL_INVALID_OPERATION;
    return ok;
}

static int check_bind_attrib_location(void) {
    reset_view();
    GLuint vs = make_shader(GL_VERTEX_SHADER,
                            "attribute vec3 pos;\n"
                            "attribute vec3 tint;\n"
                            "varying vec3 v;\n"
                            "void main() { v = tint; gl_Position = vec4(pos, 1.0); }\n");
    GLuint fs = make_shader(GL_FRAGMENT_SHADER,
                            "varying vec3 v;\n"
                            "void main() { gl_FragColor = vec4(v, 1.0); }\n");
    if (!vs || !fs) return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glBindAttribLocation(prog, 5, "pos");
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        glDeleteProgram(prog);
        return 0;
    }
    g_prog = prog;
    int ok = glGetAttribLocation(prog, "pos") == 5;
    const GLint other = glGetAttribLocation(prog, "tint");
    ok = ok && other >= 0 && other != 5;

    /* **A binding after the link does not take effect until the next one**, which is the
     * mistake this API most invites: a program that binds, draws, and wonders why. */
    glBindAttribLocation(prog, 7, "pos");
    ok = ok && glGetAttribLocation(prog, "pos") == 5;
    glLinkProgram(prog);
    ok = ok && glGetAttribLocation(prog, "pos") == 7;

    /* The language's own prefix cannot be bound. */
    (void)glGetError();
    glBindAttribLocation(prog, 3, "gl_Vertex");
    ok = ok && glGetError() == GL_INVALID_OPERATION;
    return ok;
}

static int check_limits(void) {
    reset_view();
    GLint v = 0;
    (void)glGetError();
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &v);
    int ok = (v >= 16);                       /* GL 2.0's own minimum */
    glGetIntegerv(GL_MAX_VARYING_FLOATS, &v);
    ok = ok && v >= 32;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &v);
    ok = ok && v >= 2;
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &v);
    ok = ok && v >= 128;
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &v);
    ok = ok && v >= 64;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &v);
    ok = ok && v >= 1;
    ok = ok && glGetError() == GL_NO_ERROR;

    /* The shading language's own version string, separate from GL_VERSION. */
    /* **The highest dialect the front end takes**, which is what this is asked to report - not
     * the one the GL badge pairs with. A shader may still declare `#version 110` and be held to
     * 1.10's rules; the number here is a ceiling, not a mode. */
    const GLubyte *sl = glGetString(GL_SHADING_LANGUAGE_VERSION);
    ok = ok && sl != (const GLubyte *)0;
    if (sl) {
        ok = ok && sl[0] == '1' && sl[1] == '.' && sl[2] == '2' && sl[3] == '0';
    }
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_current_program_query(void) {
    reset_view();
    GLint cur = -1;
    glGetIntegerv(GL_CURRENT_PROGRAM, &cur);
    int ok = (cur == 0);
    if (!use_program(VS_PASSTHROUGH, "void main() { gl_FragColor = vec4(1.0); }\n")) return 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &cur);
    ok = ok && cur == (GLint)g_prog;
    /* **glUseProgram(0) goes back to the fixed-function pipeline**, which a program that draws
     * its HUD with glBegin after its scene depends on. */
    glUseProgram(0);
    glGetIntegerv(GL_CURRENT_PROGRAM, &cur);
    return ok && cur == 0;
}

/* -------------------------------------------------------------------------
 * The vertex stage
 * ------------------------------------------------------------------------- */

static int check_program_draws(void) {
    reset_view();
    const GLuint p = use_program(VS_PASSTHROUGH,
                                 "void main() { gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0); }\n");
    if (!p) return 0;
    const GLint loc = glGetAttribLocation(p, "pos");
    /* Not square and not centred, so a transposed or mirrored rectangle fails rather than
     * passing by symmetry. */
    attrib_rect(loc, -0.6f, -0.3f, 0.2f, 0.7f, 0.0f);

    const uint32_t *s = scan_frame();
    /* Inside is the shader's green; outside is still the clear. */
    int ok = near_rgb(SCAN_PX(s, 40, 30), 0, 255, 0, 2);
    ok = ok && SCAN_PX(s, 4, 4) == PROBE_BG;
    ok = ok && SCAN_PX(s, PROBE_W - 5, PROBE_H - 5) == PROBE_BG;
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_mvp_uniform(void) {
    reset_view();
    const GLuint p = use_program("uniform mat4 mvp;\n"
                                 "attribute vec3 pos;\n"
                                 "void main() { gl_Position = mvp * vec4(pos, 1.0); }\n",
                                 "void main() { gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n");
    if (!p) return 0;
    const GLint loc = glGetAttribLocation(p, "pos");
    const GLint mvp = glGetUniformLocation(p, "mvp");

    /* **Column-major: element 12 is the x translation.** A transposed read moves the rectangle
     * in y instead, which a centred square would hide - so the rectangle is off-centre and the
     * check names a side. */
    float m[16];
    mat_identity(m);
    m[0] = 0.25f;
    m[5] = 0.25f;
    m[12] = 0.5f;
    glUniformMatrix4fv(mvp, 1, GL_FALSE, m);
    attrib_rect(loc, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);

    const uint32_t *s = scan_frame();
    /* The rectangle is a quarter size and shifted half a clip box to the right, so it covers
     * NDC x from 0.25 to 0.75 - which is pixels 80 to 112 of the region. Lit well right of
     * centre, clear well left of it. */
    int ok = near_rgb(SCAN_PX(s, MID_X + 24, MID_Y), 255, 0, 0, 2);
    ok = ok && SCAN_PX(s, MID_X - 24, MID_Y) == PROBE_BG;

    /* **The same numbers with `transpose` are a different matrix**, and the check is that they
     * produce a different picture rather than the same one. Transposing moves the 0.5 out of
     * the translation and into the row that computes w, so the rectangle is projected instead
     * of shifted and no longer reaches where it was - which is what an ignored flag would not
     * change. */
    glClear(GL_COLOR_BUFFER_BIT);
    glUniformMatrix4fv(mvp, 1, GL_TRUE, m);
    attrib_rect(loc, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
    s = scan_frame();
    ok = ok && SCAN_PX(s, MID_X + 24, MID_Y) == PROBE_BG;
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_attribute_arrays(void) {
    reset_view();
    const GLuint p = use_program("attribute vec3 pos;\n"
                                 "attribute vec4 tint;\n"
                                 "varying vec4 v;\n"
                                 "void main() { v = tint; gl_Position = vec4(pos, 1.0); }\n",
                                 "varying vec4 v;\n"
                                 "void main() { gl_FragColor = v; }\n");
    if (!p) return 0;
    const GLint pos = glGetAttribLocation(p, "pos");
    const GLint tint = glGetAttribLocation(p, "tint");
    if (pos < 0 || tint < 0) return 0;

    /* **Every vertex is fetched before any is shaded**, so an implementation that kept one
     * current value per slot would give every vertex the last one's - a triangle collapsed to
     * one colour rather than the three corners below. */
    static const GLfloat verts[9] = {
        -0.9f, -0.9f, 0.0f,
         0.9f, -0.9f, 0.0f,
         0.0f,  0.9f, 0.0f};
    static const GLubyte cols[12] = {
        255, 0, 0, 255,
        0, 255, 0, 255,
        0, 0, 255, 255};
    glVertexAttribPointer((GLuint)pos, 3, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray((GLuint)pos);
    /* **Normalised**, so 255 is 1.0 and not 255.0 - the difference this flag exists for. */
    glVertexAttribPointer((GLuint)tint, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, cols);
    glEnableVertexAttribArray((GLuint)tint);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    const uint32_t *s = scan_frame();
    const uint32_t bl = SCAN_PX(s, 10, PROBE_H - 8);
    const uint32_t br = SCAN_PX(s, PROBE_W - 11, PROBE_H - 8);
    const uint32_t top = SCAN_PX(s, MID_X, 8);
    int ok = chan_r(bl) > chan_g(bl) && chan_r(bl) > chan_b(bl);
    ok = ok && chan_g(br) > chan_r(br) && chan_g(br) > chan_b(br);
    ok = ok && chan_b(top) > chan_r(top) && chan_b(top) > chan_g(top);
    /* The middle mixes all three, which an unnormalised read would have saturated to white. */
    const uint32_t mid = SCAN_PX(s, MID_X, MID_Y + 6);
    ok = ok && chan_r(mid) > 16 && chan_r(mid) < 220;
    ok = ok && chan_g(mid) > 16 && chan_g(mid) < 220;
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_attribute_current_value(void) {
    reset_view();
    const GLuint p = use_program("attribute vec3 pos;\n"
                                 "attribute vec4 tint;\n"
                                 "varying vec4 v;\n"
                                 "void main() { v = tint; gl_Position = vec4(pos, 1.0); }\n",
                                 "varying vec4 v;\n"
                                 "void main() { gl_FragColor = v; }\n");
    if (!p) return 0;
    const GLint pos = glGetAttribLocation(p, "pos");
    const GLint tint = glGetAttribLocation(p, "tint");
    if (pos < 0 || tint < 0) return 0;

    /* The array is disabled, so every vertex reads `glVertexAttrib`'s current value - the
     * generic pipeline's glColor. **A 2f fills z with 0 and w with 1** rather than leaving
     * whatever a previous 4f wrote, which is the rule this half measures. */
    glVertexAttrib4f((GLuint)tint, 0.25f, 0.5f, 0.75f, 1.0f);
    attrib_rect(pos, -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    int ok = near_rgb(SCAN_PX(s, MID_X, MID_Y), 64, 128, 191, 3);

    glClear(GL_COLOR_BUFFER_BIT);
    glVertexAttrib2f((GLuint)tint, 1.0f, 0.0f);
    attrib_rect(pos, -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    s = scan_frame();
    ok = ok && near_rgb(SCAN_PX(s, MID_X, MID_Y), 255, 0, 0, 2);
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_builtin_vertex_attributes(void) {
    reset_view();
    /* A 1.10 shader may read the fixed-function attributes and the matrix stack, which is how a
     * port replaces its transform without rewriting the rest of its drawing. */
    const GLuint p = use_program(
        "void main() {\n"
        "  gl_FrontColor = gl_Color;\n"
        "  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
        "}\n",
        "void main() { gl_FragColor = gl_Color; }\n");
    if (!p) return 0;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glScalef(0.5f, 0.5f, 1.0f);
    glColor3f(0.0f, 0.0f, 1.0f);
    glBegin(GL_TRIANGLES);
    glVertex3f(-1.0f, -1.0f, 0.0f);
    glVertex3f(1.0f, -1.0f, 0.0f);
    glVertex3f(1.0f, 1.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f, 0.0f);
    glVertex3f(1.0f, 1.0f, 0.0f);
    glVertex3f(-1.0f, 1.0f, 0.0f);
    glEnd();

    const uint32_t *s = scan_frame();
    /* Half-size because of the scale, so the middle is blue and the far corner is not. */
    int ok = near_rgb(SCAN_PX(s, MID_X, MID_Y), 0, 0, 255, 2);
    ok = ok && SCAN_PX(s, 4, 4) == PROBE_BG;
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_vertex_only_program(void) {
    reset_view();
    /* **A program may have one stage.** With only a vertex shader the fixed-function fragment
     * stage runs, reading `gl_FrontColor` - which is what a port that replaces its transform
     * and keeps its texture combiner does. */
    const GLuint p = use_program("attribute vec3 pos;\n"
                                 "void main() {\n"
                                 "  gl_FrontColor = vec4(1.0, 0.0, 1.0, 1.0);\n"
                                 "  gl_Position = vec4(pos, 1.0);\n"
                                 "}\n",
                                 (const char *)0);
    if (!p) return 0;
    const GLint loc = glGetAttribLocation(p, "pos");
    attrib_rect(loc, -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    return near_rgb(SCAN_PX(s, MID_X, MID_Y), 255, 0, 255, 2) && glGetError() == GL_NO_ERROR;
}

/* -------------------------------------------------------------------------
 * Varyings
 * ------------------------------------------------------------------------- */

static int check_varying_interpolates(void) {
    reset_view();
    const GLuint p = use_program("attribute vec3 pos;\n"
                                 "attribute vec4 tint;\n"
                                 "varying vec4 v;\n"
                                 "void main() { v = tint; gl_Position = vec4(pos, 1.0); }\n",
                                 "varying vec4 v;\n"
                                 "void main() { gl_FragColor = v; }\n");
    if (!p) return 0;
    const GLint pos = glGetAttribLocation(p, "pos");
    const GLint tint = glGetAttribLocation(p, "tint");
    /* Black on the left edge, red on the right. */
    static const float corners[4][4] = {
        {0.0f, 0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}};
    attrib_rect2(pos, tint, -1.0f, -0.8f, 1.0f, 0.8f, corners);

    const uint32_t *s = scan_frame();
    const int left = chan_r(SCAN_PX(s, 16, MID_Y));
    const int mid = chan_r(SCAN_PX(s, MID_X, MID_Y));
    const int right = chan_r(SCAN_PX(s, PROBE_W - 17, MID_Y));
    int ok = left < mid && mid < right;
    /* Under an orthographic projection every w is 1, so the middle is the arithmetic middle. */
    ok = ok && near_chan(mid, 128, 12);
    /* And the other channels stayed where the corners put them. */
    ok = ok && chan_g(SCAN_PX(s, MID_X, MID_Y)) < 4;
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_varying_perspective_correct(void) {
    reset_view();
    /* **A varying is linear in clip space, not in screen space.** The quad below has w = 1 on
     * its left edge and w = 3 on its right, so the value at the screen midpoint is 0.25 and not
     * 0.5 - which is the whole difference between a perspective-correct interpolator and an
     * affine one, and is invisible on any quad drawn flat to the viewer.
     *
     * The position is built from the attribute so the shader, and not the matrix stack, decides
     * w: `pos.z` carries it. */
    const GLuint p = use_program(
        "attribute vec3 pos;\n"
        "attribute vec4 tint;\n"
        "varying vec4 v;\n"
        "void main() {\n"
        "  v = tint;\n"
        "  gl_Position = vec4(pos.x * pos.z, pos.y * pos.z, 0.0, pos.z);\n"
        "}\n",
        "varying vec4 v;\n"
        "void main() { gl_FragColor = v; }\n");
    if (!p) return 0;
    const GLint pos = glGetAttribLocation(p, "pos");
    const GLint tint = glGetAttribLocation(p, "tint");
    if (pos < 0 || tint < 0) return 0;

    const int idx[6] = {0, 1, 2, 0, 2, 3};
    const float xs[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
    const float ys[4] = {-0.8f, -0.8f, 0.8f, 0.8f};
    const float ws[4] = {1.0f, 3.0f, 3.0f, 1.0f};
    const float vs[4] = {0.0f, 1.0f, 1.0f, 0.0f};
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 6; i++) {
        const int c = idx[i];
        glVertexAttrib4f((GLuint)tint, vs[c], 0.0f, 0.0f, 1.0f);
        glVertexAttrib3f((GLuint)pos, xs[c], ys[c], ws[c]);
        glVertex3f(xs[c], ys[c], 0.0f);
    }
    glEnd();

    const uint32_t *s = scan_frame();
    const int mid = chan_r(SCAN_PX(s, MID_X, MID_Y));
    /* 0.25 is 64 of 255. An affine interpolator answers 128 here, which is far outside this. */
    return near_chan(mid, 64, 14) && glGetError() == GL_NO_ERROR;
}

static int check_several_varyings(void) {
    reset_view();
    /* Four varyings of different widths at once, which is what exercises the linker's packing:
     * an implementation that gave two of them the same slot would interpolate one into the
     * other and the colours below would be wrong rather than missing. */
    const GLuint p = use_program(
        "attribute vec3 pos;\n"
        "varying float a;\n"
        "varying vec2 b;\n"
        "varying vec3 c;\n"
        "varying vec4 d;\n"
        "void main() {\n"
        "  a = 0.25;\n"
        "  b = vec2(0.5, 0.75);\n"
        "  c = vec3(1.0, 0.0, 0.0);\n"
        "  d = vec4(0.0, 1.0, 0.0, 1.0);\n"
        "  gl_Position = vec4(pos, 1.0);\n"
        "}\n",
        "varying float a;\n"
        "varying vec2 b;\n"
        "varying vec3 c;\n"
        "varying vec4 d;\n"
        "void main() {\n"
        "  gl_FragColor = vec4(a + c.r * 0.0, b.x, b.y * d.g, 1.0);\n"
        "}\n");
    if (!p) return 0;
    const GLint loc = glGetAttribLocation(p, "pos");
    attrib_rect(loc, -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    /* (0.25, 0.5, 0.75) is (64, 128, 191). */
    return near_rgb(SCAN_PX(s, MID_X, MID_Y), 64, 128, 191, 3) && glGetError() == GL_NO_ERROR;
}

/* -------------------------------------------------------------------------
 * The fragment stage
 * ------------------------------------------------------------------------- */

static int check_frag_coord(void) {
    reset_view();
    /* **`gl_FragCoord.y` counts up from the bottom**, which is the opposite of the
     * rasteriser's rows - a shader that gets the row index instead draws this gradient upside
     * down, and that is exactly what this measures. Its x and y are pixel centres, so the
     * value at the region's middle is a known fraction of its height. */
    const GLuint p = use_program(VS_PASSTHROUGH,
                                 "void main() {\n"
                                 "  gl_FragColor = vec4(gl_FragCoord.y / 96.0, 0.0, 0.0, 1.0);\n"
                                 "}\n");
    if (!p) return 0;
    const GLint loc = glGetAttribLocation(p, "pos");
    attrib_rect(loc, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);

    const uint32_t *s = scan_frame();
    const int top_row = chan_r(SCAN_PX(s, MID_X, 6));
    const int bottom_row = chan_r(SCAN_PX(s, MID_X, PROBE_H - 7));
    /* Row 6 of the image is near the top, which is a high window y. */
    int ok = top_row > bottom_row;
    ok = ok && near_chan(top_row, (int)(255.0f * (96.0f - 6.5f) / 96.0f), 8);
    ok = ok && near_chan(bottom_row, (int)(255.0f * 6.5f / 96.0f), 8);
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_discard(void) {
    reset_view();
    glEnable(GL_DEPTH_TEST);
    /* Discards the left half. **A discarded fragment writes no depth either**, which the second
     * draw measures: something further away must still appear there. */
    const GLuint p = use_program("attribute vec3 pos;\n"
                                 "varying float side;\n"
                                 "void main() { side = pos.x; gl_Position = vec4(pos, 1.0); }\n",
                                 "varying float side;\n"
                                 "void main() {\n"
                                 "  if (side < 0.0) discard;\n"
                                 "  gl_FragColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
                                 "}\n");
    if (!p) return 0;
    attrib_rect(glGetAttribLocation(p, "pos"), -0.9f, -0.9f, 0.9f, 0.9f, -0.5f);

    const uint32_t *s = scan_frame();
    int ok = near_rgb(SCAN_PX(s, MID_X + 24, MID_Y), 0, 0, 255, 2);
    ok = ok && SCAN_PX(s, MID_X - 24, MID_Y) == PROBE_BG;

    const GLuint p2 = use_program(VS_PASSTHROUGH,
                                  "void main() { gl_FragColor = vec4(1.0, 1.0, 0.0, 1.0); }\n");
    if (!p2) return 0;
    attrib_rect(glGetAttribLocation(p2, "pos"), -0.9f, -0.9f, 0.9f, 0.9f, 0.5f);

    s = scan_frame();
    ok = ok && near_rgb(SCAN_PX(s, MID_X + 24, MID_Y), 0, 0, 255, 2);   /* still blue */
    ok = ok && near_rgb(SCAN_PX(s, MID_X - 24, MID_Y), 255, 255, 0, 2); /* the depth was free */
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_frag_depth(void) {
    reset_view();
    glEnable(GL_DEPTH_TEST);
    /* A shader that writes `gl_FragDepth` decides its own depth, and the test that follows uses
     * what the shader wrote - not the interpolated value. Here the near quad pushes itself to
     * the far plane, so the quad drawn behind it afterwards wins. */
    const GLuint p = use_program(VS_PASSTHROUGH,
                                 "void main() {\n"
                                 "  gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
                                 "  gl_FragDepth = 0.99;\n"
                                 "}\n");
    if (!p) return 0;
    attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, -0.9f);

    const uint32_t *s = scan_frame();
    int ok = near_rgb(SCAN_PX(s, MID_X, MID_Y), 255, 0, 0, 2);

    const GLuint p2 = use_program(VS_PASSTHROUGH,
                                  "void main() { gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0); }\n");
    if (!p2) return 0;
    attrib_rect(glGetAttribLocation(p2, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    s = scan_frame();
    /* z = 0 maps to depth 0.5, which is in front of the 0.99 the first shader wrote. */
    ok = ok && near_rgb(SCAN_PX(s, MID_X, MID_Y), 0, 255, 0, 2);
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_front_facing(void) {
    reset_view();
    /* `gl_FrontFacing` is false for a polygon wound away from the viewer. Culling is off, so
     * both are drawn and the two halves differ. */
    const GLuint p = use_program(VS_PASSTHROUGH,
                                 "void main() {\n"
                                 "  gl_FragColor = gl_FrontFacing ? vec4(0.0, 1.0, 0.0, 1.0)\n"
                                 "                                : vec4(1.0, 0.0, 0.0, 1.0);\n"
                                 "}\n");
    if (!p) return 0;
    const GLint loc = glGetAttribLocation(p, "pos");
    if (loc < 0) return 0;

    /* Counter-clockwise on the left, clockwise on the right. */
    const float lx[3] = {-0.9f, -0.1f, -0.5f};
    const float ly[3] = {-0.7f, -0.7f, 0.7f};
    const float rx[3] = {0.1f, 0.5f, 0.9f};
    const float ry[3] = {-0.7f, 0.7f, -0.7f};
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 3; i++) {
        glVertexAttrib3f((GLuint)loc, lx[i], ly[i], 0.0f);
        glVertex3f(lx[i], ly[i], 0.0f);
    }
    for (int i = 0; i < 3; i++) {
        glVertexAttrib3f((GLuint)loc, rx[i], ry[i], 0.0f);
        glVertex3f(rx[i], ry[i], 0.0f);
    }
    glEnd();

    const uint32_t *s = scan_frame();
    const uint32_t left = SCAN_PX(s, 30, PROBE_H - 20);
    const uint32_t right = SCAN_PX(s, PROBE_W - 31, PROBE_H - 20);
    /* One green and one red, whichever way round the winding convention lands. */
    const int lg = near_rgb(left, 0, 255, 0, 2), lr = near_rgb(left, 255, 0, 0, 2);
    const int rg = near_rgb(right, 0, 255, 0, 2), rr = near_rgb(right, 255, 0, 0, 2);
    return ((lg && rr) || (lr && rg)) && glGetError() == GL_NO_ERROR;
}

static int check_fragment_only_program(void) {
    reset_view();
    /* The other half of a half-programmable pipeline: no vertex shader, so the fixed-function
     * transform runs and the fragment shader reads what it produced through `gl_Color`. */
    GLuint fs = make_shader(GL_FRAGMENT_SHADER,
                            "void main() { gl_FragColor = vec4(gl_Color.rgb * 0.5, 1.0); }\n");
    if (!fs) return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(fs);
    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        glDeleteProgram(prog);
        return 0;
    }
    g_prog = prog;
    glUseProgram(prog);

    glColor3f(1.0f, 0.5f, 0.0f);
    glRectf(-0.8f, -0.8f, 0.8f, 0.8f);
    const uint32_t *s = scan_frame();
    return near_rgb(SCAN_PX(s, MID_X, MID_Y), 128, 64, 0, 4) && glGetError() == GL_NO_ERROR;
}

/* -------------------------------------------------------------------------
 * Texturing
 * ------------------------------------------------------------------------- */

/* A 2x2 texture of one colour on the given unit, with no filtering, and **no `glEnable`** - a
 * sampler's declared type names its target and GL 2.0 removed the enable's part in this. */
static GLuint make_flat_texture(GLenum unit, uint32_t abgr) {
    GLuint id = 0u;
    glGenTextures(1, &id);
    glActiveTexture(unit);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    const uint32_t texels[4] = {abgr, abgr, abgr, abgr};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    return id;
}

static int check_texture_sampler(void) {
    reset_view();
    GLuint tex = make_flat_texture(GL_TEXTURE0, 0xff00ff00u); /* R=0 G=255 B=0 A=255 */
    glActiveTexture(GL_TEXTURE0);

    const GLuint p = use_program("attribute vec3 pos;\n"
                                 "varying vec2 uv;\n"
                                 "void main() {\n"
                                 "  uv = pos.xy * 0.5 + vec2(0.5);\n"
                                 "  gl_Position = vec4(pos, 1.0);\n"
                                 "}\n",
                                 "uniform sampler2D tex;\n"
                                 "varying vec2 uv;\n"
                                 "void main() { gl_FragColor = texture2D(tex, uv); }\n");
    if (!p) {
        glDeleteTextures(1, &tex);
        return 0;
    }
    /* A sampler that was never set reads unit 0, which is what a freshly linked program's
     * uniforms hold - so this draw measures the default as well as the lookup. */
    attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    const int ok = near_rgb(SCAN_PX(s, MID_X, MID_Y), 0, 255, 0, 2);
    glDeleteTextures(1, &tex);
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_sampler_unit_selection(void) {
    reset_view();
    GLuint t0 = make_flat_texture(GL_TEXTURE0, 0xff0000ffu); /* red */
    GLuint t1 = make_flat_texture(GL_TEXTURE1, 0xffff0000u); /* blue */
    glActiveTexture(GL_TEXTURE0);

    const GLuint p = use_program("attribute vec3 pos;\n"
                                 "varying vec2 uv;\n"
                                 "void main() {\n"
                                 "  uv = pos.xy * 0.5 + vec2(0.5);\n"
                                 "  gl_Position = vec4(pos, 1.0);\n"
                                 "}\n",
                                 "uniform sampler2D first;\n"
                                 "uniform sampler2D second;\n"
                                 "varying vec2 uv;\n"
                                 "void main() {\n"
                                 "  gl_FragColor = mix(texture2D(first, uv),\n"
                                 "                     texture2D(second, uv), 0.5);\n"
                                 "}\n");
    int ok = (p != 0u);
    if (ok) {
        const GLint a = glGetUniformLocation(p, "first");
        const GLint b = glGetUniformLocation(p, "second");
        /* **A sampler takes the integer form only**: setting one with a float would be a unit
         * number that is nearly an integer. */
        (void)glGetError();
        glUniform1f(a, 1.0f);
        ok = ok && glGetError() == GL_INVALID_OPERATION;
        glUniform1i(a, 0);
        glUniform1i(b, 1);
        ok = ok && glGetError() == GL_NO_ERROR;
        attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
        const uint32_t *s = scan_frame();
        ok = ok && near_rgb(SCAN_PX(s, MID_X, MID_Y), 128, 0, 128, 4);

        /* Both on unit 1 is all blue, which only reads if `glUniform1i` really chose the unit
         * rather than the declaration order deciding it. */
        glClear(GL_COLOR_BUFFER_BIT);
        glUniform1i(a, 1);
        attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
        s = scan_frame();
        ok = ok && near_rgb(SCAN_PX(s, MID_X, MID_Y), 0, 0, 255, 2);
    }
    glDeleteTextures(1, &t0);
    glDeleteTextures(1, &t1);
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_derivatives(void) {
    reset_view();
    /* `dFdx` of a varying that runs 0..1 across a known width is its slope per pixel. A
     * derivative taken as zero - which is what an implementation without neighbouring values
     * answers - gives black here rather than a constant grey. */
    const GLuint p = use_program(
        "attribute vec3 pos;\n"
        "varying float v;\n"
        "void main() { v = pos.x * 0.5 + 0.5; gl_Position = vec4(pos, 1.0); }\n",
        "varying float v;\n"
        "void main() { gl_FragColor = vec4(dFdx(v) * 128.0, 0.0, 0.0, 1.0); }\n");
    if (!p) return 0;
    attrib_rect(glGetAttribLocation(p, "pos"), -1.0f, -0.8f, 1.0f, 0.8f, 0.0f);

    const uint32_t *s = scan_frame();
    const int a = chan_r(SCAN_PX(s, MID_X - 20, MID_Y));
    const int b = chan_r(SCAN_PX(s, MID_X + 20, MID_Y));
    /* v runs 0..1 over 128 pixels, so dFdx(v) is 1/128 and the product is 1.0 - saturated to
     * 255. Constant across the quad, which is what a linear varying's derivative is. */
    int ok = a > 200 && b > 200 && a == b;
    return ok && glGetError() == GL_NO_ERROR;
}

/* -------------------------------------------------------------------------
 * The language
 * ------------------------------------------------------------------------- */

/* A whole fragment shader, drawn over the region and compared against what the specification
 * says its one colour is. */
static int language_check_src(const char *fs_src, int r, int g, int b, int tol) {
    reset_view();
    const GLuint p = use_program(VS_PASSTHROUGH, fs_src);
    if (!p) return 0;
    attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    return near_rgb(SCAN_PX(s, MID_X, MID_Y), r, g, b, tol) && glGetError() == GL_NO_ERROR;
}

/* The same, for a check whose whole shader is one `main` - which is most of them. **A body that
 * declares a function cannot use this**: GLSL has no nested function definitions, so those
 * checks write their own source. */
static int language_check(const char *body, int r, int g, int b, int tol) {
    char src[1024];
    int at = 0;
    const char *head = "void main() {\n";
    for (int i = 0; head[i]; i++) src[at++] = head[i];
    for (int i = 0; body[i] && at < (int)sizeof(src) - 8; i++) src[at++] = body[i];
    const char *tail = "\n}\n";
    for (int i = 0; tail[i]; i++) src[at++] = tail[i];
    src[at] = '\0';
    return language_check_src(src, r, g, b, tol);
}

static int check_matrix_arithmetic(void) {
    /* **`mat * vec` and `vec * mat` are different answers**, not a convenience: the second is
     * the transpose's product. A matrix with one off-diagonal term separates them, and an
     * implementation that treated both as the same would give the same colour twice. */
    return language_check(
        "  mat2 m = mat2(1.0, 2.0, 0.0, 1.0);\n"   /* column-major: col0 = (1,2), col1 = (0,1) */
        "  vec2 a = m * vec2(1.0, 0.0);\n"         /* = col0 = (1, 2) */
        "  vec2 b = vec2(1.0, 0.0) * m;\n"         /* = (dot(v,col0), dot(v,col1)) = (1, 0) */
        "  gl_FragColor = vec4(a.y * 0.25, b.y, 0.0, 1.0);",
        128, 0, 0, 3);
}

static int check_swizzles(void) {
    /* Reading a swizzle, writing one, and writing one out of order - `v.zx = ...` writes z then
     * x, and an implementation that wrote them in the order the components are numbered would
     * swap them. */
    return language_check(
        "  vec4 v = vec4(0.0, 0.0, 0.0, 1.0);\n"
        "  v.zx = vec2(1.0, 0.25);\n"
        "  vec3 c = v.xzy;\n"
        "  gl_FragColor = vec4(c, 1.0);",
        64, 255, 0, 3);
}

static int check_control_flow(void) {
    /* A loop with a `continue` - which still runs the increment, and is what makes a `for` loop
     * with one terminate - a `break`, and an `if`. 1+2+3+4+5 is 15, and the break stops it at 5.
     * 15 * 0.05 is 0.75, which is 191. */
    return language_check(
        "  float total = 0.0;\n"
        "  for (int i = 1; i < 100; i++) {\n"
        "    if (i > 5) break;\n"
        "    if (i == 3) { total += float(i); continue; }\n"
        "    total += float(i);\n"
        "  }\n"
        "  gl_FragColor = vec4(total * 0.05, 0.0, 0.0, 1.0);",
        191, 0, 0, 3);
}

static int check_user_functions(void) {
    /* A function that returns a value and one with `inout` parameters. **Arguments are passed by
     * value and copied back**, never by reference - which is what GLSL says and what makes the
     * swap below mean anything. The functions are at the top level because GLSL has no nested
     * function definitions. */
    return language_check_src(
        "float half_of(float x) { return x * 0.5; }\n"
        "void swap(inout float a, inout float b) { float t = a; a = b; b = t; }\n"
        "void main() {\n"
        "  float p = 1.0;\n"
        "  float q = 0.0;\n"
        "  swap(p, q);\n"
        "  gl_FragColor = vec4(half_of(q), p, 0.0, 1.0);\n"
        "}\n",
        128, 0, 0, 3);
}

static int check_builtin_math(void) {
    /* The genType overloads, the scalar-second forms, and the three-argument ones - each with a
     * value chosen so a wrong answer is a different colour rather than a near one. */
    return language_check(
        "  float a = clamp(2.0, 0.0, 0.5);\n"          /* 0.5 */
        "  float b = mix(0.0, 1.0, 0.25);\n"           /* 0.25 */
        "  float c = smoothstep(0.0, 1.0, 0.5);\n"     /* 0.5 */
        "  float d = length(vec2(3.0, 4.0)) * 0.2;\n"  /* 1.0 */
        "  gl_FragColor = vec4(a * d, b, c * d, 1.0);",
        128, 64, 128, 3);
}

static int check_mod_is_floored(void) {
    /* **GLSL's `mod` is a floored modulus, not C's truncated `fmod`.** `mod(-1.0, 4.0)` is 3 in
     * a shader and -1 in C, and a shader tiling a texture by `mod(uv, 1.0)` wraps correctly with
     * one and mirrors at the origin with the other. 3/4 is 191.
     *
     * Integer division truncates, as it does in C: 7 / 2 is 3, and 3 * 0.25 is 0.75. */
    return language_check(
        "  float m = mod(-1.0, 4.0) * 0.25;\n"
        "  int q = 7 / 2;\n"
        "  gl_FragColor = vec4(m, float(q) * 0.25, 0.0, 1.0);",
        191, 191, 0, 3);
}

static int check_relational_builtins(void) {
    /* `lessThan` gives a bvec, `any` and `all` reduce one, and `not` inverts it. Each of the
     * three channels below is a different reduction of the same comparison. */
    return language_check(
        "  bvec3 c = lessThan(vec3(0.0, 1.0, 2.0), vec3(1.0, 1.0, 1.0));\n"
        "  float a = any(c) ? 1.0 : 0.0;\n"
        "  float b = all(c) ? 1.0 : 0.0;\n"
        "  float d = all(not(c)) ? 1.0 : 0.0;\n"
        "  gl_FragColor = vec4(a, b, d, 1.0);",
        255, 0, 0, 3);
}

static int check_short_circuit(void) {
    /* **`&&` and `||` do not evaluate their right operand when the left decides it**, which
     * GLSL requires (1.10, 5.9) and which a shader relies on to guard a divide.
     *
     * The right operand here writes through an `out` parameter, so an implementation that
     * evaluated it anyway leaves a mark and the colour differs. A pure function would have been
     * no test at all: there would be nothing to see either way. */
    return language_check_src(
        "bool mark(out float touched) { touched = 1.0; return true; }\n"
        "void main() {\n"
        "  float a = 0.0;\n"
        "  float b = 0.0;\n"
        "  bool never = false;\n"
        "  bool always = true;\n"
        "  if (never && mark(a)) { a = 1.0; }\n"
        "  if (always || mark(b)) { b = b; }\n"
        "  float k = (a == 0.0 && b == 0.0) ? 0.5 : 1.0;\n"
        "  gl_FragColor = vec4(k, 0.0, 0.0, 1.0);\n"
        "}\n",
        128, 0, 0, 3);
}

static int check_constructors(void) {
    /* **`mat4(1.0)` is the identity and not a matrix of ones** - the constructor people get
     * wrong - and `vec4(v.xy, 1.0, 0.0)` gathers four values from three arguments. */
    return language_check(
        "  mat2 m = mat2(0.5);\n"
        "  vec2 v = m * vec2(1.0, 1.0);\n"           /* (0.5, 0.5), not (1.0, 1.0) */
        "  vec4 c = vec4(v, 0.0, 1.0);\n"
        "  gl_FragColor = vec4(c.x, c.y, float(int(1.75)) * 0.0, 1.0);",
        128, 128, 0, 3);
}

/* -------------------------------------------------------------------------
 * The fixed-function stages that still apply
 * ------------------------------------------------------------------------- */

static int check_depth_test_applies(void) {
    reset_view();
    glEnable(GL_DEPTH_TEST);
    const GLuint near_p = use_program(VS_PASSTHROUGH,
                                      "void main() { gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0); }\n");
    if (!near_p) return 0;
    attrib_rect(glGetAttribLocation(near_p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, -0.5f);
    const GLuint far_p = use_program(VS_PASSTHROUGH,
                                     "void main() { gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n");
    if (!far_p) return 0;
    attrib_rect(glGetAttribLocation(far_p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.5f);
    const uint32_t *s = scan_frame();
    /* The near one wins, which says the shader's output went through the depth test rather than
     * round it. */
    return near_rgb(SCAN_PX(s, MID_X, MID_Y), 0, 255, 0, 2) && glGetError() == GL_NO_ERROR;
}

static int check_blend_applies(void) {
    reset_view();
    const GLuint p = use_program(VS_PASSTHROUGH,
                                 "void main() { gl_FragColor = vec4(1.0, 1.0, 1.0, 0.5); }\n");
    if (!p) return 0;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    /* **The alpha the shader wrote is the blend's source alpha.** White at 0.5 over the 0x20
     * background is 0x90 per channel; a driver that took the fixed-function current alpha
     * instead would give white. */
    return near_rgb(SCAN_PX(s, MID_X, MID_Y), 0x8f, 0x8f, 0x8f, 3) && glGetError() == GL_NO_ERROR;
}

static int check_cull_face_applies(void) {
    reset_view();
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    const GLuint p = use_program(VS_PASSTHROUGH,
                                 "void main() { gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n");
    if (!p) return 0;
    const GLint loc = glGetAttribLocation(p, "pos");
    if (loc < 0) return 0;
    /* A clockwise triangle, which is the back face by default and must not appear. */
    const float xs[3] = {-0.8f, -0.8f, 0.8f};
    const float ys[3] = {-0.8f, 0.8f, -0.8f};
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 3; i++) {
        glVertexAttrib3f((GLuint)loc, xs[i], ys[i], 0.0f);
        glVertex3f(xs[i], ys[i], 0.0f);
    }
    glEnd();
    const uint32_t *s = scan_frame();
    int ok = SCAN_PX(s, 30, PROBE_H - 30) == PROBE_BG;

    /* The other winding does appear, so the check is about culling and not about the draw
     * failing for some other reason. */
    glClear(GL_COLOR_BUFFER_BIT);
    const float rx[3] = {-0.8f, 0.8f, -0.8f};
    const float ry[3] = {-0.8f, -0.8f, 0.8f};
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 3; i++) {
        glVertexAttrib3f((GLuint)loc, rx[i], ry[i], 0.0f);
        glVertex3f(rx[i], ry[i], 0.0f);
    }
    glEnd();
    s = scan_frame();
    ok = ok && near_rgb(SCAN_PX(s, 30, PROBE_H - 30), 255, 0, 0, 2);
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_scissor_and_colour_mask(void) {
    reset_view();
    const GLuint p = use_program(VS_PASSTHROUGH,
                                 "void main() { gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0); }\n");
    if (!p) return 0;
    const GLint loc = glGetAttribLocation(p, "pos");

    /* The scissor is a window rectangle with its origin at the bottom-left, so this keeps the
     * lower-left quarter of the probe region. */
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, PROBE_W / 2, PROBE_H / 2);
    /* **The colour mask applies to a shader's output**, which is the part that could have been
     * bypassed: green is masked off, so white becomes magenta. */
    glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE);
    attrib_rect(loc, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);

    const uint32_t *s = scan_frame();
    int ok = near_rgb(SCAN_PX(s, 20, PROBE_H - 20), 255, 0x20, 255, 2);
    ok = ok && SCAN_PX(s, PROBE_W - 20, 20) == PROBE_BG;
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_glsl_120(void) {
    reset_view();
    /* **GLSL 1.20's implicit conversion, measured as a colour.** `2 * 0.25` is 0.5 under 1.20
     * and an error under 1.10; a front end that converted under both would accept shaders the
     * specification rejects, and an interpreter that kept the integer's type would truncate the
     * answer to 0 - a black channel where a half-lit one was meant. The three channels below
     * each take the conversion through a different route. */
    const GLuint p = use_program("#version 120\n"
                                 "attribute vec3 pos;\n"
                                 "void main() { gl_Position = vec4(pos, 1.0); }\n",
                                 "#version 120\n"
                                 "float half_of(float x) { return x * 0.5; }\n"
                                 "void main() {\n"
                                 "  float a = 2 * 0.25;\n"      /* an operator      -> 0.5  */
                                 "  float b = half_of(1);\n"    /* an argument      -> 0.5  */
                                 "  float c = clamp(3, 0, 1);\n"/* a built-in       -> 1.0  */
                                 "  float d = 1;\n"             /* an initialiser   -> 1.0  */
                                 "  gl_FragColor = vec4(a, b * 0.5, c * d * 0.75, 1.0);\n"
                                 "}\n");
    if (!p) return 0;
    attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    int ok = near_rgb(SCAN_PX(s, MID_X, MID_Y), 128, 64, 191, 3);

    /* **The same source is an error in 1.10**, which is the half that has to keep working. */
    GLuint bad = make_shader(GL_FRAGMENT_SHADER,
                             "#version 110\n"
                             "void main() { gl_FragColor = vec4(2 * 0.25); }\n");
    ok = ok && bad == 0u;

    /* And `invariant`, `centroid` and 1.20's two matrix built-ins compile - with the qualifiers
     * on one declaration, which is where the grammar puts them. */
    GLuint quals = make_shader(GL_VERTEX_SHADER,
                               "#version 120\n"
                               "invariant centroid varying vec3 v;\n"
                               "attribute vec3 pos;\n"
                               "invariant gl_Position;\n"
                               "void main() {\n"
                               "  v = transpose(outerProduct(pos, pos)) * pos;\n"
                               "  gl_Position = vec4(pos, 1.0);\n"
                               "}\n");
    ok = ok && quals != 0u;
    if (quals) glDeleteShader(quals);

    GLuint quals110 = make_shader(GL_VERTEX_SHADER,
                                  "#version 110\n"
                                  "centroid varying vec3 v;\n"
                                  "attribute vec3 pos;\n"
                                  "void main() { v = pos; gl_Position = vec4(pos, 1.0); }\n");
    ok = ok && quals110 == 0u;

    /* A later dialect is refused by number rather than taken as 1.20. */
    GLuint v130 = make_shader(GL_FRAGMENT_SHADER,
                              "#version 130\nout vec4 c;\nvoid main() { c = vec4(1.0); }\n");
    ok = ok && v130 == 0u;
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_separate_stencil(void) {
    reset_view();
    /* **The shadow-volume idiom, in one draw**: increment where a front face passes and
     * decrement where a back one does. Two triangles of opposite winding cover the same
     * rectangle, so the stencil ends at its clear value where both were drawn - and an
     * implementation carrying one stencil state for both faces increments twice and leaves 2.
     *
     * The verdict is read by drawing through a GL_EQUAL test afterwards, which is the only way
     * to see a stencil buffer from here. */
    glDisable(GL_CULL_FACE);
    glEnable(GL_STENCIL_TEST);
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
    glStencilFunc(GL_ALWAYS, 0, 0xffu);
    glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR);
    glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR);
    /* Nothing is written to colour while the stencil is being built. */
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    const GLuint p = use_program(VS_PASSTHROUGH,
                                 "void main() { gl_FragColor = vec4(1.0); }\n");
    if (!p) return 0;
    const GLint loc = glGetAttribLocation(p, "pos");
    if (loc < 0) return 0;

    /* The same quad twice, wound opposite ways - one front face and one back. */
    const float xs[6] = {-0.8f, 0.8f, 0.8f, -0.8f, 0.8f, -0.8f};
    const float ys[6] = {-0.8f, -0.8f, 0.8f, -0.8f, 0.8f, 0.8f};
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 6; i++) {
        glVertexAttrib3f((GLuint)loc, xs[i], ys[i], 0.0f);
        glVertex3f(xs[i], ys[i], 0.0f);
    }
    for (int i = 5; i >= 0; i--) {
        glVertexAttrib3f((GLuint)loc, xs[i], ys[i], 0.0f);
        glVertex3f(xs[i], ys[i], 0.0f);
    }
    glEnd();

    /* Now paint where the stencil came back to zero. */
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilFunc(GL_EQUAL, 0, 0xffu);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    const GLuint p2 = use_program(VS_PASSTHROUGH,
                                  "void main() { gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0); }\n");
    if (!p2) return 0;
    attrib_rect(glGetAttribLocation(p2, "pos"), -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);

    const uint32_t *s = scan_frame();
    /* Green inside the first quad means the two faces cancelled; green outside it means the
     * clear value was never disturbed, which is the control. */
    int ok = near_rgb(SCAN_PX(s, MID_X, MID_Y), 0, 255, 0, 2);
    ok = ok && near_rgb(SCAN_PX(s, 4, 4), 0, 255, 0, 2);
    glDisable(GL_STENCIL_TEST);
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_separate_blend_equation(void) {
    reset_view();
    /* The colour subtracts and the alpha adds, in one blend. An implementation carrying one
     * equation for both either subtracts the alpha too or adds the colour, and both land far
     * from the value below. */
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_ADD);

    const GLuint p = use_program(VS_PASSTHROUGH,
                                 "void main() { gl_FragColor = vec4(0.25, 0.25, 0.25, 0.0); }\n");
    if (!p) return 0;
    attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);

    const uint32_t *s = scan_frame();
    /* Destination minus source, 0.5 - 0.25, is 64. Adding would give 191. */
    int ok = near_rgb(SCAN_PX(s, MID_X, MID_Y), 64, 64, 64, 6);
    glDisable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_draw_buffers(void) {
    reset_view();
    GLint v = 0;
    const GLenum one[1] = {GL_BACK};
    (void)glGetError();
    glDrawBuffers(1, one);
    int ok = (glGetError() == GL_NO_ERROR);
    glGetIntegerv(GL_DRAW_BUFFER, &v);
    ok = ok && v == (GLint)GL_BACK;

    /* **A name covering more than one buffer may not appear in the list** (GL 2.0, 4.2.1), and
     * neither may a buffer named twice - both GL_INVALID_OPERATION, not a silent union. */
    const GLenum wide[1] = {GL_FRONT_AND_BACK};
    glDrawBuffers(1, wide);
    ok = ok && glGetError() == GL_INVALID_OPERATION;
    const GLenum twice[2] = {GL_BACK, GL_BACK};
    glDrawBuffers(2, twice);
    ok = ok && glGetError() == GL_INVALID_OPERATION;

    /* And the buffer it selected is still the one a draw reaches. */
    const GLuint p = use_program(VS_PASSTHROUGH,
                                 "void main() { gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n");
    if (!p) return 0;
    attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    ok = ok && near_rgb(SCAN_PX(s, MID_X, MID_Y), 255, 0, 0, 2);
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_draw_is_deterministic(void) {
    reset_view();
    /* The same program drawn twice has to produce the same frame word for word. A shader whose
     * uniforms or attributes were read out of storage that something else moves between draws
     * would differ here and nowhere else. */
    const GLuint p = use_program("uniform float k;\n"
                                 "attribute vec3 pos;\n"
                                 "varying vec3 v;\n"
                                 "void main() {\n"
                                 "  v = vec3(pos.xy * 0.5 + vec2(0.5), k);\n"
                                 "  gl_Position = vec4(pos, 1.0);\n"
                                 "}\n",
                                 "varying vec3 v;\n"
                                 "void main() { gl_FragColor = vec4(v, 1.0); }\n");
    if (!p) return 0;
    glUniform1f(glGetUniformLocation(p, "k"), 0.5f);
    const GLint loc = glGetAttribLocation(p, "pos");
    attrib_rect(loc, -0.9f, -0.9f, 0.9f, 0.9f, 0.0f);

    static uint32_t first[PROBE_W * PROBE_H];
    const uint32_t *s = scan_frame();
    for (int i = 0; i < PROBE_W * PROBE_H; i++) first[i] = s[i];

    glClear(GL_COLOR_BUFFER_BIT);
    attrib_rect(loc, -0.9f, -0.9f, 0.9f, 0.9f, 0.0f);
    s = scan_frame();
    for (int i = 0; i < PROBE_W * PROBE_H; i++) {
        if (s[i] != first[i]) return 0;
    }
    /* And something was actually drawn, so an all-background frame twice does not pass. */
    return SCAN_PX(s, MID_X, MID_Y) != PROBE_BG && glGetError() == GL_NO_ERROR;
}

/* -------------------------------------------------------------------------
 * The table
 * ------------------------------------------------------------------------- */

static const gl2_probe_case_t g_cases[] = {
    /* The object model */
    {"version-gating", check_version_gating},
    {"name-space", check_name_space},
    {"compile-status", check_compile_status},
    {"link-interface", check_link_interface},
    {"link-refuses", check_link_refuses},
    {"deferred-delete", check_deferred_delete},
    {"uniform-type-match", check_uniform_type_match},
    {"uniform-arrays", check_uniform_arrays},
    {"bind-attrib-location", check_bind_attrib_location},
    {"limits", check_limits},
    {"current-program", check_current_program_query},

    /* The vertex stage */
    {"program-draws", check_program_draws},
    {"mvp-uniform", check_mvp_uniform},
    {"attribute-arrays", check_attribute_arrays},
    {"attribute-current", check_attribute_current_value},
    {"builtin-attributes", check_builtin_vertex_attributes},
    {"vertex-only-program", check_vertex_only_program},

    /* Varyings */
    {"varying-interpolates", check_varying_interpolates},
    {"varying-perspective", check_varying_perspective_correct},
    {"several-varyings", check_several_varyings},

    /* The fragment stage */
    {"frag-coord", check_frag_coord},
    {"discard", check_discard},
    {"frag-depth", check_frag_depth},
    {"front-facing", check_front_facing},
    {"fragment-only-program", check_fragment_only_program},

    /* Texturing */
    {"texture-sampler", check_texture_sampler},
    {"sampler-unit", check_sampler_unit_selection},
    {"derivatives", check_derivatives},

    /* The language */
    {"matrix-arithmetic", check_matrix_arithmetic},
    {"swizzles", check_swizzles},
    {"control-flow", check_control_flow},
    {"user-functions", check_user_functions},
    {"builtin-math", check_builtin_math},
    {"mod-and-int-divide", check_mod_is_floored},
    {"relational-builtins", check_relational_builtins},
    {"short-circuit", check_short_circuit},
    {"constructors", check_constructors},
    {"glsl-120", check_glsl_120},

    /* What still applies around a program */
    {"depth-test", check_depth_test_applies},
    {"blend", check_blend_applies},
    {"cull-face", check_cull_face_applies},
    {"scissor-and-mask", check_scissor_and_colour_mask},
    {"deterministic", check_draw_is_deterministic},

    /* GL 2.0's three non-shader additions */
    {"separate-stencil", check_separate_stencil},
    {"separate-blend-eq", check_separate_blend_equation},
    {"draw-buffers", check_draw_buffers},
};

/* **The suite must fit in its callers' result array**, or the checks past the end are run by
 * nobody and counted by nobody - which is how gl1-probe once reported 64/64 with 65 checks in
 * the file. */
_Static_assert(sizeof(g_cases) / sizeof(g_cases[0]) <= GL2_PROBE_MAX_CASES,
               "more checks than GL2_PROBE_MAX_CASES; raise it in gl2_probe.h");

int gl2_probe_case_count(void) { return (int)(sizeof(g_cases) / sizeof(g_cases[0])); }

const char *gl2_probe_case_name(int i) {
    if (i < 0 || i >= gl2_probe_case_count()) return "";
    return g_cases[i].name;
}

int gl2_probe_run(gl2_probe_result_t *out, int max) {
    g_disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, PROBE_DISPLAY_W, PROBE_DISPLAY_H);
    if (!g_disp) return -1;
    /* **A display that could not open is returned, not hidden.** On hardware that meant a NULL
     * framebuffer which looked like a working one until the first check read it. */
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
    /* **A context has the entry points its version defines and no others**, and the default is
     * 1.1 - so without this every call this suite makes would be GL_INVALID_OPERATION and do
     * nothing. Claiming 2.0 is what a GL 2.0 program does; `version-gating` below is the check
     * that a context which has *not* claimed it is refused, which is the half that proves the
     * claim means something. */
    if (!glContextSetVersion(2, 0)) {
        glContextDestroy(ctx);
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
    g_row0 = g_fb_h - PROBE_H;

    const int n = gl2_probe_case_count();
    for (int i = 0; i < n && i < max; i++) {
        /* Errors are cleared between checks so one failure cannot cascade into the next and
         * make a single bug look like a dozen. */
        (void)glGetError();
        out[i].name = g_cases[i].name;
        if (gl2_probe_trace) gl2_probe_trace(g_cases[i].name, -1);
        out[i].passed = g_cases[i].fn();
        /* **Taken before the pixel**, so it is the check's own first error and not anything the
         * read below might raise. The clear above is what makes it the check's own. */
        const GLenum err = glGetError();
        if (gl2_probe_trace) gl2_probe_trace(g_cases[i].name, out[i].passed);
        if (!out[i].passed && gl2_probe_saw) {
            /* **From the snapshot the check itself took**, not a fresh read.
             *
             * `px()` goes through `frame()`, which calls `glFinish` - and on a console oops-gl
             * draws straight into the rotating scanout buffers, so finishing again lands on the
             * next one round: cleared, never drawn into, and reporting the reset colour for
             * every check whatever it actually produced. Eleven of gl2-probe's thirteen
             * drawing failures read that way on 2026-09-22 and the number meant nothing.
             *
             * `g_scan` is the last `scan_frame()` - the pixels the check compared - so this
             * costs no GL call and cannot disturb what it reports. */
            gl2_probe_saw(g_cases[i].name, SCAN_PX(g_scan, MID_X, MID_Y), (unsigned int)err);
        }
    }

    glUseProgram(0);
    if (g_prog) {
        glDeleteProgram(g_prog);
        g_prog = 0u;
    }
    glContextDestroy(ctx);
    oops_display_close(g_disp);
    g_disp = (oops_display_t *)0;
    g_fb = (uint32_t *)0;
    return n < max ? n : max;
}
