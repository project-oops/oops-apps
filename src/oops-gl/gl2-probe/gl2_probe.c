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
void (*gl2_probe_saw)(const char *name, uint32_t centre, unsigned int err, int drawn,
                      uint32_t left, uint32_t right);

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

/* Declared here because the blending checks above its definition report it - see the comment on
 * the definition for what it counts and why a blended check needs it. */
static void uniformity_census(const char *name);

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

/* **A loop whose trip count differs from one fragment to the next.**
 *
 * This is the check the host cannot write. Every other loop in this suite runs the same number
 * of times for every fragment, so a wave executes it in lockstep and the per-lane masks are
 * never asked a question; the compiler's own tests simulate one lane, so they cannot ask one
 * either. Here the `break` fires at a different trip for every column of the quad, which is
 * where the masks either work or do not.
 *
 * **The failure this is shaped to catch is a `break` that leaves the wave rather than the
 * lane.** The masks are scalar registers shared by all 32 lanes: take a lane out with the wrong
 * instruction and the first fragment in a wave to break stops its neighbours too, so everything
 * right of it is clamped to its value and the gradient goes flat partway. Left, middle and
 * right therefore have to be strictly increasing - a flat or reversed reading is the bug, and a
 * single sample in the middle would have read as a plausible colour either way.
 */
static int check_loop_divergence(void) {
    reset_view();
    const GLuint p = use_program("attribute vec3 pos;\n"
                                 "attribute vec4 tint;\n"
                                 "varying vec4 v;\n"
                                 "void main() { v = tint; gl_Position = vec4(pos, 1.0); }\n",
                                 "varying vec4 v;\n"
                                 "void main() {\n"
                                 "  float total = 0.0;\n"
                                 "  for (int i = 0; i < 40; i++) {\n"
                                 "    if (float(i) > v.x * 32.0) break;\n"
                                 "    total += 1.0;\n"
                                 "  }\n"
                                 "  gl_FragColor = vec4(total * 0.03, 0.0, 0.0, 1.0);\n"
                                 "}\n");
    if (!p) return 0;
    const GLint pos = glGetAttribLocation(p, "pos");
    const GLint tint = glGetAttribLocation(p, "tint");
    /* 0 on the left edge, 1 on the right: the trip count runs 1 to 33 across the quad. */
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
    /* One trip on the left edge, seventeen in the middle, thirty-three on the right - times
     * 0.03, so about 0.03, 0.51 and 0.99. The bands are wide because the exact column decides
     * which trip the break lands on; what is not wide is the ordering. */
    int ok = left < mid && mid < right;
    ok = ok && left < 48 && mid > 96 && mid < 160 && right > 208;
    /* Nothing leaked into the other channels, which is what a resurrected lane writing its own
     * answer over a neighbour's would look like. */
    ok = ok && chan_g(SCAN_PX(s, MID_X, MID_Y)) < 4;
    return ok && glGetError() == GL_NO_ERROR;
}

/* **A `discard` inside a loop, taken by some fragments and not others.**
 *
 * A loop reloads `exec` from its active mask at the top of every trip, so a discarded lane left
 * in that mask is handed straight back on the next trip and reaches the export alive. With
 * every fragment discarding, or none, that mistake is invisible: the whole quad goes one way.
 * Here the right half discards and the left half does not, so a lane that came back writes
 * green over a fragment that was thrown away, and the background does not survive on that side.
 *
 * **The trip count is past the unroller on purpose.** A loop that unrolls has no reload to get
 * wrong - each copy simply runs with `exec` at zero - so at twenty-four trips this check would
 * pass without touching the path it is named after. It has to branch to mean anything.
 */
static int check_discard_inside_a_loop(void) {
    reset_view();
    const GLuint p = use_program("attribute vec3 pos;\n"
                                 "attribute vec4 tint;\n"
                                 "varying vec4 v;\n"
                                 "void main() { v = tint; gl_Position = vec4(pos, 1.0); }\n",
                                 "varying vec4 v;\n"
                                 "void main() {\n"
                                 "  float total = 0.0;\n"
                                 "  for (int i = 0; i < 100; i++) {\n"
                                 "    total += 1.0;\n"
                                 "    if (v.x > 0.5 && total > 4.0) discard;\n"
                                 "  }\n"
                                 "  gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
                                 "}\n");
    if (!p) return 0;
    const GLint pos = glGetAttribLocation(p, "pos");
    const GLint tint = glGetAttribLocation(p, "tint");
    static const float corners[4][4] = {
        {0.0f, 0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}};
    attrib_rect2(pos, tint, -1.0f, -0.8f, 1.0f, 0.8f, corners);

    const uint32_t *s = scan_frame();
    /* Green where the loop ran to its end, and the clear value where it discarded. */
    int ok = near_rgb(SCAN_PX(s, 16, MID_Y), 0, 255, 0, 2);
    ok = ok && SCAN_PX(s, PROBE_W - 17, MID_Y) == PROBE_BG;
    return ok && glGetError() == GL_NO_ERROR;
}

/* **An early `return` taken by some fragments and not others.**
 *
 * A guard clause is the shape - `if (...) return x;` and then the real body - and the lanes that
 * take it have to rejoin the caller immediately afterwards. With every fragment going the same
 * way that is invisible: the whole quad returns or none of it does, and a mask that was never
 * restored looks identical to one that was. Here the left half returns early and the right half
 * runs the body, and the green channel is written *after* the call, so a lane whose mask was not
 * put back loses its green rather than its red.
 */
static int check_early_return(void) {
    reset_view();
    const GLuint p = use_program("attribute vec3 pos;\n"
                                 "attribute vec4 tint;\n"
                                 "varying vec4 v;\n"
                                 "void main() { v = tint; gl_Position = vec4(pos, 1.0); }\n",
                                 "varying vec4 v;\n"
                                 "float pick(float a) {\n"
                                 "  if (a < 0.5) { return 0.25; }\n"
                                 "  return 1.0;\n"
                                 "}\n"
                                 "void main() {\n"
                                 "  float r = pick(v.x);\n"
                                 "  gl_FragColor = vec4(r, 1.0, 0.0, 1.0);\n"
                                 "}\n");
    if (!p) return 0;
    const GLint pos = glGetAttribLocation(p, "pos");
    const GLint tint = glGetAttribLocation(p, "tint");
    static const float corners[4][4] = {
        {0.0f, 0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}};
    attrib_rect2(pos, tint, -1.0f, -0.8f, 1.0f, 0.8f, corners);

    const uint32_t *s = scan_frame();
    /* Left returned early (0.25), right ran on (1.0), and both kept the green the caller wrote
     * after the call - which is the half a restored mask is responsible for. */
    int ok = near_rgb(SCAN_PX(s, 16, MID_Y), 64, 255, 0, 6);
    ok = ok && near_rgb(SCAN_PX(s, PROBE_W - 17, MID_Y), 255, 255, 0, 6);
    return ok && glGetError() == GL_NO_ERROR;
}

/* **A local array indexed by an unrolled loop's counter**, which is the shape that makes an
 * array in a register file worth having. Element `k` sits `k * width` registers along, so a
 * stride that is wrong reads a neighbour rather than faulting - and neighbours here are chosen
 * so that reading one gives a visibly different colour rather than a near one. */
static int check_local_arrays(void) {
    /* w = 1, 2, 3, 4. The sum is 10, and 10 * 0.1 is 1.0 - a total no single element reaches,
     * so a loop that read one element four times comes out at 0.4 or less. The green channel
     * takes a single element by literal index, which a wrong stride moves off 0.75. */
    return language_check(
        "  float w[4];\n"
        "  for (int i = 0; i < 4; i++) { w[i] = float(i) + 1.0; }\n"
        "  float total = 0.0;\n"
        "  for (int i = 0; i < 4; i++) { total += w[i]; }\n"
        "  vec3 v[2];\n"
        "  v[0] = vec3(0.0, 0.25, 0.5);\n"
        "  v[1] = vec3(0.75, 1.0, 0.0);\n"
        "  gl_FragColor = vec4(total * 0.1, v[1].x, v[0].z, 1.0);",
        255, 191, 128, 3);
}

/* **A cube map sampled from a compiled shader**, with the hardware picking the face.
 *
 * Each face is a flat colour, so the answer says which face the direction resolved to and not
 * merely that something was sampled. `+X` and `-Z` are chosen because they are the pair a lost
 * sign confuses: `-Z` is face 5 and `+Z` is face 4, and a lowering that dropped the sign would
 * return magenta where cyan is due and still look like a working cube map.
 */
static int check_texture_cube(void) {
    reset_view();
    static const GLubyte faces[6][4] = {
        {255, 0, 0, 255},   /* +X red    */ {0, 255, 0, 255},   /* -X green  */
        {0, 0, 255, 255},   /* +Y blue   */ {255, 255, 0, 255}, /* -Y yellow */
        {255, 0, 255, 255}, /* +Z magenta*/ {0, 255, 255, 255}, /* -Z cyan   */
    };
    GLuint t = 0;
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_CUBE_MAP, t);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (int f = 0; f < 6; f++) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)f, 0, GL_RGBA, 1, 1, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, faces[f]);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    /* The left half looks along +X and the right half along -Z, decided by the varying so the
     * two halves are one draw and one shader. */
    const GLuint p = use_program("attribute vec3 pos;\n"
                                 "varying vec3 dir;\n"
                                 "void main() {\n"
                                 "  dir = (pos.x < 0.0) ? vec3(1.0, 0.0, 0.0)\n"
                                 "                      : vec3(0.0, 0.0, -1.0);\n"
                                 "  gl_Position = vec4(pos, 1.0);\n"
                                 "}\n",
                                 "uniform samplerCube sky;\n"
                                 "varying vec3 dir;\n"
                                 "void main() { gl_FragColor = textureCube(sky, dir); }\n");
    if (!p) {
        glDeleteTextures(1, &t);
        return 0;
    }
    attrib_rect(glGetAttribLocation(p, "pos"), -0.9f, -0.8f, 0.9f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    int ok = near_rgb(SCAN_PX(s, 16, MID_Y), 255, 0, 0, 2);            /* +X, face 0 */
    ok = ok && near_rgb(SCAN_PX(s, PROBE_W - 17, MID_Y), 0, 255, 255, 2); /* -Z, face 5 */
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glDeleteTextures(1, &t);
    return ok && glGetError() == GL_NO_ERROR;
}

/* `texture2DProj` divides by the coordinate's last component. The `vec4` form divides by `w` and
 * **ignores `z`**, which is the rule worth measuring: a lowering that took "the last component
 * of the vector" would divide by the 99.0 parked in `z` and sample a corner. */
static int check_texture_proj(void) {
    reset_view();
    GLuint tex = make_flat_texture(GL_TEXTURE0, 0xff0080ffu);
    glActiveTexture(GL_TEXTURE0);
    const GLuint p = use_program("attribute vec3 pos;\n"
                                 "varying vec2 uv;\n"
                                 "void main() {\n"
                                 "  uv = pos.xy * 0.5 + vec2(0.5);\n"
                                 "  gl_Position = vec4(pos, 1.0);\n"
                                 "}\n",
                                 "uniform sampler2D tex;\n"
                                 "varying vec2 uv;\n"
                                 "void main() {\n"
                                 "  vec4 q = vec4(uv * 2.0, 99.0, 2.0);\n"
                                 "  gl_FragColor = texture2DProj(tex, q);\n"
                                 "}\n");
    if (!p) { glDeleteTextures(1, &tex); return 0; }
    attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    const int ok = near_rgb(SCAN_PX(s, MID_X, MID_Y), 255, 128, 0, 3);
    glDeleteTextures(1, &tex);
    return ok && glGetError() == GL_NO_ERROR;
}

/* **A volume sampled from a compiled shader.** Each slice is a flat colour, so the answer says
 * which slice the third coordinate reached - and that is the thing a volume can get wrong that
 * a 2D cannot. r = 0.75 lands in the second of two slices. */
static int check_texture_3d(void) {
    reset_view();
    static const GLubyte slices[2][4] = {{255, 0, 0, 255}, {0, 0, 255, 255}};
    GLubyte vol[2 * 4];
    for (int i = 0; i < 2; i++) {
        for (int k = 0; k < 4; k++) vol[i * 4 + k] = slices[i][k];
    }
    GLuint t = 0;
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_3D, t);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 1, 1, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, vol);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    const GLuint p = use_program(VS_PASSTHROUGH,
                                 "uniform sampler3D vol;\n"
                                 "void main() {\n"
                                 "  gl_FragColor = texture3D(vol, vec3(0.5, 0.5, 0.75));\n"
                                 "}\n");
    if (!p) { glDeleteTextures(1, &t); return 0; }
    attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    const int ok = near_rgb(SCAN_PX(s, MID_X, MID_Y), 0, 0, 255, 4); /* the far slice */
    glBindTexture(GL_TEXTURE_3D, 0);
    glDeleteTextures(1, &t);
    return ok && glGetError() == GL_NO_ERROR;
}

/* **A shadow lookup compares instead of returning a texel**, and the comparison is the
 * sampler's own `GL_TEXTURE_COMPARE_FUNC`. The stored depth is 0.5, so a reference of 0.25
 * passes and 0.75 fails under less-or-equal - and both halves are drawn, because with `s` below
 * the stored depth a lowering that left the reference in the wrong address register still gets
 * the passing half right and only the failing half gives it away. */
static int check_shadow_compare(void) {
    reset_view();
    static const GLfloat depth = 0.5f;
    GLuint t = 0;
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 1, 1, 0, GL_DEPTH_COMPONENT, GL_FLOAT,
                 &depth);

    /* The left half references 0.25 and the right half 0.75, from the varying. */
    const GLuint p = use_program("attribute vec3 pos;\n"
                                 "varying float ref;\n"
                                 "void main() {\n"
                                 "  ref = (pos.x < 0.0) ? 0.25 : 0.75;\n"
                                 "  gl_Position = vec4(pos, 1.0);\n"
                                 "}\n",
                                 "uniform sampler2DShadow depth;\n"
                                 "varying float ref;\n"
                                 "void main() {\n"
                                 "  gl_FragColor = shadow2D(depth, vec3(0.5, 0.5, ref));\n"
                                 "}\n");
    if (!p) { glDeleteTextures(1, &t); return 0; }
    attrib_rect(glGetAttribLocation(p, "pos"), -0.9f, -0.8f, 0.9f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    int ok = near_rgb(SCAN_PX(s, 16, MID_Y), 255, 255, 255, 4);            /* 0.25 passes */
    ok = ok && near_rgb(SCAN_PX(s, PROBE_W - 17, MID_Y), 0, 0, 0, 4);      /* 0.75 fails */
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &t);
    return ok && glGetError() == GL_NO_ERROR;
}

/* `m * m` is a product and not componentwise, and `transpose` moves the off-diagonal. Values
 * chosen so the product, the componentwise answer and the transpose are three different
 * colours - a matrix test on symmetric operands passes with the rows and columns swapped. */
static int check_matrix_products(void) {
    return language_check_src(
        "#version 120\n"
        "void main() {\n"
        "  mat2 a = mat2(1.0, 2.0, 0.0, 1.0);\n"   /* col0 = (1,2), col1 = (0,1) */
        "  mat2 b = mat2(3.0, 0.0, 0.0, 4.0);\n"
        "  mat2 p = a * b;\n"                      /* (1,0) element is 6 */
        "  mat2 c = matrixCompMult(a, b);\n"       /* the same element is 0 */
        "  mat2 t = transpose(a);\n"               /* t[1][0] is 2, a[1][0] is 0 */
        "  gl_FragColor = vec4(p[0][1] * 0.125, c[0][1], t[1][0] * 0.5, 1.0);\n"
        "}\n",
        191, 0, 255, 3);
}

/* The inverse trigonometric functions, against values that are exact in the language's own
 * terms: asin(1) is pi/2, acos(0) is pi/2, atan(1) is pi/4. Scaled by 1/pi so the channels are
 * 0.5, 0.5 and 0.25 - and a lowering that lost a quadrant fixup lands on none of them. */
static int check_inverse_trig(void) {
    return language_check(
        "  float a = asin(1.0) * 0.3183098862;\n"
        "  float b = acos(0.0) * 0.3183098862;\n"
        "  float c = atan(1.0) * 0.3183098862;\n"
        "  gl_FragColor = vec4(a, b, c, 1.0);",
        128, 128, 64, 4);
}

/* `refract` returns the zero vector under total internal reflection, which is the
 * specification's wording and the half a shader leans on. Encoded as `r * 0.5 + 0.5`, so the
 * zero vector is a flat grey and a NaN that escaped the select is not. */
static int check_refract(void) {
    return language_check(
        "  vec3 i = normalize(vec3(1.0, -0.05, 0.0));\n"
        "  vec3 r = refract(i, vec3(0.0, 1.0, 0.0), 2.0);\n"
        "  gl_FragColor = vec4(r * 0.5 + 0.5, 1.0);",
        128, 128, 128, 3);
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
    const int ok = near_rgb(SCAN_PX(s, MID_X, MID_Y), 0x8f, 0x8f, 0x8f, 3);
    /* **And this one is in the affected class too.** `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA` at
     * source alpha 0.5 is `src/2 + dst/2` - a genuine combination of both terms, which
     * `blend-uniformity` measured as correct at one pixel in every 2x2 quad and wrong at the
     * other three. This check has been passing on hardware from the region's centre, which is
     * the even/even pixel the lattice gets right. Diagnostic for the same reason
     * `separate-blend-eq`'s is; `-5b8e` is where it is answered. */
    uniformity_census("blend/uniformity");
    return ok && glGetError() == GL_NO_ERROR;
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
    /*
     * **And whether that centre pixel speaks for the region**, which until 2026-09-23 nobody
     * had asked.
     *
     * `blend-uniformity` established that a blend *combining* two terms is correct at one pixel
     * in every 2x2 quad and wrong at the other three, while a blend whose result is one operand
     * is correct everywhere. `GL_ONE, GL_ONE` under `GL_FUNC_REVERSE_SUBTRACT` combines, so this
     * check is in the affected class - and it decides from a single pixel, the region's centre,
     * which is even/even: the lane the lattice gets right.
     *
     * That matters well beyond this check. `0xff40c040` is the value two obSCEne requests are
     * built on - `-9c31`'s reading that the pairing is positional by channel, and `-4d07`'s
     * `LINEAR_GENERAL` arm - and every arm in both read one pixel. This row says whether the
     * value they were reasoning about is the region's or one lane's.
     *
     * Diagnostic, not the verdict: what this check exists to measure is the *equation*, and
     * failing it for a fault every blended draw shares would bury that. `-5b8e` is where the
     * lattice is answered; when it is, the verdict here can widen.
     */
    uniformity_census("separate-blend-eq/uniformity");
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

/* One pixel of a named colour buffer, packed the way the `saw` rows read: A, R, G, B.
 *
 * A single-pixel read is the thing `scan_frame`'s comment warns against doing *after* a scan,
 * because `frame()` finishes again and lands on the next scanout buffer round. This is not that
 * read: it goes through `glReadPixels`, which reads the buffer it is told to by name rather than
 * the rotating readback copy, and the check below takes every one of them before it scans.
 * gl1-probe's `read_centre` is the same two calls and passes on hardware in `front-buffer`. */
static uint32_t pixel_of(GLenum buffer, int gx, int gy) {
    GLubyte c[4] = {0u, 0u, 0u, 0u};
    glReadBuffer(buffer);
    glReadPixels(gx, gy, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, c);
    glReadBuffer(GL_BACK);
    return ((uint32_t)c[3] << 24) | ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) |
           (uint32_t)c[2];
}

static uint32_t centre_of(GLenum buffer) { return pixel_of(buffer, MID_X, MID_Y); }

/*
 * **The row `scan_frame` calls the centre is not the row `glReadPixels` calls the centre**, and
 * the two have been disagreeing about the same "pixel".
 *
 * `scan_frame` indexes the readback image top-down from `g_row0 = g_fb_h - PROBE_H`, so its
 * `MID_Y` is display row `g_row0 + 48`; `glGetFrameReadback` fills that image with GL y
 * `height - 1 - row`, which makes it **GL row 47**. `glReadPixels(MID_X, MID_Y)` asks for GL row
 * **48**. Adjacent rows, both well inside a rectangle drawn over the whole region - so for a
 * uniform draw they must agree, and they do not: the verdict row reads `0xff4080ff` where
 * `centre_of` reads `0xff408000`, the same three channels and a blue 255 apart.
 *
 * Which makes "the region is not uniform" the thing to measure, and it costs one more read.
 * If GL row 47 answers 255 and row 48 answers 0, the two-target draw is writing some rows and
 * not others and the read path was never the fault here; if both answer 0, the two paths really
 * are reading the same pixel differently and the fault is below `glReadPixels`.
 */
static uint32_t row_below_centre_of(GLenum buffer) { return pixel_of(buffer, MID_X, MID_Y - 1); }

/*
 * **The shape of the back's blue channel across the whole region**, from the snapshot
 * `scan_frame` already takes, so it costs no GL call.
 *
 * Two samples cannot name a boundary: every other row, one row, a tile edge and half the region
 * are all consistent with "row 47 is 255 and row 48 is 0", and which it is decides whether a
 * fault is rasterisation, addressing or the export. So count the region and print where the
 * values sit.
 *
 * A scan row `y` is GL row `PROBE_H - 1 - y`, because `scan_frame` indexes the readback image
 * top-down from `g_row0` while `glGetFrameReadback` fills it with GL `height - 1 - row`. The
 * masks are in **scan** coordinates, so bit 0 of `-rows` is GL row 95.
 *
 * Three buckets rather than two, because a third value anywhere means this is not a clean split
 * and the bit masks are the wrong instrument to read it with.
 */
/*
 * **Which channels are a lattice, measured against the region's own centre.**
 *
 * `blue_census` says blue is right at one pixel in four and wrong at the rest, for a blend into
 * one target as much as two. It cannot say whether blue is *written* at one pixel in four or
 * *blended* there - the difference between a draw that never put the channel down and a colour
 * block that dropped the destination for three lanes of every quad - and those are different
 * bugs with different registers behind them. Run after a plain unblended draw, this answers it:
 * a lattice with no blending in the frame at all belongs to the draw.
 *
 * Counts pixels differing from the centre rather than from an expected constant, so it needs no
 * argument and reads the same after any uniform draw. The centre is even/even, which the lattice
 * gets right, so "differs from the centre" is "wrong" wherever the census above found a lattice.
 */
static void uniformity_census(const char *name) {
    const uint32_t *const s = scan_frame();
    const uint32_t mid = SCAN_PX(s, MID_X, MID_Y);
    int dr = 0, dg = 0, db = 0;
    for (int y = 0; y < PROBE_H; y++) {
        for (int x = 0; x < PROBE_W; x++) {
            const uint32_t c = SCAN_PX(s, x, y);
            if (chan_r(c) != chan_r(mid)) dr++;
            if (chan_g(c) != chan_g(mid)) dg++;
            if (chan_b(c) != chan_b(mid)) db++;
        }
    }
    /* `saw` is the centre the three counts are measured against, so a row is self-contained. */
    if (gl2_probe_saw) gl2_probe_saw(name, mid, 0u, dr, (uint32_t)dg, (uint32_t)db);
}

static void blue_census(const char *name_count, const char *name_rows, const char *name_cols,
                        const char *name_cols3) {
    const uint32_t *const s = scan_frame();
    int n_full = 0, n_zero = 0, n_other = 0;
    uint32_t rows[3] = {0u, 0u, 0u};     /* scan rows 0..95 at the centre column */
    uint32_t cols[4] = {0u, 0u, 0u, 0u}; /* scan cols 0..127 at the centre row */
    for (int y = 0; y < PROBE_H; y++) {
        for (int x = 0; x < PROBE_W; x++) {
            const int b = chan_b(SCAN_PX(s, x, y));
            if (b >= 250) n_full++;
            else if (b <= 5) n_zero++;
            else n_other++;
        }
        if (chan_b(SCAN_PX(s, MID_X, y)) >= 250) rows[y >> 5] |= 1u << (y & 31);
    }
    for (int x = 0; x < PROBE_W; x++) {
        if (chan_b(SCAN_PX(s, x, MID_Y)) >= 250) cols[x >> 5] |= 1u << (x & 31);
    }
    if (!gl2_probe_saw) return;
    gl2_probe_saw(name_count, (uint32_t)n_full, 0u, 0, (uint32_t)n_zero, (uint32_t)n_other);
    gl2_probe_saw(name_rows, rows[0], 0u, 0, rows[1], rows[2]);
    gl2_probe_saw(name_cols, cols[0], 0u, 0, cols[1], cols[2]);
    gl2_probe_saw(name_cols3, cols[3], 0u, 0, 0u, 0u);
}

/*
 * **The ten GL 2.0 entry points nothing in this suite had ever called** (2026-09-23).
 *
 * Every GL 2.0 function is implemented; an audit of the specification's list against
 * `include/GL/gl.h` and the definitions behind it finds none missing. What the audit did find is
 * that ten of them had never been *exercised* here: `glGetShaderSource`, `glGetAttachedShaders`,
 * `glGetActiveAttrib`, `glGetUniformiv`, `glGetVertexAttribfv`, `glGetVertexAttribiv`,
 * `glGetVertexAttribPointerv`, `glStencilMaskSeparate`, `glDetachShader` and
 * `glValidateProgram`.
 *
 * **They are all introspection, which is why it matters.** A port does not call these to draw;
 * it calls them to find out what it is holding - SDL asking which attributes a program has, a
 * loader reading a shader back, an engine restoring vertex array state. A wrong answer from one
 * of them does not produce a wrong picture that a probe would notice. It produces a port that
 * binds the wrong attribute and draws nothing, and the drawing checks all still pass.
 *
 * Nothing here draws, so this check's verdict is the same on the host and on a console - which
 * makes it one of the few that can be trusted while the blend lattice is open.
 */
static int check_program_introspection(void) {
    reset_view();
    const char *const VS_SRC = "attribute vec3 pos;\n"
                               "attribute vec2 uv;\n"
                               "uniform int k;\n"
                               "varying vec2 v;\n"
                               "void main() { v = uv * float(k); gl_Position = vec4(pos, 1.0); }\n";
    GLuint vs = make_shader(GL_VERTEX_SHADER, VS_SRC);
    GLuint fs = make_shader(GL_FRAGMENT_SHADER,
                            "varying vec2 v;\n"
                            "void main() { gl_FragColor = vec4(v, 0.0, 1.0); }\n");
    if (!vs || !fs) return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) { glDeleteProgram(prog); return 0; }
    g_prog = prog;
    glUseProgram(prog);
    int ok = 1;

    /* **The source comes back as it went in**, with its length not counting the terminator -
     * which is the rule these getters share and the one easiest to get wrong by one. */
    {
        char buf[256];
        GLsizei len = -1;
        for (int i = 0; i < 256; i++) buf[i] = '\0';
        glGetShaderSource(vs, (GLsizei)sizeof(buf), &len, buf);
        int n = 0;
        while (VS_SRC[n] != '\0') n++;
        ok = ok && len == (GLsizei)n && buf[n] == '\0';
        for (int i = 0; i < n && ok; i++) ok = ok && buf[i] == VS_SRC[i];
    }

    /* Both shaders come back attached, and `count` is what was written rather than the capacity. */
    {
        GLuint got[4] = {0u, 0u, 0u, 0u};
        GLsizei count = -1;
        glGetAttachedShaders(prog, 4, &count, got);
        ok = ok && count == 2;
        const int has_vs = (got[0] == vs || got[1] == vs);
        const int has_fs = (got[0] == fs || got[1] == fs);
        ok = ok && has_vs && has_fs;
    }

    /* **An active attribute by index**, with its name, type and size. The index is not the
     * location - the specification is explicit that they are unrelated - so the name is looked
     * up rather than assumed, and its location asked for separately. */
    {
        GLint active = 0;
        glGetProgramiv(prog, GL_ACTIVE_ATTRIBUTES, &active);
        ok = ok && active == 2;
        int seen_pos = 0, seen_uv = 0;
        for (GLint i = 0; i < active && ok; i++) {
            char name[64];
            GLsizei len = 0;
            GLint size = 0;
            GLenum type = 0;
            for (int c = 0; c < 64; c++) name[c] = '\0';
            glGetActiveAttrib(prog, (GLuint)i, (GLsizei)sizeof(name), &len, &size, &type, name);
            ok = ok && size == 1 && len > 0;
            if (name[0] == 'p') { seen_pos = 1; ok = ok && type == GL_FLOAT_VEC3; }
            if (name[0] == 'u') { seen_uv = 1; ok = ok && type == GL_FLOAT_VEC2; }
        }
        ok = ok && seen_pos && seen_uv;
    }

    /* An integer uniform, read back through the integer getter rather than the float one. */
    {
        const GLint loc = glGetUniformLocation(prog, "k");
        ok = ok && loc >= 0;
        glUniform1i(loc, 7);
        GLint got = 0;
        glGetUniformiv(prog, loc, &got);
        ok = ok && got == 7;
    }

    /* **Vertex array state, read back through all three getters.** The pointer one takes a
     * `void **`, which is the odd signature in this family and the one a port gets wrong. */
    {
        static const float verts[6] = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
        const GLint pos_loc = glGetAttribLocation(prog, "pos");
        ok = ok && pos_loc >= 0;
        glEnableVertexAttribArray((GLuint)pos_loc);
        glVertexAttribPointer((GLuint)pos_loc, 3, GL_FLOAT, GL_FALSE, 12, verts);
        GLint enabled = 0, size = 0, stride = 0;
        GLenum type = 0;
        glGetVertexAttribiv((GLuint)pos_loc, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
        glGetVertexAttribiv((GLuint)pos_loc, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
        glGetVertexAttribiv((GLuint)pos_loc, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
        {
            GLint t = 0;
            glGetVertexAttribiv((GLuint)pos_loc, GL_VERTEX_ATTRIB_ARRAY_TYPE, &t);
            type = (GLenum)t;
        }
        ok = ok && enabled == GL_TRUE && size == 3 && stride == 12 && type == GL_FLOAT;
        void *ptr = (void *)0;
        glGetVertexAttribPointerv((GLuint)pos_loc, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr);
        ok = ok && ptr == (void *)verts;
        /* The current generic value, through the float getter. */
        glVertexAttrib4f((GLuint)pos_loc, 0.25f, 0.5f, 0.75f, 1.0f);
        float cur[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        glGetVertexAttribfv((GLuint)pos_loc, GL_CURRENT_VERTEX_ATTRIB, cur);
        ok = ok && near_chan((int)(cur[0] * 255.0f + 0.5f), 64, 1) &&
             near_chan((int)(cur[2] * 255.0f + 0.5f), 191, 1);
        glDisableVertexAttribArray((GLuint)pos_loc);
    }

    /* **The two stencil write masks move independently**, which is the whole point of the
     * separate form and cannot be seen from one of them. */
    {
        glStencilMaskSeparate(GL_FRONT, 0x0fu);
        glStencilMaskSeparate(GL_BACK, 0xf0u);
        GLint front = 0, back = 0;
        glGetIntegerv(GL_STENCIL_WRITEMASK, &front);
        glGetIntegerv(GL_STENCIL_BACK_WRITEMASK, &back);
        ok = ok && front == 0x0f && back == 0xf0;
        glStencilMaskSeparate(GL_FRONT_AND_BACK, 0xffu);
    }

    /* Validation answers, and detaching leaves one shader attached. */
    {
        glValidateProgram(prog);
        GLint valid = -1;
        glGetProgramiv(prog, GL_VALIDATE_STATUS, &valid);
        ok = ok && (valid == GL_TRUE || valid == GL_FALSE);
        glDetachShader(prog, fs);
        GLsizei count = -1;
        GLuint got[4] = {0u, 0u, 0u, 0u};
        glGetAttachedShaders(prog, 4, &count, got);
        ok = ok && count == 1 && got[0] == vs;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return ok && glGetError() == GL_NO_ERROR;
}

/*
 * **`gl_PointCoord`** (GLSL 1.20; since 2026-09-23) - where the fragment sits inside the point,
 * (0,0) at one corner and (1,1) at the other.
 *
 * Refused by this implementation until today on the grounds that point sprites were not
 * implemented, which had not been true since 2026-09-20: gl1-probe's `point-sprite` drives
 * `GL_COORD_REPLACE` and passes on hardware. So the interesting part of this check is not that
 * the name compiles - a unit test says that - but that the **value is right across the point**,
 * which only a drawn sprite can show.
 *
 * One large point, its fragment shader painting `vec4(gl_PointCoord, 0, 1)`. Red therefore runs
 * left to right and green top to bottom, and the four quadrants of the point are distinguishable
 * from one another: reading one pixel would pass on a shader that returned a constant. The
 * corners are sampled inside the point rather than at its edge, where a half-pixel of coverage
 * decides whether the sample lands on the sprite at all.
 *
 * **t runs downward**, because `GL_POINT_SPRITE_COORD_ORIGIN` defaults to `GL_UPPER_LEFT` - the
 * opposite of the rest of GL, and the specification's own choice. So the *upper* half of the
 * point on screen holds the small green values, and this check would pass just as well with the
 * origin flipped if it only looked at one axis.
 *
 * **No `glEnable(GL_POINT_SPRITE)`.** That switch belongs to ARB_point_sprite and the
 * fixed-function path; `gl_PointCoord` is defined for any point, and a shader reading it gets
 * the coordinate from the program alone. A check that enabled the switch would pass without
 * proving that.
 */
static int check_point_coord(void) {
    reset_view();
    /* **A fragment shader on its own**, which is what `gl_PointCoord` requires here and what the
     * link says so. A point is expanded into its square before the vertex stage, in object space
     * through the inverse MVP; a vertex shader recomputes each corner from attributes that are
     * the same for all four, so it collapses the square. The fixed-function vertex stage is the
     * one that transforms the expansion as it was built. */
    GLuint fs = make_shader(GL_FRAGMENT_SHADER,
                            "void main() { gl_FragColor = vec4(gl_PointCoord, 0.0, 1.0); }\n");
    if (!fs) return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(fs);
    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) { glDeleteProgram(prog); return 0; }
    g_prog = prog;
    glUseProgram(prog);

    /* Large enough that its quadrants are tens of pixels apart, and centred, so the whole point
     * lies inside the region however the rasteriser rounds its edges. */
    glPointSize(64.0f);
    glBegin(GL_POINTS);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glEnd();
    glPointSize(1.0f);

    const uint32_t *const s = scan_frame();
    /* A 64-pixel point centred in a 128x96 region spans x 32..96 and y 16..80 in scan rows.
     * Sampled a quarter of the way in from each edge, well clear of the coverage boundary. */
    const uint32_t tl = SCAN_PX(s, 48, 32);
    const uint32_t tr = SCAN_PX(s, 80, 32);
    const uint32_t bl = SCAN_PX(s, 48, 64);
    const uint32_t br = SCAN_PX(s, 80, 64);
    if (gl2_probe_saw) {
        gl2_probe_saw("point-coord/top", tl, 0u, 0, tr, 0u);
        gl2_probe_saw("point-coord/bottom", bl, 0u, 0, br, 0u);
    }

    /* s rises to the right on both rows, and t rises downward on both columns - each asserted as
     * an ordering rather than a value, because the exact coordinate at a sample depends on where
     * the rasteriser put the point's edges. A constant, a swapped pair or an inverted axis all
     * fail; a point drawn a pixel off does not. */
    int ok = chan_r(tr) > chan_r(tl) + 32 && chan_r(br) > chan_r(bl) + 32;
    ok = ok && chan_g(bl) > chan_g(tl) + 32 && chan_g(br) > chan_g(tr) + 32;
    /* And blue is the constant the shader wrote, so a quadrant that is background rather than
     * sprite is caught rather than read as a coordinate. */
    ok = ok && chan_b(tl) == 0 && chan_b(br) == 0;
    return ok && glGetError() == GL_NO_ERROR;
}

/*
 * **Which blends are a lattice, and whether the arithmetic has anything to do with it.**
 *
 * `two-draw-buffers` established that a `GL_ONE, GL_ONE` blend is correct at one pixel in every
 * 2x2 quad and wrong at the other three, while an unblended draw of the same shader over the
 * same region is uniform to the last pixel - and that one colour target and two behave
 * identically. `REQ-20260923T2015Z-5b8e` carries that to obSCEne.
 *
 * The question that request will be asked back is the shape of it, and four blend modes answer
 * it. **`GL_ONE, GL_ZERO` is the one that matters most**: arithmetically it is a copy - the
 * destination contributes nothing and the result is the source, exactly what the unblended draw
 * writes. If that is *also* a lattice, then the fault is the colour block being switched on at
 * all and not the sum it computes, and no amount of reading blend factors will find it. If it is
 * clean, the arithmetic is implicated and the destination fetch is the thing to look at.
 *
 * `GL_ZERO, GL_ONE` is its mirror: the source contributes nothing and the destination must
 * survive untouched. `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA` is the blend real programs use, so
 * its census says how much of the port this actually costs.
 *
 * Each arm paints its own destination, blends over it, and reports the per-channel count of
 * pixels differing from the region's centre - the centre being even/even, the lane the lattice
 * gets right. A clean arm reports three zeros.
 *
 * **Which checks this calls into question**, audited 2026-09-23 so the answer to `-5b8e` has a
 * list to be applied to. A check is affected when it decides from a single pixel of a blend that
 * combines both terms; one whose result is a single operand is not.
 *
 *   gl2-probe: `blend` (`GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA` at alpha 0.5),
 *   `separate-blend-eq` (`GL_ONE, GL_ONE` reverse-subtract), `two-draw-buffers`, and this check.
 *   The first two now census themselves; they still pass on their centre pixel.
 *
 *   gl1-probe, by what the factors actually compute rather than by whether blending is on:
 *
 *     **combining, so affected** - `blend`, `texture-luminance`, `pixel-fragments`,
 *     `internal-formats`, `smooth`, `polygon-smooth`, `smooth-textured`, `tex-unit1-stretch`,
 *     `blend-over-texture` and `lit-texture-parity` (all `GL_SRC_ALPHA,
 *     GL_ONE_MINUS_SRC_ALPHA`); `attrib-stack`, `front-and-back` and `blend-equation`
 *     (`GL_ONE, GL_ONE`); `blend-additive-strip` (`GL_SRC_ALPHA, GL_ONE`).
 *
 *     **not combining, so not affected** - `blend-constant` and `logic-op`, whose every arm is
 *     `<something>, GL_ZERO` or `GL_ONE, GL_ZERO`: the result is one operand scaled, and
 *     `blend-uniformity` measured that case clean. `blend-constant` passing with exact byte
 *     values on hardware is consistent with it and is a small piece of corroboration.
 *
 * **Two caveats, because this is a reading of source and not a measurement.** `GL_SRC_ALPHA,
 * GL_ONE_MINUS_SRC_ALPHA` only combines when the source alpha is strictly between 0 and 1 - at
 * alpha 1 it degenerates to a copy, which is how the first version of this very check produced a
 * clean arm and nearly a wrong conclusion. And `GL_MAX` in `blend-equation` selects an operand
 * per channel rather than summing, so whether it is affected is genuinely unknown.
 *
 * So fourteen gl1 checks and three here decide from one pixel of a blend that combines. None is
 * known wrong - the lattice's correct lane is exactly where they sample - and none is known
 * right either. `polygon-smooth` and `smooth` are the ones to look at first: antialiasing *is* a
 * blend, and a coverage fade landing on one lane in four reads as a working fade at any single
 * sample. That is the state to hold until `-5b8e` comes back.
 */
static int check_blend_uniformity(void) {
    reset_view();
    const GLuint p = use_program(VS_PASSTHROUGH,
                                 "uniform vec4 c;\n"
                                 "void main() { gl_FragColor = c; }\n");
    if (!p) return 0;
    const GLint pos = glGetAttribLocation(p, "pos");
    const GLint kc = glGetUniformLocation(p, "c");
    if (pos < 0 || kc < 0) return 0;

    /*
     * **A blend only counts as one if both terms survive it**, and the source's alpha is what
     * decides that for the factors real programs use. The first run of this check set it to 1.0,
     * which turns `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA` into `src * 1 + dst * 0` - the same copy
     * `GL_ONE, GL_ZERO` performs. Three of the four arms reduced to "the result is one operand",
     * all three came back clean, and the one arm that genuinely added two non-zero terms was the
     * one that showed the lattice. That is a result, but it was nearly an accident: 0.5 here is
     * what makes the alpha arm a real mixture and the reading a measurement.
     */
    static const struct {
        const char *name;
        GLenum src, dst;
        float src_alpha;
    } MODES[5] = {
        {"blend-uniformity/one-one", GL_ONE, GL_ONE, 1.0f},
        {"blend-uniformity/one-zero", GL_ONE, GL_ZERO, 1.0f},
        {"blend-uniformity/zero-one", GL_ZERO, GL_ONE, 1.0f},
        /* Half alpha, so this is `src/2 + dst/2` and both terms are really there. */
        {"blend-uniformity/src-alpha", GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, 0.5f},
        /* A second genuine sum that does not go through alpha at all, so a fault that turns out
         * to be alpha's can be told from one that belongs to the addition. */
        {"blend-uniformity/dst-color", GL_DST_COLOR, GL_ONE, 1.0f},
    };

    int ok = 1;
    for (int m = 0; m < 5; m++) {
        /* A destination with something in every channel, so no arm can be clean by arithmetic
         * accident - a channel that is zero either side proves nothing. */
        glDisable(GL_BLEND);
        glUniform4f(kc, 0.125f, 0.25f, 0.5f, 1.0f);
        attrib_rect(pos, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);

        glEnable(GL_BLEND);
        glBlendFunc(MODES[m].src, MODES[m].dst);
        glUniform4f(kc, 0.25f, 0.5f, 0.125f, MODES[m].src_alpha);
        attrib_rect(pos, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
        glDisable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ZERO);

        uniformity_census(MODES[m].name);

        /*
         * **Every arm is in the verdict, not just the last one.** The first version scanned once
         * after the loop, which measured whichever mode happened to run last - and reported
         * `pass` on a run whose `one-one` census had printed 6144, 6144, 9216 beside it. A check
         * that prints its own contradiction and calls itself green is worse than one that does
         * not look: the rows were read by a person, and the verdict would have been read by a
         * count.
         */
        const uint32_t *const s = scan_frame();
        const uint32_t mid = SCAN_PX(s, MID_X, MID_Y);
        for (int y = 0; y < PROBE_H && ok; y++) {
            for (int x = 0; x < PROBE_W && ok; x++) {
                if (SCAN_PX(s, x, y) != mid) ok = 0;
            }
        }
    }
    return ok && glGetError() == GL_NO_ERROR;
}

/*
 * **Two colour buffers written by one compiled shader**, each blending against its own
 * destination.
 *
 * `draw-buffers` above measures the API and then draws into one buffer. Nothing in this suite
 * has ever put a fragment into two colour targets, which is why the fault below has only ever
 * been visible from the other suite.
 *
 * **What this is here to tell apart.** gl1-probe's `front-and-back` reaches two targets through
 * `glDrawBuffer(GL_FRONT_AND_BACK)` and fails on hardware in a particular shape: red and green
 * blend correctly against each target's own destination, and **blue comes back the same byte in
 * both** - `0x14`, `0x56`, `0xb9`, `0xd3`, `0x4e` across five runs, drifting between runs and
 * stable within one. A byte equal in both targets cannot be a blend result. The two destinations
 * there differ by the whole range in blue, and under `GL_ONE, GL_ONE` the one with 255 in it
 * saturates whatever the source is; 78 is not a value that blend can produce.
 *
 * That check's source blue is 0.0. This one drives the same two-export path twice - once with
 * the blue set and once with it zero - so the two readings can be separated:
 *
 *   - a defined blue survives and a zero one does not: what reaches the export is the fault,
 *     not the export, and the next question is which lane the second export reads.
 *   - neither survives: the second export itself, and this file reproduces it from a shader
 *     whose output it chooses, which is a far shorter loop than the fixed-function path.
 *   - both survive: `front-and-back`'s fault is not the two-export path at all, and belongs to
 *     GL 1.x's own colour path - which would retire a line of enquiry rather than open one.
 *
 * `glDrawBuffers(2, {GL_BACK, GL_FRONT})` is GL 2.0's way to the same place. The union of two
 * distinct names is what `GL_FRONT_AND_BACK` names in one, and the list form is how a program
 * asks for it without naming a buffer that covers more than one - which 4.2.1 refuses, and which
 * `draw-buffers` above asserts is refused.
 *
 * **The front is the discriminator and the back is the control.** The front's destination blue
 * is 0, so the source's blue arrives there intact and the two arms expect different values. The
 * back's is 255, so it saturates in both arms and must read 255 either way - a back that reads
 * anything else is the `front-and-back` fault reproduced here.
 */
static int check_two_draw_buffers(void) {
    reset_view();
    const GLuint p = use_program(VS_PASSTHROUGH,
                                 "uniform vec4 c;\n"
                                 "void main() { gl_FragColor = c; }\n");
    if (!p) return 0;
    const GLint pos = glGetAttribLocation(p, "pos");
    const GLint kc = glGetUniformLocation(p, "c");
    if (pos < 0 || kc < 0) return 0;

    int ok = 1;
    for (int arm = 0; arm < 2; arm++) {
        /* 0.75 is 191 of 255, far from both destinations and from the drifting byte. */
        const float src_b = (arm == 0) ? 0.75f : 0.0f;
        const int want_b = (arm == 0) ? 191 : 0;
        const char *const was_name =
            (arm == 0) ? "two-draw-buffers/set-was" : "two-draw-buffers/zero-was";
        const char *const got_name =
            (arm == 0) ? "two-draw-buffers/set-got" : "two-draw-buffers/zero-got";

        /* **Each destination drawn on its own**, so they differ in every channel the blend
         * reads - and so the front's is a buffer this check put there rather than whatever the
         * last one left. Alpha stays 1.0 throughout: the blended draw contributes none. */
        glDisable(GL_BLEND);
        glDrawBuffer(GL_BACK);
        glUniform4f(kc, 0.0f, 0.0f, 1.0f, 1.0f);
        attrib_rect(pos, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
        glDrawBuffer(GL_FRONT);
        glUniform4f(kc, 1.0f, 0.0f, 0.0f, 1.0f);
        attrib_rect(pos, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
        glDrawBuffer(GL_BACK);

        /* Both destinations through the same reads the verdict uses, before the blended draw.
         * If these are not red and blue the check never measured a blend, and the rows below
         * say so rather than leaving it to be inferred. */
        const uint32_t was_front = centre_of(GL_FRONT);
        const uint32_t was_back = centre_of(GL_BACK);

        const GLenum both[2] = {GL_BACK, GL_FRONT};
        glDrawBuffers(2, both);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glUniform4f(kc, 0.25f, 0.5f, src_b, 0.0f);
        attrib_rect(pos, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
        glDisable(GL_BLEND);
        glDrawBuffer(GL_BACK);

        const uint32_t front = centre_of(GL_FRONT);
        const uint32_t back = centre_of(GL_BACK);

        /* GL row 47, which is the row the verdict's own scan calls the centre. */
        const uint32_t back_below = row_below_centre_of(GL_BACK);
        const uint32_t front_below = row_below_centre_of(GL_FRONT);

        if (gl2_probe_saw) {
            gl2_probe_saw(was_name, was_front, 0u, 0, was_back, 0u);
            gl2_probe_saw(got_name, front, 0u, 0, back, 0u);
            /* `saw` is the front one row down, `L` the back one row down: the same two reads the
             * row above made, moved by one, against a verdict row that samples exactly here. */
            gl2_probe_saw(arm == 0 ? "two-draw-buffers/set-row47"
                                   : "two-draw-buffers/zero-row47",
                          front_below, 0u, 0, back_below, 0u);
        }

        ok = ok && near_rgb(was_front, 255, 0, 0, 2) && near_rgb(was_back, 0, 0, 255, 2);
        /* 0.25 and 0.5 are 64 and 128; the front adds them to red, the back to blue. */
        ok = ok && near_rgb(front, 255, 128, want_b, 6);
        ok = ok && near_rgb(back, 64, 128, 255, 6);
    }

    /*
     * **The shape of the wrong blue**, from the one snapshot the check already takes.
     *
     * GL row 47 of the back holds blue 255 and GL row 48 holds blue 0, in both arms, after a draw
     * that covers the whole region uniformly. One pixel either side of a boundary says nothing
     * about what the boundary *is* - every other row, one row, a tile edge, half the region are
     * all consistent with two samples - and which it is decides whether this is rasterisation,
     * addressing, or the second export. So: count the region, and print where the two values sit.
     *
     * A scan row `y` is GL row `PROBE_H - 1 - y`, because `scan_frame` indexes the readback image
     * top-down from `g_row0` and `glGetFrameReadback` fills it with GL `height - 1 - row`. The
     * masks below are in **scan** rows and columns, so bit 0 of `blue-rows` is GL row 95.
     *
     * Counted in three buckets rather than two: a third value anywhere would mean this is not a
     * clean split and the row/column masks are not the right instrument.
     */
    blue_census("two-draw-buffers/two-count", "two-draw-buffers/two-rows",
                "two-draw-buffers/two-cols", "two-draw-buffers/two-cols3");
    uniformity_census("two-draw-buffers/two-u");

    /*
     * **The same blend into one target**, which is the control the pattern above demands.
     *
     * A quarter of the region holding the right blue, at exactly the pixels with both coordinates
     * even, is the fragment quad's shape. Nothing in this suite had ever counted a channel across
     * the region before, so "only under two targets" is an assumption, not a measurement: every
     * other check reads one pixel, and `scan_frame`'s centre is an even/even pixel - the one the
     * lattice gets right. A single-target blend with the same colours says which it is.
     *
     * If this census is also one-in-four, the two-target path is innocent and something far older
     * has been writing three quarters of every blended blue wrong, unseen because nobody looked
     * anywhere but the centre.
     */
    glDisable(GL_BLEND);
    glDrawBuffer(GL_BACK);
    glUniform4f(kc, 0.0f, 0.0f, 1.0f, 1.0f);
    attrib_rect(pos, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
    /* **No blending has happened in this frame yet.** One uniform draw of a constant colour, and
     * the region has to be that colour everywhere. A lattice here is the draw's, and nothing
     * below it needs investigating. */
    uniformity_census("two-draw-buffers/plain");

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glUniform4f(kc, 0.25f, 0.5f, 0.75f, 0.0f);
    attrib_rect(pos, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
    glDisable(GL_BLEND);
    blue_census("two-draw-buffers/one-count", "two-draw-buffers/one-rows",
                "two-draw-buffers/one-cols", "two-draw-buffers/one-cols3");
    uniformity_census("two-draw-buffers/one-u");

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
    {"texture-cube", check_texture_cube},
    {"texture-proj", check_texture_proj},
    {"texture-3d", check_texture_3d},
    {"shadow-compare", check_shadow_compare},

    /* The language */
    {"matrix-arithmetic", check_matrix_arithmetic},
    {"matrix-products", check_matrix_products},
    {"inverse-trig", check_inverse_trig},
    {"refract", check_refract},
    {"swizzles", check_swizzles},
    {"control-flow", check_control_flow},
    {"loop-divergence", check_loop_divergence},
    {"discard-in-loop", check_discard_inside_a_loop},
    {"user-functions", check_user_functions},
    {"early-return", check_early_return},
    {"local-arrays", check_local_arrays},
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
    {"two-draw-buffers", check_two_draw_buffers},
    {"blend-uniformity", check_blend_uniformity},
    {"point-coord", check_point_coord},
    {"program-introspection", check_program_introspection},
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
            /* **How much of the region is not the reset colour**, counted from the same
             * snapshot. One pixel cannot tell "nothing drew" from "the draw missed this
             * pixel", and those are different bugs. */
            int drawn = 0;
            for (int q = 0; q < PROBE_W * PROBE_H; q++) {
                if (g_scan[q] != PROBE_BG) drawn++;
            }
            gl2_probe_saw(g_cases[i].name, SCAN_PX(g_scan, MID_X, MID_Y), (unsigned int)err,
                          drawn, SCAN_PX(g_scan, MID_X - 24, MID_Y),
                          SCAN_PX(g_scan, MID_X + 24, MID_Y));
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
