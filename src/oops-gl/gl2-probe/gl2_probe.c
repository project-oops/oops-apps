/*
 * gl2-probe: OpenGL 2.0 measured against the specification, check by check.
 *
 * The programmable counterpart of gl1-probe: shader and program objects, generic vertex
 * attributes, and what a shader computes when it runs. Most checks end in a pixel,
 * because that is what can differ between the host reference and the console.
 *
 * Each check starts from `reset_view`, which restores state, drops the current program
 * and clears to a colour no check draws. One table runs on host and console, and every
 * sample goes through `frame()` so both paths are measured alike.
 */

#include "gl2_probe.h"

#include <GL/gl.h>
#include <oops/display.h>

#ifdef OOPS_HOST_BUILD
#include <string.h>
#else
#include <oops/freestd.h>
#include <oops/time.h> /* the throughput check is timed on the console only */
#endif

#include "probe_px.h"

static uint32_t *g_fb;
static oops_display_t *g_disp;
static unsigned int g_fb_w;
static unsigned int g_fb_h;
static unsigned int g_row0;

/* The program the current check built. The next `reset_view` deletes it, so a check
 * cannot draw with the one before it. */
static GLuint g_prog;

void (*gl2_probe_trace)(const char *name, int verdict);
void (*gl2_probe_saw)(const char *name, uint32_t centre, unsigned int err, int drawn,
                      uint32_t left, uint32_t right);

/* The probe's rectangle, synchronised once; the only way this file reads a pixel. Each
 * glFinish can move the target to the next scanout buffer, so a second read after a
 * scan would see a cleared buffer. */
static uint32_t g_scan[PROBE_W * PROBE_H];

static const uint32_t *scan_frame(void) {
    const uint32_t *f = probe_frame(g_fb);
    for (int i = 0; i < PROBE_W * PROBE_H; i++)
        g_scan[i] = 0u;
    probe_snapshot(f, g_row0, g_fb_w, g_scan);
    return g_scan;
}

static int near_chan(int got, int want, int tol) {
    const int d = got - want;
    return (d < 0 ? -d : d) <= tol;
}

/* Declared early because the blending checks above the definitions report them. */
static void uniformity_census_of(const uint32_t *s, const char *name);
static int census_wrong(const uint32_t *s, int r, int g, int b, int tol);

/* -------------------------------------------------------------------------
 * The state every check starts from
 * ------------------------------------------------------------------------- */

static void reset_view(void) {
    /* The program is dropped before the clear, so the clear does not run through the
     * last check's fragment shader. */
    glUseProgram(0);
    if (g_prog) {
        glDeleteProgram(g_prog);
        g_prog = 0u;
    }
    for (GLuint i = 0; i < 16u; i++)
        glDisableVertexAttribArray(i);

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
 * The shaders are deleted as soon as the program links, so every check relies on
 * deferred shader deletion.
 * ------------------------------------------------------------------------- */

static GLuint make_shader(GLenum type, const char *src) {
    const GLchar *strings[1];
    strings[0] = src;
    GLuint sh = glCreateShader(type);
    if (!sh)
        return 0u;
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

/* Links a program and makes it current, returning 0 on any failure. A NULL `fs` gives
 * a vertex-only program, and the fixed-function fragment stage runs. */
static GLuint use_program(const char *vs_src, const char *fs_src) {
    GLuint vs = make_shader(GL_VERTEX_SHADER, vs_src);
    if (!vs)
        return 0u;
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
    if (fs)
        glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    if (fs)
        glDeleteShader(fs);
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

static void mat_identity(float m[16]) {
    for (int i = 0; i < 16; i++)
        m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

/* -------------------------------------------------------------------------
 * Geometry the checks draw
 *
 * Immediate mode, with arrays where the check is about arrays. `glVertex` emits the
 * vertex; the shaders take position from a generic attribute, so the two are
 * independent.
 * ------------------------------------------------------------------------- */

/* A rectangle in NDC through generic slot `loc`, as two triangles. */
static void attrib_rect(GLint loc, float x0, float y0, float x1, float y1, float z) {
    if (loc < 0)
        return;
    const float xs[6] = {x0, x1, x1, x0, x1, x0};
    const float ys[6] = {y0, y0, y1, y0, y1, y1};
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 6; i++) {
        glVertexAttrib3f((GLuint)loc, xs[i], ys[i], z);
        glVertex3f(xs[i], ys[i], z);
    }
    glEnd();
}

/* The same, with a second attribute that varies per corner. */
static void attrib_rect2(GLint pos, GLint extra, float x0, float y0, float x1, float y1,
                         const float corner[4][4]) {
    if (pos < 0)
        return;
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

/* Position straight through, for every check that is not about the vertex stage. */
static const char *const VS_PASSTHROUGH =
    "attribute vec3 pos;\n"
    "void main() { gl_Position = vec4(pos, 1.0); }\n";

/* The middle of a full-region rectangle. */
#define MID_X (PROBE_W / 2)
#define MID_Y (PROBE_H / 2)

/* -------------------------------------------------------------------------
 * The object model
 * ------------------------------------------------------------------------- */

static int check_version_gating(void) {
    reset_view();
    /* A context has the entry points its version defines and no others: narrowing to
     * 1.5 removes the programmable surface, and widening to 2.0 restores it. */
    if (!glContextSetVersion(1, 5))
        return 0;
    (void)glGetError();

    /* Each answers the value it returns on failure, and each records
     * GL_INVALID_OPERATION. */
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

    /* An enumerant from a later version is GL_INVALID_ENUM, and the query leaves its
     * destination alone. */
    GLint v = 1234;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &v);
    ok = ok && glGetError() == GL_INVALID_ENUM && v == 1234;

    /* GL 1.x stays available: every 1.2-1.5 feature is also an ARB or EXT extension,
     * which is available whatever the core version. */
    GLuint buf = 0u;
    glGenBuffers(1, &buf);
    ok = ok && buf != 0u;
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glActiveTexture(GL_TEXTURE0);
    glSecondaryColor3f(1.0f, 0.0f, 0.0f);
    ok = ok && glGetError() == GL_NO_ERROR;
    glDeleteBuffers(1, &buf);

    /* Every later check needs 2.0. */
    ok = ok && glContextSetVersion(2, 0) == GL_TRUE;
    const GLuint sh = glCreateShader(GL_FRAGMENT_SHADER);
    ok = ok && sh != 0u;
    if (sh)
        glDeleteShader(sh);
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_name_space(void) {
    reset_view();
    const GLuint a = glCreateShader(GL_VERTEX_SHADER);
    const GLuint p = glCreateProgram();
    const GLuint b = glCreateShader(GL_FRAGMENT_SHADER);
    int ok = a && p && b && a != p && p != b && a != b;
    /* A shader call on a program name is GL_INVALID_OPERATION - the object exists and
     * is the wrong kind - where a name nothing owns is GL_INVALID_VALUE. */
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

    /* A type error the grammar accepts: `vec3 * mat4` has dimensions that do not
     * meet, so the compile fails with a log. */
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
                     "void main() { vtint = tint * scale; gl_Position = mvp * "
                     "vec4(pos, 1.0); }\n",
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
    glGetActiveUniform(g_prog, 0, (GLsizei)sizeof(name), (GLsizei *)0, &size, &type,
                       name);
    ok = ok && size == 1 && (type == GL_FLOAT_MAT4 || type == GL_FLOAT);

    const GLint mvp = glGetUniformLocation(g_prog, "mvp");
    const GLint scale = glGetUniformLocation(g_prog, "scale");
    ok = ok && mvp >= 0 && scale >= 0 && mvp != scale;
    /* A name nothing declares is -1 and not an error. */
    ok = ok && glGetUniformLocation(g_prog, "absent") == -1 &&
         glGetError() == GL_NO_ERROR;
    /* The language's built-in names are not program attributes. */
    ok = ok && glGetAttribLocation(g_prog, "gl_Vertex") == -1;
    return ok;
}

static int check_link_refuses(void) {
    reset_view();
    /* A varying the fragment shader reads and the vertex shader never writes fails to
     * link. */
    GLuint vs = make_shader(GL_VERTEX_SHADER, VS_PASSTHROUGH);
    GLuint fs = make_shader(GL_FRAGMENT_SHADER,
                            "varying vec3 missing;\n"
                            "void main() { gl_FragColor = vec4(missing, 1.0); }\n");
    if (!vs || !fs)
        return 0;
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
    /* A program that did not link cannot be made current. */
    (void)glGetError();
    glUseProgram(prog);
    ok = ok && glGetError() == GL_INVALID_OPERATION;

    /* A vertex shader that never writes gl_Position is undefined by the specification;
     * this implementation makes it a link error. */
    GLuint vs2 = make_shader(GL_VERTEX_SHADER, "attribute vec3 pos;\n"
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
    GLuint fs =
        make_shader(GL_FRAGMENT_SHADER, "void main() { gl_FragColor = vec4(1.0); }\n");
    if (!vs || !fs)
        return 0;
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

    /* A flagged shader: `glIsShader` says no and `glGetShaderiv` still answers. */
    int ok = (glIsShader(vs) == GL_FALSE);
    GLint flagged = -1;
    (void)glGetError();
    glGetShaderiv(vs, GL_DELETE_STATUS, &flagged);
    ok = ok && flagged == GL_TRUE && glGetError() == GL_NO_ERROR;
    /* The program still works. */
    glUseProgram(prog);
    ok = ok && glGetError() == GL_NO_ERROR;
    g_prog = prog;
    return ok;
}

static int check_uniform_type_match(void) {
    reset_view();
    if (!use_program(
            "uniform float scale;\n"
            "uniform int count;\n"
            "attribute vec3 pos;\n"
            "void main() { gl_Position = vec4(pos, 1.0) * scale * float(count); }\n",
            "void main() { gl_FragColor = vec4(1.0); }\n")) {
        return 0;
    }
    const GLint scale = glGetUniformLocation(g_prog, "scale");
    const GLint count = glGetUniformLocation(g_prog, "count");
    if (scale < 0 || count < 0)
        return 0;
    (void)glGetError();

    glUniform1f(scale, 2.0f);
    int ok = (glGetError() == GL_NO_ERROR);
    GLfloat got = 0.0f;
    glGetUniformfv(g_prog, scale, &got);
    ok = ok && got > 1.99f && got < 2.01f;

    /* The command matches the declared type: `glUniform1i` on a float is an error,
     * not a conversion. */
    glUniform1i(scale, 3);
    ok = ok && glGetError() == GL_INVALID_OPERATION;
    glUniform1f(count, 3.0f);
    ok = ok && glGetError() == GL_INVALID_OPERATION;
    glUniform1i(count, 3);
    ok = ok && glGetError() == GL_NO_ERROR;
    /* A width that does not match is the same kind of error. */
    glUniform2f(scale, 1.0f, 2.0f);
    ok = ok && glGetError() == GL_INVALID_OPERATION;
    /* A location of -1 is silently ignored. */
    glUniform1f(-1, 5.0f);
    ok = ok && glGetError() == GL_NO_ERROR;
    return ok;
}

static int check_uniform_arrays(void) {
    reset_view();
    if (!use_program(
            "uniform vec4 palette[3];\n"
            "uniform float after;\n"
            "attribute vec3 pos;\n"
            "void main() { gl_Position = vec4(pos, 1.0) * palette[2] * after; }\n",
            "void main() { gl_FragColor = vec4(1.0); }\n")) {
        return 0;
    }
    const GLint base = glGetUniformLocation(g_prog, "palette");
    if (base < 0)
        return 0;
    /* The unbracketed name is element zero, the bracketed forms are consecutive, and
     * past the end is -1 rather than a location into whatever comes next. */
    int ok = glGetUniformLocation(g_prog, "palette[0]") == base;
    ok = ok && glGetUniformLocation(g_prog, "palette[2]") == base + 2;
    ok = ok && glGetUniformLocation(g_prog, "palette[3]") == -1;
    /* Locations are per element, so the uniform after the array does not collide with
     * its elements. */
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
    GLuint vs = make_shader(
        GL_VERTEX_SHADER, "attribute vec3 pos;\n"
                          "attribute vec3 tint;\n"
                          "varying vec3 v;\n"
                          "void main() { v = tint; gl_Position = vec4(pos, 1.0); }\n");
    GLuint fs = make_shader(GL_FRAGMENT_SHADER,
                            "varying vec3 v;\n"
                            "void main() { gl_FragColor = vec4(v, 1.0); }\n");
    if (!vs || !fs)
        return 0;
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

    /* A binding after the link takes effect at the next link. */
    glBindAttribLocation(prog, 7, "pos");
    ok = ok && glGetAttribLocation(prog, "pos") == 5;
    glLinkProgram(prog);
    ok = ok && glGetAttribLocation(prog, "pos") == 7;

    /* The reserved `gl_` prefix cannot be bound. */
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
    int ok = (v >= 16); /* GL 2.0's own minimum */
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

    /* GL_SHADING_LANGUAGE_VERSION reports the highest dialect the front end takes, a
     * ceiling rather than a mode: `#version 110` keeps 1.10 rules. */
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
    if (!use_program(VS_PASSTHROUGH, "void main() { gl_FragColor = vec4(1.0); }\n"))
        return 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &cur);
    ok = ok && cur == (GLint)g_prog;
    /* glUseProgram(0) returns to the fixed-function pipeline. */
    glUseProgram(0);
    glGetIntegerv(GL_CURRENT_PROGRAM, &cur);
    return ok && cur == 0;
}

/* -------------------------------------------------------------------------
 * The vertex stage
 * ------------------------------------------------------------------------- */

static int check_program_draws(void) {
    reset_view();
    const GLuint p = use_program(
        VS_PASSTHROUGH, "void main() { gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0); }\n");
    if (!p)
        return 0;
    const GLint loc = glGetAttribLocation(p, "pos");
    /* Not square and not centred, so a transposed or mirrored rectangle fails. */
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
    const GLuint p =
        use_program("uniform mat4 mvp;\n"
                    "attribute vec3 pos;\n"
                    "void main() { gl_Position = mvp * vec4(pos, 1.0); }\n",
                    "void main() { gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n");
    if (!p)
        return 0;
    const GLint loc = glGetAttribLocation(p, "pos");
    const GLint mvp = glGetUniformLocation(p, "mvp");

    /* Column-major: element 12 is the x translation. A transposed read moves the
     * rectangle in y instead, so the rectangle is off-centre. */
    float m[16];
    mat_identity(m);
    m[0] = 0.25f;
    m[5] = 0.25f;
    m[12] = 0.5f;
    glUniformMatrix4fv(mvp, 1, GL_FALSE, m);
    attrib_rect(loc, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);

    const uint32_t *s = scan_frame();
    /* Quarter size, shifted right by 0.5: NDC x 0.25 to 0.75, pixels 80 to 112. */
    int ok = near_rgb(SCAN_PX(s, MID_X + 24, MID_Y), 255, 0, 0, 2);
    ok = ok && SCAN_PX(s, MID_X - 24, MID_Y) == PROBE_BG;

    /* With `transpose` the 0.5 moves into the row that computes w, so the rectangle is
     * projected instead of shifted and leaves the sampled pixel. */
    glClear(GL_COLOR_BUFFER_BIT);
    glUniformMatrix4fv(mvp, 1, GL_TRUE, m);
    attrib_rect(loc, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
    s = scan_frame();
    ok = ok && SCAN_PX(s, MID_X + 24, MID_Y) == PROBE_BG;
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_attribute_arrays(void) {
    reset_view();
    const GLuint p =
        use_program("attribute vec3 pos;\n"
                    "attribute vec4 tint;\n"
                    "varying vec4 v;\n"
                    "void main() { v = tint; gl_Position = vec4(pos, 1.0); }\n",
                    "varying vec4 v;\n"
                    "void main() { gl_FragColor = v; }\n");
    if (!p)
        return 0;
    const GLint pos = glGetAttribLocation(p, "pos");
    const GLint tint = glGetAttribLocation(p, "tint");
    if (pos < 0 || tint < 0)
        return 0;

    /* Each vertex fetches its own array element, so the three corners keep three
     * distinct colours. */
    static const GLfloat verts[9] = {-0.9f, -0.9f, 0.0f, 0.9f, -0.9f,
                                     0.0f,  0.0f,  0.9f, 0.0f};
    static const GLubyte cols[12] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255};
    glVertexAttribPointer((GLuint)pos, 3, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray((GLuint)pos);
    /* Normalised, so 255 is 1.0 and not 255.0. */
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
    /* The middle mixes all three; an unnormalised read saturates to white. */
    const uint32_t mid = SCAN_PX(s, MID_X, MID_Y + 6);
    ok = ok && chan_r(mid) > 16 && chan_r(mid) < 220;
    ok = ok && chan_g(mid) > 16 && chan_g(mid) < 220;
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_attribute_current_value(void) {
    reset_view();
    const GLuint p =
        use_program("attribute vec3 pos;\n"
                    "attribute vec4 tint;\n"
                    "varying vec4 v;\n"
                    "void main() { v = tint; gl_Position = vec4(pos, 1.0); }\n",
                    "varying vec4 v;\n"
                    "void main() { gl_FragColor = v; }\n");
    if (!p)
        return 0;
    const GLint pos = glGetAttribLocation(p, "pos");
    const GLint tint = glGetAttribLocation(p, "tint");
    if (pos < 0 || tint < 0)
        return 0;

    /* With the array disabled every vertex reads the current value. A 2f fills z with
     * 0 and w with 1 rather than keeping what a previous 4f wrote. */
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
    /* A 1.10 shader may read the fixed-function attributes and the matrix stack. */
    const GLuint p =
        use_program("void main() {\n"
                    "  gl_FrontColor = gl_Color;\n"
                    "  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
                    "}\n",
                    "void main() { gl_FragColor = gl_Color; }\n");
    if (!p)
        return 0;

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
    /* Half-size from the scale: the middle is blue and the corner is not. */
    int ok = near_rgb(SCAN_PX(s, MID_X, MID_Y), 0, 0, 255, 2);
    ok = ok && SCAN_PX(s, 4, 4) == PROBE_BG;
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_vertex_only_program(void) {
    reset_view();
    /* A program may have one stage: with only a vertex shader, the fixed-function
     * fragment stage runs and reads `gl_FrontColor`. */
    const GLuint p = use_program("attribute vec3 pos;\n"
                                 "void main() {\n"
                                 "  gl_FrontColor = vec4(1.0, 0.0, 1.0, 1.0);\n"
                                 "  gl_Position = vec4(pos, 1.0);\n"
                                 "}\n",
                                 (const char *)0);
    if (!p)
        return 0;
    const GLint loc = glGetAttribLocation(p, "pos");
    attrib_rect(loc, -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    return near_rgb(SCAN_PX(s, MID_X, MID_Y), 255, 0, 255, 2) &&
           glGetError() == GL_NO_ERROR;
}

/* -------------------------------------------------------------------------
 * Varyings
 * ------------------------------------------------------------------------- */

static int check_varying_interpolates(void) {
    reset_view();
    const GLuint p =
        use_program("attribute vec3 pos;\n"
                    "attribute vec4 tint;\n"
                    "varying vec4 v;\n"
                    "void main() { v = tint; gl_Position = vec4(pos, 1.0); }\n",
                    "varying vec4 v;\n"
                    "void main() { gl_FragColor = v; }\n");
    if (!p)
        return 0;
    const GLint pos = glGetAttribLocation(p, "pos");
    const GLint tint = glGetAttribLocation(p, "tint");
    /* Black on the left edge, red on the right. */
    static const float corners[4][4] = {{0.0f, 0.0f, 0.0f, 1.0f},
                                        {1.0f, 0.0f, 0.0f, 1.0f},
                                        {1.0f, 0.0f, 0.0f, 1.0f},
                                        {0.0f, 0.0f, 0.0f, 1.0f}};
    attrib_rect2(pos, tint, -1.0f, -0.8f, 1.0f, 0.8f, corners);

    const uint32_t *s = scan_frame();
    const int left = chan_r(SCAN_PX(s, 16, MID_Y));
    const int mid = chan_r(SCAN_PX(s, MID_X, MID_Y));
    const int right = chan_r(SCAN_PX(s, PROBE_W - 17, MID_Y));
    int ok = left < mid && mid < right;
    /* Every w is 1, so the middle is the arithmetic middle. */
    ok = ok && near_chan(mid, 128, 12);
    ok = ok && chan_g(SCAN_PX(s, MID_X, MID_Y)) < 4;
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_varying_perspective_correct(void) {
    reset_view();
    /* Varyings interpolate perspective-correctly. The quad has w = 1 on its left edge
     * and w = 3 on its right (carried in `pos.z`), so the screen midpoint is 0.25, not
     * the affine 0.5. */
    const GLuint p =
        use_program("attribute vec3 pos;\n"
                    "attribute vec4 tint;\n"
                    "varying vec4 v;\n"
                    "void main() {\n"
                    "  v = tint;\n"
                    "  gl_Position = vec4(pos.x * pos.z, pos.y * pos.z, 0.0, pos.z);\n"
                    "}\n",
                    "varying vec4 v;\n"
                    "void main() { gl_FragColor = v; }\n");
    if (!p)
        return 0;
    const GLint pos = glGetAttribLocation(p, "pos");
    const GLint tint = glGetAttribLocation(p, "tint");
    if (pos < 0 || tint < 0)
        return 0;

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
    /* 0.25 is 64 of 255; an affine interpolator gives 128. */
    return near_chan(mid, 64, 14) && glGetError() == GL_NO_ERROR;
}

static int check_several_varyings(void) {
    reset_view();
    /* Four varyings of different widths get distinct slots in the linker's packing. */
    const GLuint p =
        use_program("attribute vec3 pos;\n"
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
    if (!p)
        return 0;
    const GLint loc = glGetAttribLocation(p, "pos");
    attrib_rect(loc, -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    /* (0.25, 0.5, 0.75) is (64, 128, 191). */
    return near_rgb(SCAN_PX(s, MID_X, MID_Y), 64, 128, 191, 3) &&
           glGetError() == GL_NO_ERROR;
}

/* -------------------------------------------------------------------------
 * The fragment stage
 * ------------------------------------------------------------------------- */

static int check_frag_coord(void) {
    reset_view();
    /* `gl_FragCoord.y` counts up from the bottom, opposite to image rows, and its x and
     * y are pixel centres. */
    const GLuint p = use_program(
        VS_PASSTHROUGH, "void main() {\n"
                        "  gl_FragColor = vec4(gl_FragCoord.y / 96.0, 0.0, 0.0, 1.0);\n"
                        "}\n");
    if (!p)
        return 0;
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
    /* Discards the left half. A discarded fragment writes no depth, so the farther
     * second draw still appears there. */
    const GLuint p =
        use_program("attribute vec3 pos;\n"
                    "varying float side;\n"
                    "void main() { side = pos.x; gl_Position = vec4(pos, 1.0); }\n",
                    "varying float side;\n"
                    "void main() {\n"
                    "  if (side < 0.0) discard;\n"
                    "  gl_FragColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
                    "}\n");
    if (!p)
        return 0;
    attrib_rect(glGetAttribLocation(p, "pos"), -0.9f, -0.9f, 0.9f, 0.9f, -0.5f);

    const uint32_t *s = scan_frame();
    int ok = near_rgb(SCAN_PX(s, MID_X + 24, MID_Y), 0, 0, 255, 2);
    ok = ok && SCAN_PX(s, MID_X - 24, MID_Y) == PROBE_BG;

    const GLuint p2 = use_program(
        VS_PASSTHROUGH, "void main() { gl_FragColor = vec4(1.0, 1.0, 0.0, 1.0); }\n");
    if (!p2)
        return 0;
    attrib_rect(glGetAttribLocation(p2, "pos"), -0.9f, -0.9f, 0.9f, 0.9f, 0.5f);

    s = scan_frame();
    ok = ok && near_rgb(SCAN_PX(s, MID_X + 24, MID_Y), 0, 0, 255, 2); /* still blue */
    ok = ok && near_rgb(SCAN_PX(s, MID_X - 24, MID_Y), 255, 255, 0,
                        2); /* the depth was free */
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_frag_depth(void) {
    reset_view();
    glEnable(GL_DEPTH_TEST);
    /* The depth test uses the `gl_FragDepth` the shader wrote, not the interpolated
     * value: the near quad pushes itself back, so the quad drawn behind it wins. */
    const GLuint p =
        use_program(VS_PASSTHROUGH, "void main() {\n"
                                    "  gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
                                    "  gl_FragDepth = 0.99;\n"
                                    "}\n");
    if (!p)
        return 0;
    attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, -0.9f);

    const uint32_t *s = scan_frame();
    int ok = near_rgb(SCAN_PX(s, MID_X, MID_Y), 255, 0, 0, 2);

    const GLuint p2 = use_program(
        VS_PASSTHROUGH, "void main() { gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0); }\n");
    if (!p2)
        return 0;
    attrib_rect(glGetAttribLocation(p2, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    s = scan_frame();
    /* z = 0 maps to depth 0.5, which is in front of the 0.99 the first shader wrote. */
    ok = ok && near_rgb(SCAN_PX(s, MID_X, MID_Y), 0, 255, 0, 2);
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_front_facing(void) {
    reset_view();
    /* `gl_FrontFacing` is false for a polygon wound away from the viewer. Culling is
     * off, so both are drawn and the two halves differ. */
    const GLuint p = use_program(
        VS_PASSTHROUGH, "void main() {\n"
                        "  gl_FragColor = gl_FrontFacing ? vec4(0.0, 1.0, 0.0, 1.0)\n"
                        "                                : vec4(1.0, 0.0, 0.0, 1.0);\n"
                        "}\n");
    if (!p)
        return 0;
    const GLint loc = glGetAttribLocation(p, "pos");
    if (loc < 0)
        return 0;

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
    /* One green and one red, whichever way the winding convention lands. */
    const int lg = near_rgb(left, 0, 255, 0, 2), lr = near_rgb(left, 255, 0, 0, 2);
    const int rg = near_rgb(right, 0, 255, 0, 2), rr = near_rgb(right, 255, 0, 0, 2);
    return ((lg && rr) || (lr && rg)) && glGetError() == GL_NO_ERROR;
}

static int check_fragment_only_program(void) {
    reset_view();
    /* No vertex shader: the fixed-function transform runs and the fragment shader reads
     * its output through `gl_Color`. */
    GLuint fs =
        make_shader(GL_FRAGMENT_SHADER,
                    "void main() { gl_FragColor = vec4(gl_Color.rgb * 0.5, 1.0); }\n");
    if (!fs)
        return 0;
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
    return near_rgb(SCAN_PX(s, MID_X, MID_Y), 128, 64, 0, 4) &&
           glGetError() == GL_NO_ERROR;
}

/* -------------------------------------------------------------------------
 * Texturing
 * ------------------------------------------------------------------------- */
/* A 2x2 texture of one colour on the given unit, unfiltered and not enabled - a
 * sampler's declared type names its target, so shaders need no `glEnable`. */
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

/* The GLSL dialect libultraship's `gfx_opengl.cpp` emits: 1.20 constructs under a
 * `#version 130` line, which the libultraship ports patch to `#version 120`. The shader
 * has Fast3D's shape - two units with different texels, a non-white colour varying and
 * a multiply-and-clamp - and `#version 130` is refused, which the patch relies on. */
static int check_libultraship_dialect(void) {
    reset_view();
    /* Unit 0 white, unit 1 half-green and quarter-blue. The literal is 0xAABBGGRR. */
    GLuint t0 = make_flat_texture(GL_TEXTURE0, 0xffffffffu); /* 1.0, 1.0, 1.0  */
    GLuint t1 = make_flat_texture(GL_TEXTURE1, 0xff4080ffu); /* 1.0, 0.5, 0.25 */
    /* `make_flat_texture` leaves the last unit it touched selected. */
    glActiveTexture(GL_TEXTURE0);

    const GLuint p = use_program(
        "#version 120\n"
        "attribute vec3 pos;\n"
        "attribute vec4 aInput1;\n"
        "varying vec2 vTexCoord0;\n"
        "varying vec2 vTexCoord1;\n"
        "varying vec4 vInput1;\n"
        "void main() {\n"
        "  vTexCoord0 = pos.xy * 0.5 + vec2(0.5);\n"
        "  vTexCoord1 = vTexCoord0;\n"
        "  vInput1 = aInput1;\n"
        "  gl_Position = vec4(pos, 1.0);\n"
        "}\n",
        "#version 120\n"
        "uniform sampler2D uTex0;\n"
        "uniform sampler2D uTex1;\n"
        "varying vec2 vTexCoord0;\n"
        "varying vec2 vTexCoord1;\n"
        "varying vec4 vInput1;\n"
        "void main() {\n"
        "  vec4 texel0 = texture2D(uTex0, vTexCoord0);\n"
        "  vec4 texel1 = texture2D(uTex1, vTexCoord1);\n"
        "  vec3 c = clamp(texel0.rgb * texel1.rgb * vInput1.rgb, 0.0, 1.0);\n"
        "  gl_FragColor = vec4(c, texel0.a);\n"
        "}\n");
    int ok = (p != 0u);
    if (ok) {
        /* Both samplers named, so a program that reads unit 0 twice does not pass. */
        glUniform1i(glGetUniformLocation(p, "uTex0"), 0);
        glUniform1i(glGetUniformLocation(p, "uTex1"), 1);

        /* Colour input (1.0, 1.0, 0.5): blue is the product of three different
         * numbers, and no pair alone gives it. */
        static const float corner[4][4] = {
            {1.0f, 1.0f, 0.5f, 1.0f},
            {1.0f, 1.0f, 0.5f, 1.0f},
            {1.0f, 1.0f, 0.5f, 1.0f},
            {1.0f, 1.0f, 0.5f, 1.0f},
        };
        attrib_rect2(glGetAttribLocation(p, "pos"), glGetAttribLocation(p, "aInput1"),
                     -0.8f, -0.8f, 0.8f, 0.8f, corner);

        const uint32_t *s = scan_frame();
        /* R = 1.0  * 1.0  * 1.0 = 1.000 -> 255
         * G = 1.0  * 0.5  * 1.0 = 0.500 -> 128
         * B = 1.0  * 0.25 * 0.5 = 0.125 ->  32 */
        ok = near_rgb(SCAN_PX(s, MID_X, MID_Y), 255, 128, 32, 4);
    }

    /* `#version 130` is refused. The preprocessor's `do_version` only records the
     * number; the refusal comes later in the compiler. */
    GLuint v130 = make_shader(GL_FRAGMENT_SHADER,
                              "#version 130\n"
                              "varying vec2 uv;\n"
                              "void main() { gl_FragColor = vec4(uv, 0.0, 1.0); }\n");
    ok = ok && v130 == 0u;

    glDeleteTextures(1, &t0);
    glDeleteTextures(1, &t1);
    glActiveTexture(GL_TEXTURE0);
    return ok && glGetError() == GL_NO_ERROR;
}
static int check_texture_sampler(void) {
    reset_view();
    GLuint tex = make_flat_texture(GL_TEXTURE0, 0xff00ff00u); /* R=0 G=255 B=0 A=255 */
    glActiveTexture(GL_TEXTURE0);

    const GLuint p =
        use_program("attribute vec3 pos;\n"
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
    /* A sampler never set reads unit 0, the value a freshly linked program holds. */
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
        /* A sampler takes the integer form only. */
        (void)glGetError();
        glUniform1f(a, 1.0f);
        ok = ok && glGetError() == GL_INVALID_OPERATION;
        glUniform1i(a, 0);
        glUniform1i(b, 1);
        ok = ok && glGetError() == GL_NO_ERROR;
        attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
        const uint32_t *s = scan_frame();
        ok = ok && near_rgb(SCAN_PX(s, MID_X, MID_Y), 128, 0, 128, 4);

        /* Both on unit 1 is all blue: `glUniform1i` chooses the unit, not the
         * declaration order. */
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
    /* `dFdx` of a varying that runs 0..1 across a known width is its slope per pixel;
     * a zero derivative gives black. */
    const GLuint p = use_program(
        "attribute vec3 pos;\n"
        "varying float v;\n"
        "void main() { v = pos.x * 0.5 + 0.5; gl_Position = vec4(pos, 1.0); }\n",
        "varying float v;\n"
        "void main() { gl_FragColor = vec4(dFdx(v) * 128.0, 0.0, 0.0, 1.0); }\n");
    if (!p)
        return 0;
    attrib_rect(glGetAttribLocation(p, "pos"), -1.0f, -0.8f, 1.0f, 0.8f, 0.0f);

    const uint32_t *s = scan_frame();
    const int a = chan_r(SCAN_PX(s, MID_X - 20, MID_Y));
    const int b = chan_r(SCAN_PX(s, MID_X + 20, MID_Y));
    /* v runs 0..1 over 128 pixels, so dFdx(v) * 128 is 1.0 across the quad. */
    int ok = a > 200 && b > 200 && a == b;
    return ok && glGetError() == GL_NO_ERROR;
}

/* -------------------------------------------------------------------------
 * The language
 * ------------------------------------------------------------------------- */

/* Draws a whole fragment shader over the region and compares its one colour. */
static int language_check_src(const char *fs_src, int r, int g, int b, int tol) {
    reset_view();
    const GLuint p = use_program(VS_PASSTHROUGH, fs_src);
    if (!p)
        return 0;
    attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    return near_rgb(SCAN_PX(s, MID_X, MID_Y), r, g, b, tol) &&
           glGetError() == GL_NO_ERROR;
}

/* The same for a body wrapped in `main`. GLSL has no nested function definitions, so a
 * check that declares a function writes its own source. */
static int language_check(const char *body, int r, int g, int b, int tol) {
    char src[1024];
    int at = 0;
    const char *head = "void main() {\n";
    for (int i = 0; head[i]; i++)
        src[at++] = head[i];
    for (int i = 0; body[i] && at < (int)sizeof(src) - 8; i++)
        src[at++] = body[i];
    const char *tail = "\n}\n";
    for (int i = 0; tail[i]; i++)
        src[at++] = tail[i];
    src[at] = '\0';
    return language_check_src(src, r, g, b, tol);
}

static int check_matrix_arithmetic(void) {
    /* `vec * mat` is the transpose's product, not `mat * vec`; one off-diagonal term
     * separates them. */
    return language_check(
        "  mat2 m = mat2(1.0, 2.0, 0.0, 1.0);\n" /* column-major: col0 = (1,2), col1 =
                                                    (0,1) */
        "  vec2 a = m * vec2(1.0, 0.0);\n"       /* = col0 = (1, 2) */
        "  vec2 b = vec2(1.0, 0.0) * m;\n" /* = (dot(v,col0), dot(v,col1)) = (1, 0) */
        "  gl_FragColor = vec4(a.y * 0.25, b.y, 0.0, 1.0);",
        128, 0, 0, 3);
}

static int check_swizzles(void) {
    /* Swizzle reads and writes, out of order too: `v.zx = ...` writes z then x. */
    return language_check("  vec4 v = vec4(0.0, 0.0, 0.0, 1.0);\n"
                          "  v.zx = vec2(1.0, 0.25);\n"
                          "  vec3 c = v.xzy;\n"
                          "  gl_FragColor = vec4(c, 1.0);",
                          64, 255, 0, 3);
}

static int check_control_flow(void) {
    /* `continue` still runs the increment, and `break` stops at 5: 1+2+3+4+5 is 15,
     * and 15 * 0.05 is 0.75, which is 191. */
    return language_check("  float total = 0.0;\n"
                          "  for (int i = 1; i < 100; i++) {\n"
                          "    if (i > 5) break;\n"
                          "    if (i == 3) { total += float(i); continue; }\n"
                          "    total += float(i);\n"
                          "  }\n"
                          "  gl_FragColor = vec4(total * 0.05, 0.0, 0.0, 1.0);",
                          191, 0, 0, 3);
}

static int check_user_functions(void) {
    /* A value-returning function and one with `inout` parameters, which are copied in
     * and back out rather than passed by reference. */
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
    /* genType overloads, scalar-second and three-argument forms, with values chosen so
     * a wrong answer is a different colour rather than a near one. */
    return language_check("  float a = clamp(2.0, 0.0, 0.5);\n"         /* 0.5 */
                          "  float b = mix(0.0, 1.0, 0.25);\n"          /* 0.25 */
                          "  float c = smoothstep(0.0, 1.0, 0.5);\n"    /* 0.5 */
                          "  float d = length(vec2(3.0, 4.0)) * 0.2;\n" /* 1.0 */
                          "  gl_FragColor = vec4(a * d, b, c * d, 1.0);",
                          128, 64, 128, 3);
}

static int check_mod_is_floored(void) {
    /* GLSL's `mod` is floored, not C's truncated `fmod`: `mod(-1.0, 4.0)` is 3.
     * Integer division truncates as in C: 7 / 2 is 3. */
    return language_check("  float m = mod(-1.0, 4.0) * 0.25;\n"
                          "  int q = 7 / 2;\n"
                          "  gl_FragColor = vec4(m, float(q) * 0.25, 0.0, 1.0);",
                          191, 191, 0, 3);
}

static int check_relational_builtins(void) {
    /* `lessThan` gives a bvec, `any` and `all` reduce one, and `not` inverts it; each
     * channel is a different reduction of the same comparison. */
    return language_check(
        "  bvec3 c = lessThan(vec3(0.0, 1.0, 2.0), vec3(1.0, 1.0, 1.0));\n"
        "  float a = any(c) ? 1.0 : 0.0;\n"
        "  float b = all(c) ? 1.0 : 0.0;\n"
        "  float d = all(not(c)) ? 1.0 : 0.0;\n"
        "  gl_FragColor = vec4(a, b, d, 1.0);",
        255, 0, 0, 3);
}

static int check_short_circuit(void) {
    /* `&&` and `||` skip the right operand when the left decides (GLSL 1.10, 5.9). The
     * right operand writes an `out` parameter, so evaluating it changes the colour. */
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

/* A loop whose trip count differs per fragment, so `break` must leave only its own
 * lane of the wave. A `break` that leaves the whole wave flattens the gradient partway,
 * so left, middle and right must be strictly increasing. */
static int check_loop_divergence(void) {
    reset_view();
    const GLuint p =
        use_program("attribute vec3 pos;\n"
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
    if (!p)
        return 0;
    const GLint pos = glGetAttribLocation(p, "pos");
    const GLint tint = glGetAttribLocation(p, "tint");
    /* 0 on the left edge, 1 on the right: the trip count runs 1 to 33. */
    static const float corners[4][4] = {{0.0f, 0.0f, 0.0f, 1.0f},
                                        {1.0f, 0.0f, 0.0f, 1.0f},
                                        {1.0f, 0.0f, 0.0f, 1.0f},
                                        {0.0f, 0.0f, 0.0f, 1.0f}};
    attrib_rect2(pos, tint, -1.0f, -0.8f, 1.0f, 0.8f, corners);

    const uint32_t *s = scan_frame();
    const int left = chan_r(SCAN_PX(s, 16, MID_Y));
    const int mid = chan_r(SCAN_PX(s, MID_X, MID_Y));
    const int right = chan_r(SCAN_PX(s, PROBE_W - 17, MID_Y));
    /* About 1, 17 and 33 trips, times 0.03. The bands are wide because the column
     * decides the trip the break lands on; the ordering is strict. */
    int ok = left < mid && mid < right;
    ok = ok && left < 48 && mid > 96 && mid < 160 && right > 208;
    /* Nothing leaks into the other channels. */
    ok = ok && chan_g(SCAN_PX(s, MID_X, MID_Y)) < 4;
    return ok && glGetError() == GL_NO_ERROR;
}

/* A `discard` inside a loop, taken by the right half only. The loop reloads `exec` from
 * its active mask each trip, so a discarded lane must leave that mask too. The trip
 * count is past the unroller's limit so the loop branches. */
static int check_discard_inside_a_loop(void) {
    reset_view();
    const GLuint p =
        use_program("attribute vec3 pos;\n"
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
    if (!p)
        return 0;
    const GLint pos = glGetAttribLocation(p, "pos");
    const GLint tint = glGetAttribLocation(p, "tint");
    static const float corners[4][4] = {{0.0f, 0.0f, 0.0f, 1.0f},
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

/* An early `return` taken by the left half only; returning lanes rejoin the caller.
 * Green is written after the call, so a lane whose mask is not restored loses it. */
static int check_early_return(void) {
    reset_view();
    const GLuint p =
        use_program("attribute vec3 pos;\n"
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
    if (!p)
        return 0;
    const GLint pos = glGetAttribLocation(p, "pos");
    const GLint tint = glGetAttribLocation(p, "tint");
    static const float corners[4][4] = {{0.0f, 0.0f, 0.0f, 1.0f},
                                        {1.0f, 0.0f, 0.0f, 1.0f},
                                        {1.0f, 0.0f, 0.0f, 1.0f},
                                        {0.0f, 0.0f, 0.0f, 1.0f}};
    attrib_rect2(pos, tint, -1.0f, -0.8f, 1.0f, 0.8f, corners);

    const uint32_t *s = scan_frame();
    /* Left returned early (0.25), right ran on (1.0), and both keep the green. */
    int ok = near_rgb(SCAN_PX(s, 16, MID_Y), 64, 255, 0, 6);
    ok = ok && near_rgb(SCAN_PX(s, PROBE_W - 17, MID_Y), 255, 255, 0, 6);
    return ok && glGetError() == GL_NO_ERROR;
}

/* One `struct` arm. A struct is a run of registers on the console and a float array on
 * the host, both laid out by the semantic pass. Each arm puts a different member in a
 * different channel, so an off-by-one layout comes back rotated. The region is
 * censused, not sampled, because a lane fault can be banded and miss the middle. */
static int struct_arm_vs(const char *vs_src, const char *fs_src, const char *name,
                         int r, int g, int b) {
    reset_view();
    const GLuint p = use_program(vs_src, fs_src);
    if (!p) {
        if (gl2_probe_saw)
            gl2_probe_saw(name, 0u, 0xffffu, 0, 0u, 0u);
        return 0;
    }
    attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    const uint32_t *const s = scan_frame();
    const int wrong = census_wrong(s, r, g, b, 6);
    if (gl2_probe_saw) {
        /* `saw` the middle pixel, `drawn` the census misses, `L` the wanted colour. */
        gl2_probe_saw(
            name, SCAN_PX(s, MID_X, MID_Y), 0u, wrong,
            0xff000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b, 0u);
    }
    return wrong == 0 && glGetError() == GL_NO_ERROR;
}

/* A struct arm with the passthrough vertex shader. */
static int struct_arm(const char *fs_src, const char *name, int r, int g, int b) {
    return struct_arm_vs(VS_PASSTHROUGH, fs_src, name, r, g, b);
}

/* -------------------------------------------------------------------------
 * Framebuffer objects
 *
 * These arms read the attachment through `glReadPixels`, not the display. The
 * attachment is `PROBE_W` by `PROBE_H`, so `census_wrong` covers the same box. On the
 * console they measure `CB_COLOR0_BASE` pointed at an attachment in Garlic; the
 * renderbuffer and texture arms are separate so each is reported on its own.
 * ------------------------------------------------------------------------- */

static uint32_t g_fbo_read[PROBE_W * PROBE_H];

/* The bound framebuffer's colour attachment, packed as the 0xAARRGGBB words
 * `census_wrong` reads. */
static const uint32_t *fbo_read_attachment(void) {
    static uint8_t bytes[PROBE_W * PROBE_H * 4];
    for (int i = 0; i < PROBE_W * PROBE_H; i++)
        g_fbo_read[i] = 0u;
    glReadPixels(0, 0, PROBE_W, PROBE_H, GL_RGBA, GL_UNSIGNED_BYTE, bytes);
    for (int i = 0; i < PROBE_W * PROBE_H; i++) {
        g_fbo_read[i] = 0xff000000u | ((uint32_t)bytes[i * 4 + 0] << 16) |
                        ((uint32_t)bytes[i * 4 + 1] << 8) | (uint32_t)bytes[i * 4 + 2];
    }
    return g_fbo_read;
}

/* One framebuffer-object arm; `use_texture` picks a texture or renderbuffer colour
 * attachment. */
static int fbo_arm(int use_texture, const char *name, int r, int g, int b) {
    reset_view();

    GLuint fbo = 0u, rbo = 0u, tex = 0u;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    if (use_texture) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, PROBE_W, PROBE_H, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, (const GLvoid *)0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex,
                               0);
    } else {
        glGenRenderbuffers(1, &rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, PROBE_W, PROBE_H);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  rbo);
    }

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        /* `saw` carries the incomplete status. */
        if (gl2_probe_saw)
            gl2_probe_saw(name, (uint32_t)status, 0xfb01u, -1, 0u, 0u);
        glBindFramebuffer(GL_FRAMEBUFFER, 0u);
        glDeleteFramebuffers(1, &fbo);
        if (rbo)
            glDeleteRenderbuffers(1, &rbo);
        if (tex)
            glDeleteTextures(1, &tex);
        return 0;
    }

    glViewport(0, 0, PROBE_W, PROBE_H);
    const GLuint p = use_program(
        VS_PASSTHROUGH, "void main() { gl_FragColor = vec4(0.25, 0.5, 0.75, 1.0); }\n");
    int wrong = -1;
    uint32_t mid = 0u;
    if (p) {
        attrib_rect(glGetAttribLocation(p, "pos"), -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
        const uint32_t *const s = fbo_read_attachment();
        wrong = census_wrong(s, r, g, b, 6);
        mid = SCAN_PX(s, MID_X, MID_Y);
    }

    /* The display still holds the `reset_view` clear; read after unbinding, which
     * restores the display as the read target. */
    glBindFramebuffer(GL_FRAMEBUFFER, 0u);
    const uint32_t *const disp = scan_frame();
    const int display_wrong = census_wrong(disp, 0x20, 0x20, 0x20, 6);

    if (gl2_probe_saw) {
        gl2_probe_saw(name, mid, 0u, wrong,
                      0xff000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) |
                          (uint32_t)b,
                      (uint32_t)display_wrong);
    }
    glDeleteFramebuffers(1, &fbo);
    if (rbo)
        glDeleteRenderbuffers(1, &rbo);
    if (tex)
        glDeleteTextures(1, &tex);
    return wrong == 0 && display_wrong == 0 && glGetError() == GL_NO_ERROR;
}

static int check_framebuffer_objects(void) {
    /* Completeness rules: an empty framebuffer and one whose attachment has no storage
     * are incomplete with different statuses. */
    GLuint fbo = 0u, rbo = 0u;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    const GLenum empty = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                              rbo);
    const GLenum no_storage = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0u);
    const GLenum window = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rbo);

    const int rules_ok = (empty == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT) &&
                         (no_storage == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT) &&
                         (window == GL_FRAMEBUFFER_COMPLETE);
    if (gl2_probe_saw) {
        gl2_probe_saw("fbo/completeness", (uint32_t)empty, 0u, rules_ok ? 0 : 1,
                      (uint32_t)no_storage, (uint32_t)window);
    }

    int ok = rules_ok;
    ok = fbo_arm(0, "fbo/renderbuffer", 64, 128, 191) && ok;
    ok = fbo_arm(1, "fbo/texture", 64, 128, 191) && ok;
    return ok;
}

static int check_structs(void) {
    /* Construction and a read of each member, reversed on the way out so a member
     * read from its neighbour's register swaps two channels. */
    int ok = struct_arm("struct C { float r; float g; float b; };\n"
                        "void main() {\n"
                        "  C c = C(0.75, 0.5, 0.25);\n"
                        "  gl_FragColor = vec4(c.b, c.g, c.r, 1.0);\n"
                        "}\n",
                        "structs/construct", 64, 128, 191);

    /* A vector member and a swizzle of it: both meanings of `.` in one expression. */
    ok = ok && struct_arm("struct M { float lead; vec3 v; };\n"
                          "void main() {\n"
                          "  M m = M(0.0, vec3(0.25, 0.5, 0.75));\n"
                          "  gl_FragColor = vec4(m.v.x, m.v.y, m.v.z, 1.0);\n"
                          "}\n",
                          "structs/vec-member", 64, 128, 191);

    /* A member write leaves its neighbours alone. */
    ok = ok && struct_arm("struct C { float r; float g; float b; };\n"
                          "void main() {\n"
                          "  C c = C(0.25, 1.0, 0.75);\n"
                          "  c.g = 0.5;\n"
                          "  gl_FragColor = vec4(c.r, c.g, c.b, 1.0);\n"
                          "}\n",
                          "structs/member-write", 64, 128, 191);

    /* A write to a whole-struct copy leaves the original untouched. */
    ok = ok && struct_arm("struct C { float r; float g; float b; };\n"
                          "void main() {\n"
                          "  C a = C(0.25, 0.5, 0.75);\n"
                          "  C b = a;\n"
                          "  b.r = 1.0;\n"
                          "  gl_FragColor = vec4(a.r, b.g, b.b, 1.0);\n"
                          "}\n",
                          "structs/copy", 64, 128, 191);

    /* A nested struct's layout is offset within the outer one. */
    ok = ok && struct_arm("struct In { float x; float y; };\n"
                          "struct Out { float lead; In in2; };\n"
                          "void main() {\n"
                          "  Out o = Out(0.25, In(0.5, 0.75));\n"
                          "  gl_FragColor = vec4(o.lead, o.in2.x, o.in2.y, 1.0);\n"
                          "}\n",
                          "structs/nested", 64, 128, 191);

    /* Through a function, by value in and by value out. */
    ok = ok &&
         struct_arm("struct C { float r; float g; float b; };\n"
                    "C half_of(C c) { return C(c.r * 0.5, c.g * 0.5, c.b * 0.5); }\n"
                    "void main() {\n"
                    "  C c = half_of(C(0.5, 1.0, 1.5));\n"
                    "  gl_FragColor = vec4(c.r, c.g, c.b, 1.0);\n"
                    "}\n",
                    "structs/function", 64, 128, 191);

    /* Built from a varying, so nothing folds at compile time. `v` is 0.5 at every
     * corner, so the region stays uniform; a read of the literal member instead gives
     * flat grey. */
    ok =
        ok && struct_arm_vs(
                  "attribute vec3 pos;\n"
                  "varying float v;\n"
                  "void main() { v = 0.5; gl_Position = vec4(pos, 1.0); }\n",
                  "varying float v;\n"
                  "struct C { float fromv; float lit; };\n"
                  "void main() {\n"
                  "  C c = C(v, 0.25);\n"
                  "  gl_FragColor = vec4(c.fromv * 0.5, c.fromv, c.fromv * 1.5, 1.0);\n"
                  "}\n",
                  "structs/from-varying", 64, 128, 191);

    return ok;
}

static int check_local_arrays(void) {
    /* Local arrays indexed by a loop counter and by literal. w = 1..4 sums to 10,
     * which no repeated element reaches; a wrong stride moves green off 0.75. */
    return language_check("  float w[4];\n"
                          "  for (int i = 0; i < 4; i++) { w[i] = float(i) + 1.0; }\n"
                          "  float total = 0.0;\n"
                          "  for (int i = 0; i < 4; i++) { total += w[i]; }\n"
                          "  vec3 v[2];\n"
                          "  v[0] = vec3(0.0, 0.25, 0.5);\n"
                          "  v[1] = vec3(0.75, 1.0, 0.0);\n"
                          "  gl_FragColor = vec4(total * 0.1, v[1].x, v[0].z, 1.0);",
                          255, 191, 128, 3);
}

/* A cube map sampled from a shader, with the hardware picking the face. Each face is
 * a flat colour; `-Z` (face 5, cyan) is sampled because a lost sign gives `+Z` (face 4,
 * magenta). */
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
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)f, 0, GL_RGBA, 1, 1, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, faces[f]);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    /* The left half looks along +X and the right half along -Z, in one draw. */
    const GLuint p =
        use_program("attribute vec3 pos;\n"
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
    int ok = near_rgb(SCAN_PX(s, 16, MID_Y), 255, 0, 0, 2); /* +X, face 0 */
    ok = ok &&
         near_rgb(SCAN_PX(s, PROBE_W - 17, MID_Y), 0, 255, 255, 2); /* -Z, face 5 */
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glDeleteTextures(1, &t);
    return ok && glGetError() == GL_NO_ERROR;
}

/* `texture2DProj` with a `vec4` divides by `w` and ignores `z`; dividing by the 99.0
 * in `z` would sample a corner. */
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
    if (!p) {
        glDeleteTextures(1, &tex);
        return 0;
    }
    attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    const int ok = near_rgb(SCAN_PX(s, MID_X, MID_Y), 255, 128, 0, 3);
    glDeleteTextures(1, &tex);
    return ok && glGetError() == GL_NO_ERROR;
}

/* A volume sampled from a shader. Each slice is a flat colour, so the answer names the
 * slice the third coordinate reached: r = 0.75 lands in the second of two. */
static int check_texture_3d(void) {
    reset_view();
    static const GLubyte slices[2][4] = {{255, 0, 0, 255}, {0, 0, 255, 255}};
    GLubyte vol[2 * 4];
    for (int i = 0; i < 2; i++) {
        for (int k = 0; k < 4; k++)
            vol[i * 4 + k] = slices[i][k];
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

    const GLuint p = use_program(
        VS_PASSTHROUGH, "uniform sampler3D vol;\n"
                        "void main() {\n"
                        "  gl_FragColor = texture3D(vol, vec3(0.5, 0.5, 0.75));\n"
                        "}\n");
    if (!p) {
        glDeleteTextures(1, &t);
        return 0;
    }
    attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    const int ok = near_rgb(SCAN_PX(s, MID_X, MID_Y), 0, 0, 255, 4); /* the far slice */
    glBindTexture(GL_TEXTURE_3D, 0);
    glDeleteTextures(1, &t);
    return ok && glGetError() == GL_NO_ERROR;
}

/* A shadow lookup compares with the sampler's `GL_TEXTURE_COMPARE_FUNC` instead of
 * returning a texel. Stored depth is 0.5, so under less-or-equal 0.25 passes and 0.75
 * fails; a reference in the wrong address register shows only on the failing half. */
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 1, 1, 0, GL_DEPTH_COMPONENT,
                 GL_FLOAT, &depth);

    /* The left half references 0.25 and the right half 0.75, from the varying. */
    const GLuint p =
        use_program("attribute vec3 pos;\n"
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
    if (!p) {
        glDeleteTextures(1, &t);
        return 0;
    }
    attrib_rect(glGetAttribLocation(p, "pos"), -0.9f, -0.8f, 0.9f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    int ok = near_rgb(SCAN_PX(s, 16, MID_Y), 255, 255, 255, 4);       /* 0.25 passes */
    ok = ok && near_rgb(SCAN_PX(s, PROBE_W - 17, MID_Y), 0, 0, 0, 4); /* 0.75 fails */
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &t);
    return ok && glGetError() == GL_NO_ERROR;
}

/* `a * b` is a matrix product, not componentwise, and `transpose` moves the
 * off-diagonal. Asymmetric operands make the three answers different colours. */
static int check_matrix_products(void) {
    return language_check_src(
        "#version 120\n"
        "void main() {\n"
        "  mat2 a = mat2(1.0, 2.0, 0.0, 1.0);\n" /* col0 = (1,2), col1 = (0,1) */
        "  mat2 b = mat2(3.0, 0.0, 0.0, 4.0);\n"
        "  mat2 p = a * b;\n"                /* (1,0) element is 6 */
        "  mat2 c = matrixCompMult(a, b);\n" /* the same element is 0 */
        "  mat2 t = transpose(a);\n"         /* t[1][0] is 2, a[1][0] is 0 */
        "  gl_FragColor = vec4(p[0][1] * 0.125, c[0][1], t[1][0] * 0.5, 1.0);\n"
        "}\n",
        191, 0, 255, 3);
}

/* Inverse trigonometry at exact values: asin(1) and acos(0) are pi/2, atan(1) is
 * pi/4. Scaled by 1/pi, the channels are 0.5, 0.5 and 0.25. */
static int check_inverse_trig(void) {
    return language_check("  float a = asin(1.0) * 0.3183098862;\n"
                          "  float b = acos(0.0) * 0.3183098862;\n"
                          "  float c = atan(1.0) * 0.3183098862;\n"
                          "  gl_FragColor = vec4(a, b, c, 1.0);",
                          128, 128, 64, 4);
}

/* `refract` returns the zero vector under total internal reflection. Encoded as
 * `r * 0.5 + 0.5`, the zero vector is flat grey and a NaN is not. */
static int check_refract(void) {
    return language_check("  vec3 i = normalize(vec3(1.0, -0.05, 0.0));\n"
                          "  vec3 r = refract(i, vec3(0.0, 1.0, 0.0), 2.0);\n"
                          "  gl_FragColor = vec4(r * 0.5 + 0.5, 1.0);",
                          128, 128, 128, 3);
}

static int check_constructors(void) {
    /* A scalar matrix constructor builds a diagonal, not a matrix of ones, and a vector
     * constructor gathers components from mixed arguments. */
    return language_check(
        "  mat2 m = mat2(0.5);\n"
        "  vec2 v = m * vec2(1.0, 1.0);\n" /* (0.5, 0.5), not (1.0, 1.0) */
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
    const GLuint near_p = use_program(
        VS_PASSTHROUGH, "void main() { gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0); }\n");
    if (!near_p)
        return 0;
    attrib_rect(glGetAttribLocation(near_p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, -0.5f);
    const GLuint far_p = use_program(
        VS_PASSTHROUGH, "void main() { gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n");
    if (!far_p)
        return 0;
    attrib_rect(glGetAttribLocation(far_p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.5f);
    const uint32_t *s = scan_frame();
    /* The near one wins: shader output goes through the depth test. */
    return near_rgb(SCAN_PX(s, MID_X, MID_Y), 0, 255, 0, 2) &&
           glGetError() == GL_NO_ERROR;
}

static int check_blend_applies(void) {
    reset_view();
    const GLuint p = use_program(
        VS_PASSTHROUGH, "void main() { gl_FragColor = vec4(1.0, 1.0, 1.0, 0.5); }\n");
    if (!p)
        return 0;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    /* The shader's alpha is the blend's source alpha: white at 0.5 over the 0x20
     * background is about 0x90 per channel. Censused, not sampled, because a combining
     * blend can be right at one pixel per 2x2 quad and wrong at the rest. */
    const int bad = census_wrong(s, 0x8f, 0x8f, 0x8f, 3);
    const int ok = (bad == 0);
    uniformity_census_of(s, "blend/uniformity");
    if (gl2_probe_saw) {
        gl2_probe_saw("blend/wrong-of-3072", (uint32_t)bad, 0u, 0,
                      SCAN_PX(s, MID_X, MID_Y), 0u);
    }
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_cull_face_applies(void) {
    reset_view();
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    const GLuint p = use_program(
        VS_PASSTHROUGH, "void main() { gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n");
    if (!p)
        return 0;
    const GLint loc = glGetAttribLocation(p, "pos");
    if (loc < 0)
        return 0;
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

    /* The other winding does appear. */
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
    const GLuint p = use_program(
        VS_PASSTHROUGH, "void main() { gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0); }\n");
    if (!p)
        return 0;
    const GLint loc = glGetAttribLocation(p, "pos");

    /* The scissor is a window rectangle with its origin at the bottom-left, so this
     * keeps the lower-left quarter of the probe region. */
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, PROBE_W / 2, PROBE_H / 2);
    /* The colour mask applies to shader output: with green masked, white becomes
     * magenta. */
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
    /* GLSL 1.20 implicit int-to-float conversion: `2 * 0.25` is 0.5 under 1.20 and an
     * error under 1.10. Each channel converts through a different route. */
    const GLuint p =
        use_program("#version 120\n"
                    "attribute vec3 pos;\n"
                    "void main() { gl_Position = vec4(pos, 1.0); }\n",
                    "#version 120\n"
                    "float half_of(float x) { return x * 0.5; }\n"
                    "void main() {\n"
                    "  float a = 2 * 0.25;\n"       /* an operator      -> 0.5  */
                    "  float b = half_of(1);\n"     /* an argument      -> 0.5  */
                    "  float c = clamp(3, 0, 1);\n" /* a built-in       -> 1.0  */
                    "  float d = 1;\n"              /* an initialiser   -> 1.0  */
                    "  gl_FragColor = vec4(a, b * 0.5, c * d * 0.75, 1.0);\n"
                    "}\n");
    if (!p)
        return 0;
    attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    int ok = near_rgb(SCAN_PX(s, MID_X, MID_Y), 128, 64, 191, 3);

    /* The same source is an error in 1.10. */
    GLuint bad = make_shader(GL_FRAGMENT_SHADER,
                             "#version 110\n"
                             "void main() { gl_FragColor = vec4(2 * 0.25); }\n");
    ok = ok && bad == 0u;

    /* `invariant`, `centroid` and 1.20's two matrix built-ins compile, with both
     * qualifiers on one declaration. */
    GLuint quals =
        make_shader(GL_VERTEX_SHADER, "#version 120\n"
                                      "invariant centroid varying vec3 v;\n"
                                      "attribute vec3 pos;\n"
                                      "invariant gl_Position;\n"
                                      "void main() {\n"
                                      "  v = transpose(outerProduct(pos, pos)) * pos;\n"
                                      "  gl_Position = vec4(pos, 1.0);\n"
                                      "}\n");
    ok = ok && quals != 0u;
    if (quals)
        glDeleteShader(quals);

    GLuint quals110 = make_shader(
        GL_VERTEX_SHADER, "#version 110\n"
                          "centroid varying vec3 v;\n"
                          "attribute vec3 pos;\n"
                          "void main() { v = pos; gl_Position = vec4(pos, 1.0); }\n");
    ok = ok && quals110 == 0u;

    /* A later dialect is refused by number rather than taken as 1.20. */
    GLuint v130 =
        make_shader(GL_FRAGMENT_SHADER,
                    "#version 130\nout vec4 c;\nvoid main() { c = vec4(1.0); }\n");
    ok = ok && v130 == 0u;
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_separate_stencil(void) {
    reset_view();
    /* Separate stencil ops per face: front faces increment and back faces decrement, so
     * the same quad drawn with both windings returns the stencil to 0 (one shared state
     * leaves 2). A GL_EQUAL draw afterwards reads it. */
    glDisable(GL_CULL_FACE);
    glEnable(GL_STENCIL_TEST);
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
    glStencilFunc(GL_ALWAYS, 0, 0xffu);
    glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR);
    glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR);
    /* Nothing is written to colour while the stencil is being built. */
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    const GLuint p =
        use_program(VS_PASSTHROUGH, "void main() { gl_FragColor = vec4(1.0); }\n");
    if (!p)
        return 0;
    const GLint loc = glGetAttribLocation(p, "pos");
    if (loc < 0)
        return 0;

    /* The same quad twice, wound opposite ways. */
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
    const GLuint p2 = use_program(
        VS_PASSTHROUGH, "void main() { gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0); }\n");
    if (!p2)
        return 0;
    attrib_rect(glGetAttribLocation(p2, "pos"), -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);

    const uint32_t *s = scan_frame();
    /* Green inside: the two faces cancelled. Green outside is the control. */
    int ok = near_rgb(SCAN_PX(s, MID_X, MID_Y), 0, 255, 0, 2);
    ok = ok && near_rgb(SCAN_PX(s, 4, 4), 0, 255, 0, 2);
    glDisable(GL_STENCIL_TEST);
    return ok && glGetError() == GL_NO_ERROR;
}

static int check_separate_blend_equation(void) {
    reset_view();
    /* Separate blend equations: the colour subtracts and the alpha adds. */
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_ADD);

    const GLuint p =
        use_program(VS_PASSTHROUGH,
                    "void main() { gl_FragColor = vec4(0.25, 0.25, 0.25, 0.0); }\n");
    if (!p)
        return 0;
    attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);

    const uint32_t *s = scan_frame();
    /* Destination minus source, 0.5 - 0.25, is 64. Adding would give 191. */
    int ok = near_rgb(SCAN_PX(s, MID_X, MID_Y), 64, 64, 64, 6);
    /* A diagnostic row: this blend combines two terms, so the region's agreement with
     * its centre is reported alongside the verdict, which measures the equation. */
    uniformity_census_of(s, "separate-blend-eq/uniformity");
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

    /* A name covering more than one buffer is GL_INVALID_OPERATION (GL 2.0, 4.2.1). */
    const GLenum wide[1] = {GL_FRONT_AND_BACK};
    glDrawBuffers(1, wide);
    ok = ok && glGetError() == GL_INVALID_OPERATION;

    /* `GL_MAX_DRAW_BUFFERS` is 1: it is also the length of `gl_FragData`, and the
     * fragment stage exports one colour target. A longer list is GL_INVALID_VALUE. */
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &v);
    ok = ok && v == 1;
    const GLenum twice[2] = {GL_BACK, GL_BACK};
    glDrawBuffers(2, twice);
    ok = ok && glGetError() == GL_INVALID_VALUE;

    /* The selected buffer is the one a draw reaches. */
    const GLuint p = use_program(
        VS_PASSTHROUGH, "void main() { gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n");
    if (!p)
        return 0;
    attrib_rect(glGetAttribLocation(p, "pos"), -0.8f, -0.8f, 0.8f, 0.8f, 0.0f);
    const uint32_t *s = scan_frame();
    ok = ok && near_rgb(SCAN_PX(s, MID_X, MID_Y), 255, 0, 0, 2);
    return ok && glGetError() == GL_NO_ERROR;
}

/* One pixel of a named colour buffer, packed A, R, G, B as the `saw` rows read.
 * `glReadPixels` reads the named buffer, not the rotating readback copy, and callers
 * take these reads before they scan. */
static uint32_t pixel_of(GLenum buffer, int gx, int gy) {
    GLubyte c[4] = {0u, 0u, 0u, 0u};
    glReadBuffer(buffer);
    glReadPixels(gx, gy, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, c);
    glReadBuffer(GL_BACK);
    return ((uint32_t)c[3] << 24) | ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) |
           (uint32_t)c[2];
}

static uint32_t centre_of(GLenum buffer) {
    return pixel_of(buffer, MID_X, MID_Y);
}

/* GL row `MID_Y - 1`, which is the row `scan_frame` calls `MID_Y`: the scan indexes
 * the readback top-down, so scan row 48 is GL row 47. */
static uint32_t row_below_centre_of(GLenum buffer) {
    return pixel_of(buffer, MID_X, MID_Y - 1);
}

/* The census box: 64 by 48 pixels, centred, inside every quad this suite draws (the
 * smallest, `-0.8..0.8`, covers x 13..115 and y 10..86), so an undrawn border never
 * counts. */
#define CENSUS_X0 (MID_X - 32)
#define CENSUS_X1 (MID_X + 32)
#define CENSUS_Y0 (MID_Y - 24)
#define CENSUS_Y1 (MID_Y + 24)

/* How many census-box pixels are not the expected colour; matches gl1-probe's
 * `census_wrong`. */
static int census_wrong(const uint32_t *s, int r, int g, int b, int tol) {
    int bad = 0;
    for (int y = CENSUS_Y0; y < CENSUS_Y1; y++) {
        for (int x = CENSUS_X0; x < CENSUS_X1; x++) {
            if (!near_rgb(SCAN_PX(s, x, y), r, g, b, tol))
                bad++;
        }
    }
    return bad;
}

/* Per channel, how many census-box pixels differ from the centre. Run after an
 * unblended draw, a lattice here belongs to the draw rather than the blend. */
static void uniformity_census_of(const uint32_t *s, const char *name) {
    const uint32_t mid = SCAN_PX(s, MID_X, MID_Y);
    int dr = 0, dg = 0, db = 0;
    for (int y = CENSUS_Y0; y < CENSUS_Y1; y++) {
        for (int x = CENSUS_X0; x < CENSUS_X1; x++) {
            const uint32_t c = SCAN_PX(s, x, y);
            if (chan_r(c) != chan_r(mid))
                dr++;
            if (chan_g(c) != chan_g(mid))
                dg++;
            if (chan_b(c) != chan_b(mid))
                db++;
        }
    }
    /* `saw` is the centre the three counts are measured against. */
    if (gl2_probe_saw)
        gl2_probe_saw(name, mid, 0u, dr, (uint32_t)dg, (uint32_t)db);
}

/* The uniformity census with a fresh scan, for a check that has not scanned yet; a
 * check that has uses `uniformity_census_of`, since a second scan reads the next
 * scanout buffer. */
static void uniformity_census(const char *name) {
    uniformity_census_of(scan_frame(), name);
}

/* The blue channel across the whole region: counts of full, zero and other values,
 * and bit masks of full blue down the centre column and along the centre row, in scan
 * coordinates (scan row `y` is GL row `PROBE_H - 1 - y`). */
static void blue_census(const char *name_count, const char *name_rows,
                        const char *name_cols, const char *name_cols3) {
    const uint32_t *const s = scan_frame();
    int n_full = 0, n_zero = 0, n_other = 0;
    uint32_t rows[3] = {0u, 0u, 0u};     /* scan rows 0..95 at the centre column */
    uint32_t cols[4] = {0u, 0u, 0u, 0u}; /* scan cols 0..127 at the centre row */
    for (int y = 0; y < PROBE_H; y++) {
        for (int x = 0; x < PROBE_W; x++) {
            const int b = chan_b(SCAN_PX(s, x, y));
            if (b >= 250)
                n_full++;
            else if (b <= 5)
                n_zero++;
            else
                n_other++;
        }
        if (chan_b(SCAN_PX(s, MID_X, y)) >= 250)
            rows[y >> 5] |= 1u << (y & 31);
    }
    for (int x = 0; x < PROBE_W; x++) {
        if (chan_b(SCAN_PX(s, x, MID_Y)) >= 250)
            cols[x >> 5] |= 1u << (x & 31);
    }
    if (!gl2_probe_saw)
        return;
    gl2_probe_saw(name_count, (uint32_t)n_full, 0u, 0, (uint32_t)n_zero,
                  (uint32_t)n_other);
    gl2_probe_saw(name_rows, rows[0], 0u, 0, rows[1], rows[2]);
    gl2_probe_saw(name_cols, cols[0], 0u, 0, cols[1], cols[2]);
    gl2_probe_saw(name_cols3, cols[3], 0u, 0, 0u, 0u);
}

/* A loop keeps an unblended draw uniform. A flat shader and one arm per loop shape all
 * paint (64, 128, 64), censused over the interior, so a count that moves names the
 * shape. */
static int check_loop_uniformity(void) {
    reset_view();
    const GLuint a = use_program(
        VS_PASSTHROUGH, "void main() { gl_FragColor = vec4(0.25, 0.5, 0.25, 1.0); }\n");
    if (!a)
        return 0;
    attrib_rect(glGetAttribLocation(a, "pos"), -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
    const uint32_t *s = scan_frame();
    const int flat_bad = census_wrong(s, 64, 128, 64, 3);
    const uint32_t flat_mid = SCAN_PX(s, MID_X, MID_Y);

    /* The arms vary the loop shape: a uniform `if` inside a branched loop (as an
     * assignment, a read-modify-write, or guarding `break`), a per-pixel `break`, and
     * loops with no `if`. `counter-out` measures the loop counter, not the mask. */
    static const struct {
        const char *name;
        const char *body;
    } ARMS[8] = {
        /* A per-pixel `break` condition. Every interior lane has `gl_FragCoord.x`
         * above 8, so all eight adds land: `0.03125 * 8` is 0.25. */
        {"loop-uniformity/break-divergent",
         "  float b = 0.0;\n"
         "  for (int k = 0; k < 8; k++) { if (float(k) > gl_FragCoord.x) break; b += "
         "0.03125; }\n"
         "  gl_FragColor = vec4(0.25, 0.5, b, 1.0);\n"},
        /* The loop counter advances: `last` is 7 after the loop, and 7 / 28 is the
         * 0.25 the other arms expect. A stuck counter reads red 0. */
        {"loop-uniformity/counter-out",
         "  float last = 0.0;\n"
         "  for (int k = 0; k < 8; k++) { if (k < 1) last = 0.0; last = float(k); }\n"
         "  gl_FragColor = vec4(last / 28.0, 0.5, 0.25, 1.0);\n"},
        /* `if-assign` and `if-rmw` differ only in whether the masked store reads `x`
         * first. The assigned value depends on the trip, so a mask that never applies
         * stores 7.25 and saturates. */
        {"loop-uniformity/if-assign",
         "  float x = 0.0;\n"
         "  for (int k = 0; k < 8; k++) { if (k < 1) x = 0.25 + float(k); }\n"
         "  gl_FragColor = vec4(0.25, 0.5, x, 1.0);\n"},
        {"loop-uniformity/if-rmw",
         "  float x = 0.0;\n"
         "  for (int k = 0; k < 8; k++) { if (k < 1) x += 0.25; }\n"
         "  gl_FragColor = vec4(0.25, 0.5, x, 1.0);\n"},
        {"loop-uniformity/if-in-loop",
         "  float b = 0.0;\n"
         "  for (int k = 0; k < 8; k++) { if (k < 1) b += 0.25; }\n"
         "  gl_FragColor = vec4(0.25, 0.5, b, 1.0);\n"},
        {"loop-uniformity/break",
         "  float b = 0.0;\n"
         "  for (int k = 0; k < 8; k++) { if (k == 1) break; b += 0.25; }\n"
         "  gl_FragColor = vec4(0.25, 0.5, b, 1.0);\n"},
        {"loop-uniformity/count", "  float g = 0.0;\n"
                                  "  for (int j = 0; j < 4; j++) { g += 0.125; }\n"
                                  "  gl_FragColor = vec4(0.25, g, 0.25, 1.0);\n"},
        {"loop-uniformity/once", "  float r = 0.0;\n"
                                 "  for (int i = 0; i < 1; i++) { r += 0.25; }\n"
                                 "  gl_FragColor = vec4(r, 0.5, 0.25, 1.0);\n"},
    };

    int worst = 0;
    for (int m = 0; m < 8; m++) {
        char src[320];
        int at = 0;
        const char *head = "void main() {\n";
        for (int c = 0; head[c] && at < 260; c++)
            src[at++] = head[c];
        for (int c = 0; ARMS[m].body[c] && at < 310; c++)
            src[at++] = ARMS[m].body[c];
        src[at++] = '}';
        src[at++] = '\n';
        src[at] = '\0';

        reset_view();
        const GLuint b = use_program(VS_PASSTHROUGH, src);
        if (!b)
            return 0;
        attrib_rect(glGetAttribLocation(b, "pos"), -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
        s = scan_frame();
        const int bad = census_wrong(s, 64, 128, 64, 3);
        if (bad > worst)
            worst = bad;
        /* The first wrong pixel, as `(x << 16) | y` in scan coordinates, and its
         * value. */
        uint32_t first_at = 0xffffffffu, first_px = 0u;
        for (int y = CENSUS_Y0; y < CENSUS_Y1 && first_at == 0xffffffffu; y++) {
            for (int x = CENSUS_X0; x < CENSUS_X1; x++) {
                if (!near_rgb(SCAN_PX(s, x, y), 64, 128, 64, 3)) {
                    first_at = ((uint32_t)x << 16) | (uint32_t)y;
                    first_px = SCAN_PX(s, x, y);
                    break;
                }
            }
        }
        if (gl2_probe_saw) {
            gl2_probe_saw(ARMS[m].name, SCAN_PX(s, MID_X, MID_Y), 0u, bad, first_at,
                          first_px);
        }
    }

    if (gl2_probe_saw)
        gl2_probe_saw("loop-uniformity/flat", flat_mid, 0u, flat_bad, 0u, 0u);
    return flat_bad == 0 && worst == 0 && glGetError() == GL_NO_ERROR;
}

/* `do { } while` runs its body once before the condition is tested, so a false first
 * condition still gives one trip. Both paths draw it and must agree; a counted `for`
 * with `break` giving the same three answers is measured beside it. */
static int check_do_while(void) {
    reset_view();
    const char *const DO_FS = "void main() {\n"
                              "  float once = 0.0;\n"
                              "  int i = 0;\n"
                              "  do { once += 1.0; i++; } while (i < 0);\n"
                              "  gl_FragColor = vec4(once / 4.0, 0.5, 0.25, 1.0);\n"
                              "}\n";
    /* It links on both paths: a console code-generation failure is reported by the
     * draw, not by `GL_LINK_STATUS`, which reads the same on host and console. */
    const GLuint p = use_program(VS_PASSTHROUGH, DO_FS);
    if (!p)
        return 0;
    GLint linked = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &linked);
    int ok = (linked == GL_TRUE);

    (void)glGetError();
    attrib_rect(glGetAttribLocation(p, "pos"), -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
    const GLenum drew = glGetError();
    const uint32_t *s = scan_frame();
    const int painted = census_wrong(s, 0x20, 0x20, 0x20, 4);

    /* `once` is 1, so red is 1/4. The console bounds `do` and `while` with the
     * `GLSL_GEN_MAX_TRIPS` ceiling rather than a counted trip number. */
    ok = ok && drew == GL_NO_ERROR && census_wrong(s, 64, 128, 64, 3) == 0;
    (void)painted;

    /* The counted-`for`-with-`break` form of the same three answers. */
    reset_view();
    const GLuint q =
        use_program(VS_PASSTHROUGH,
                    "void main() {\n"
                    "  float once = 0.0;\n"
                    "  for (int i = 0; i < 1; i++) { once += 1.0; }\n"
                    "  float n = 0.0;\n"
                    "  for (int j = 0; j < 4; j++) { n += 1.0; }\n"
                    "  float b = 0.0;\n"
                    "  for (int k = 0; k < 8; k++) { if (k == 1) break; b += 1.0; }\n"
                    "  gl_FragColor = vec4(once / 4.0, n / 8.0, b / 4.0, 1.0);\n"
                    "}\n");
    if (!q)
        return 0;
    attrib_rect(glGetAttribLocation(q, "pos"), -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
    s = scan_frame();
    /* 1/4, 4/8 and 1/4 of 255: 64, 128, 64. */
    const int bad = census_wrong(s, 64, 128, 64, 3);
    if (gl2_probe_saw) {
        gl2_probe_saw("do-while/refused-err", (uint32_t)drew, 0u, painted,
                      SCAN_PX(s, MID_X, MID_Y), (uint32_t)bad);
    }
    return ok && bad == 0 && glGetError() == GL_NO_ERROR;
}

/* The introspection entry points: shader source, attached shaders, active attributes,
 * uniform and vertex-attribute getters, `glStencilMaskSeparate`, `glDetachShader` and
 * `glValidateProgram`. Ports call these to learn what they hold, so a wrong answer
 * binds the wrong attribute rather than drawing a wrong picture. Nothing here draws. */
static int check_program_introspection(void) {
    reset_view();
    const char *const VS_SRC =
        "attribute vec3 pos;\n"
        "attribute vec2 uv;\n"
        "uniform int k;\n"
        "varying vec2 v;\n"
        "void main() { v = uv * float(k); gl_Position = vec4(pos, 1.0); }\n";
    GLuint vs = make_shader(GL_VERTEX_SHADER, VS_SRC);
    GLuint fs = make_shader(GL_FRAGMENT_SHADER,
                            "varying vec2 v;\n"
                            "void main() { gl_FragColor = vec4(v, 0.0, 1.0); }\n");
    if (!vs || !fs)
        return 0;
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
    g_prog = prog;
    glUseProgram(prog);
    int ok = 1;

    /* The source comes back as it went in, its length not counting the terminator. */
    {
        char buf[256];
        GLsizei len = -1;
        for (int i = 0; i < 256; i++)
            buf[i] = '\0';
        glGetShaderSource(vs, (GLsizei)sizeof(buf), &len, buf);
        int n = 0;
        while (VS_SRC[n] != '\0')
            n++;
        ok = ok && len == (GLsizei)n && buf[n] == '\0';
        for (int i = 0; i < n && ok; i++)
            ok = ok && buf[i] == VS_SRC[i];
    }

    /* Both shaders come back attached; `count` is what was written. */
    {
        GLuint got[4] = {0u, 0u, 0u, 0u};
        GLsizei count = -1;
        glGetAttachedShaders(prog, 4, &count, got);
        ok = ok && count == 2;
        const int has_vs = (got[0] == vs || got[1] == vs);
        const int has_fs = (got[0] == fs || got[1] == fs);
        ok = ok && has_vs && has_fs;
    }

    /* An active attribute by index, with name, type and size. The index is unrelated
     * to the location, which is queried separately. */
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
            for (int c = 0; c < 64; c++)
                name[c] = '\0';
            glGetActiveAttrib(prog, (GLuint)i, (GLsizei)sizeof(name), &len, &size,
                              &type, name);
            ok = ok && size == 1 && len > 0;
            if (name[0] == 'p') {
                seen_pos = 1;
                ok = ok && type == GL_FLOAT_VEC3;
            }
            if (name[0] == 'u') {
                seen_uv = 1;
                ok = ok && type == GL_FLOAT_VEC2;
            }
        }
        ok = ok && seen_pos && seen_uv;
    }

    /* An integer uniform, read back through the integer getter. */
    {
        const GLint loc = glGetUniformLocation(prog, "k");
        ok = ok && loc >= 0;
        glUniform1i(loc, 7);
        GLint got = 0;
        glGetUniformiv(prog, loc, &got);
        ok = ok && got == 7;
    }

    /* Vertex array state through all three getters; the pointer getter takes a
     * `void **`. */
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
        glGetVertexAttribPointerv((GLuint)pos_loc, GL_VERTEX_ATTRIB_ARRAY_POINTER,
                                  &ptr);
        ok = ok && ptr == (void *)verts;
        /* The current generic value, through the float getter. */
        glVertexAttrib4f((GLuint)pos_loc, 0.25f, 0.5f, 0.75f, 1.0f);
        float cur[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        glGetVertexAttribfv((GLuint)pos_loc, GL_CURRENT_VERTEX_ATTRIB, cur);
        ok = ok && near_chan((int)(cur[0] * 255.0f + 0.5f), 64, 1) &&
             near_chan((int)(cur[2] * 255.0f + 0.5f), 191, 1);
        glDisableVertexAttribArray((GLuint)pos_loc);
    }

    /* The front and back stencil write masks move independently. */
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

/* `gl_PointCoord` (GLSL 1.20) across a large point: s rises to the right and t rises
 * downward, since `GL_POINT_SPRITE_COORD_ORIGIN` defaults to `GL_UPPER_LEFT`. There is
 * no `glEnable(GL_POINT_SPRITE)`: the coordinate is defined for any point. */
static int check_point_coord(void) {
    reset_view();
    /* A fragment shader on its own, as `gl_PointCoord` requires here: points expand to
     * squares in object space before the vertex stage, and a vertex shader would see
     * identical attributes at all four corners and collapse the square. */
    GLuint fs =
        make_shader(GL_FRAGMENT_SHADER,
                    "void main() { gl_FragColor = vec4(gl_PointCoord, 0.0, 1.0); }\n");
    if (!fs)
        return 0;
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

    /* Large and centred, so the whole point lies inside the region. */
    glPointSize(64.0f);
    glBegin(GL_POINTS);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glEnd();
    glPointSize(1.0f);

    const uint32_t *const s = scan_frame();
    /* The point spans x 32..96 and scan rows 16..80; sampled a quarter of the way in
     * from each edge, clear of the coverage boundary. */
    const uint32_t tl = SCAN_PX(s, 48, 32);
    const uint32_t tr = SCAN_PX(s, 80, 32);
    const uint32_t bl = SCAN_PX(s, 48, 64);
    const uint32_t br = SCAN_PX(s, 80, 64);
    if (gl2_probe_saw) {
        gl2_probe_saw("point-coord/top", tl, 0u, 0, tr, 0u);
        gl2_probe_saw("point-coord/bottom", bl, 0u, 0, br, 0u);
    }

    /* Orderings rather than values, because the exact coordinate depends on where the
     * rasteriser puts the point's edges. */
    int ok = chan_r(tr) > chan_r(tl) + 32 && chan_r(br) > chan_r(bl) + 32;
    ok = ok && chan_g(bl) > chan_g(tl) + 32 && chan_g(br) > chan_g(tr) + 32;
    /* Blue is the shader's constant 0, so a background quadrant is caught. */
    ok = ok && chan_b(tl) == 0 && chan_b(br) == 0;
    return ok && glGetError() == GL_NO_ERROR;
}

/* A spatial map of the uniform-`if`-in-a-loop shader over the whole region. The
 * shader leaves 0.25, or 2.0 saturated where the mask fails; a failure has one value,
 * so each pixel is one bit. */
#define LS_WORDS (PROBE_W * PROBE_H / 32) /* 12,288 pixels, one bit each */

static uint32_t g_ls_a[LS_WORDS];
static uint32_t g_ls_b[LS_WORDS];

/* One draw reduced to a bitmap, set meaning saturated. Returns the bits set, or -1 if
 * any pixel holds a third value, which a one-bit map cannot carry. */
static int ls_build(uint32_t *map) {
    const uint32_t *const s = scan_frame();
    int n = 0;
    for (int i = 0; i < LS_WORDS; i++)
        map[i] = 0u;
    for (int y = 0; y < PROBE_H; y++) {
        for (int x = 0; x < PROBE_W; x++) {
            const uint32_t c = SCAN_PX(s, x, y);
            if (near_rgb(c, 64, 128, 64, 3))
                continue;
            if (!near_rgb(c, 64, 128, 255, 3))
                return -1;
            const int bit = y * PROBE_W + x;
            map[bit >> 5] |= 1u << (bit & 31);
            n++;
        }
    }
    return n;
}

static int ls_popcount(uint32_t v) {
    int n = 0;
    while (v) {
        v &= v - 1u;
        n++;
    }
    return n;
}

static int ls_draw(void) {
    reset_view();
    const GLuint p = use_program(
        VS_PASSTHROUGH, "void main() {\n"
                        "  float x = 0.0;\n"
                        "  for (int k = 0; k < 8; k++) { if (k < 1) x += 0.25; }\n"
                        "  gl_FragColor = vec4(0.25, 0.5, x, 1.0);\n"
                        "}\n");
    if (!p)
        return 0;
    attrib_rect(glGetAttribLocation(p, "pos"), -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
    return 1;
}

static int check_loop_spatial(void) {
    /* The same draw runs twice and the bitmaps are compared: a fault tied to pixel
     * position gives identical maps, and one tied to wave scheduling does not. The full
     * map is dumped rather than summarised. */
    if (!ls_draw())
        return 0;
    const int a = ls_build(g_ls_a);
    if (!ls_draw())
        return 0;
    const int b = ls_build(g_ls_b);

    int differ = 0, first = -1;
    if (a >= 0 && b >= 0) {
        for (int i = 0; i < LS_WORDS; i++) {
            const uint32_t d = g_ls_a[i] ^ g_ls_b[i];
            if (d && first < 0) {
                int t = 0;
                while (!((d >> t) & 1u))
                    t++;
                first = i * 32 + t;
            }
            differ += ls_popcount(d);
        }
    }

    if (gl2_probe_saw) {
        /* `saw` the first draw's count, `drawn` the second's, `L` how many pixels
         * changed between them and `R` the first that did. */
        gl2_probe_saw("loop-spatial/pass2", (uint32_t)(a < 0 ? -1 : a), 0u,
                      b < 0 ? 0 : b, (uint32_t)differ, (uint32_t)first);
        /* The map, three words to a line, with `err` and `drawn` carrying the line
         * index so an interleaved log reassembles. Printed only on a fault. */
        if (a > 0) {
            for (int i = 0; i < LS_WORDS / 3; i++)
                gl2_probe_saw("loop-spatial/map", g_ls_a[i * 3], (unsigned int)i, i,
                              g_ls_a[i * 3 + 1], g_ls_a[i * 3 + 2]);
        }
    }
    return a == 0 && b == 0 && glGetError() == GL_NO_ERROR;
}

/* Every blend mode leaves a full-region draw uniform. `GL_ONE, GL_ZERO` is a copy and
 * `GL_ZERO, GL_ONE` keeps the destination; the alpha and `GL_DST_COLOR` arms combine
 * both terms. Each arm reports per-channel counts differing from the centre. */
static int check_blend_uniformity(void) {
    reset_view();
    const GLuint p = use_program(VS_PASSTHROUGH, "uniform vec4 c;\n"
                                                 "void main() { gl_FragColor = c; }\n");
    if (!p)
        return 0;
    const GLint pos = glGetAttribLocation(p, "pos");
    const GLint kc = glGetUniformLocation(p, "c");
    if (pos < 0 || kc < 0)
        return 0;

    /* Source alpha decides whether `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA` combines: at
     * 1.0 it is a copy, so that arm uses 0.5. */
    static const struct {
        const char *name;
        GLenum src, dst;
        float src_alpha;
    } MODES[5] = {
        {"blend-uniformity/one-one", GL_ONE, GL_ONE, 1.0f},
        {"blend-uniformity/one-zero", GL_ONE, GL_ZERO, 1.0f},
        {"blend-uniformity/zero-one", GL_ZERO, GL_ONE, 1.0f},
        /* Half alpha: `src/2 + dst/2`. */
        {"blend-uniformity/src-alpha", GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, 0.5f},
        /* A sum that does not go through alpha. */
        {"blend-uniformity/dst-color", GL_DST_COLOR, GL_ONE, 1.0f},
    };

    int ok = 1;
    for (int m = 0; m < 5; m++) {
        /* A destination non-zero in every channel, so no arm is clean by accident. */
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

        /* Every arm is in the verdict. */
        const uint32_t *const s = scan_frame();
        const uint32_t mid = SCAN_PX(s, MID_X, MID_Y);
        for (int y = 0; y < PROBE_H && ok; y++) {
            for (int x = 0; x < PROBE_W && ok; x++) {
                if (SCAN_PX(s, x, y) != mid)
                    ok = 0;
            }
        }
    }
    return ok && glGetError() == GL_NO_ERROR;
}

/* One compiled shader blending into two colour buffers through
 * `glDrawBuffer(GL_FRONT_AND_BACK)`, each against its own destination. Two arms, source
 * blue set and zero. The front's destination blue is 0, so its expected blue differs
 * per arm; the back's is 255 and saturates in both, so the back is the control. */
static int check_two_draw_buffers(void) {
    reset_view();
    const GLuint p = use_program(VS_PASSTHROUGH, "uniform vec4 c;\n"
                                                 "void main() { gl_FragColor = c; }\n");
    if (!p)
        return 0;
    const GLint pos = glGetAttribLocation(p, "pos");
    const GLint kc = glGetUniformLocation(p, "c");
    if (pos < 0 || kc < 0)
        return 0;

    int ok = 1;
    for (int arm = 0; arm < 2; arm++) {
        /* 0.75 is 191 of 255, far from both destinations. */
        const float src_b = (arm == 0) ? 0.75f : 0.0f;
        const int want_b = (arm == 0) ? 191 : 0;
        const char *const was_name =
            (arm == 0) ? "two-draw-buffers/set-was" : "two-draw-buffers/zero-was";
        const char *const got_name =
            (arm == 0) ? "two-draw-buffers/set-got" : "two-draw-buffers/zero-got";

        /* Each destination drawn on its own, so they differ in every channel the blend
         * reads. The blended draw's alpha is 0, so destination alpha stays 1.0. */
        glDisable(GL_BLEND);
        glDrawBuffer(GL_BACK);
        glUniform4f(kc, 0.0f, 0.0f, 1.0f, 1.0f);
        attrib_rect(pos, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
        glDrawBuffer(GL_FRONT);
        glUniform4f(kc, 1.0f, 0.0f, 0.0f, 1.0f);
        attrib_rect(pos, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
        glDrawBuffer(GL_BACK);

        /* Both destinations before the blended draw, through the reads the verdict
         * uses: red and blue, or the check measured no blend. */
        const uint32_t was_front = centre_of(GL_FRONT);
        const uint32_t was_back = centre_of(GL_BACK);

        glDrawBuffer(GL_FRONT_AND_BACK);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glUniform4f(kc, 0.25f, 0.5f, src_b, 0.0f);
        attrib_rect(pos, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);

        /* The front read while it is still the second target. The copy that makes a
         * second target readable is emitted only while `fb_also` is live, and
         * `glDrawBuffer(GL_BACK)` clears it, so this separates an unwritten target
         * from an unread one. */
        const uint32_t front_live = centre_of(GL_FRONT);

        glDisable(GL_BLEND);
        glDrawBuffer(GL_BACK);

        const uint32_t front = centre_of(GL_FRONT);
        const uint32_t back = centre_of(GL_BACK);

        /* GL row 47, which is the row the verdict's own scan calls the centre. */
        const uint32_t back_below = row_below_centre_of(GL_BACK);
        const uint32_t front_below = row_below_centre_of(GL_FRONT);

        if (gl2_probe_saw) {
            gl2_probe_saw(was_name, was_front, 0u, 0, was_back, 0u);
            /* `saw` the front while bound as the second target, `L` the same pixel
             * after the draw buffer went back to one. */
            gl2_probe_saw(arm == 0 ? "two-draw-buffers/set-live"
                                   : "two-draw-buffers/zero-live",
                          front_live, 0u, 0, front, 0u);
            gl2_probe_saw(got_name, front, 0u, 0, back, 0u);
            /* `saw` the front one row down, `L` the back one row down - the row the
             * scan calls the centre. */
            gl2_probe_saw(arm == 0 ? "two-draw-buffers/set-row47"
                                   : "two-draw-buffers/zero-row47",
                          front_below, 0u, 0, back_below, 0u);
        }

        ok =
            ok && near_rgb(was_front, 255, 0, 0, 2) && near_rgb(was_back, 0, 0, 255, 2);
        /* 0.25 and 0.5 are 64 and 128; the front adds them to red, the back to blue. */
        ok = ok && near_rgb(front, 255, 128, want_b, 6);
        ok = ok && near_rgb(back, 64, 128, 255, 6);
    }

    /* The back target's region after the two-target blend, as a control: it is uniform
     * when the single-target path is right. */
    blue_census("two-draw-buffers/two-count", "two-draw-buffers/two-rows",
                "two-draw-buffers/two-cols", "two-draw-buffers/two-cols3");
    uniformity_census("two-draw-buffers/two-u");

    /* The same blend into one target, the control for the two-target census. */
    glDisable(GL_BLEND);
    glDrawBuffer(GL_BACK);
    glUniform4f(kc, 0.0f, 0.0f, 1.0f, 1.0f);
    attrib_rect(pos, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
    /* An unblended constant-colour draw is uniform; a lattice here is the draw's. */
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
    /* The same program drawn twice produces the same frame word for word. */
    const GLuint p = use_program("uniform float k;\n"
                                 "attribute vec3 pos;\n"
                                 "varying vec3 v;\n"
                                 "void main() {\n"
                                 "  v = vec3(pos.xy * 0.5 + vec2(0.5), k);\n"
                                 "  gl_Position = vec4(pos, 1.0);\n"
                                 "}\n",
                                 "varying vec3 v;\n"
                                 "void main() { gl_FragColor = vec4(v, 1.0); }\n");
    if (!p)
        return 0;
    glUniform1f(glGetUniformLocation(p, "k"), 0.5f);
    const GLint loc = glGetAttribLocation(p, "pos");
    attrib_rect(loc, -0.9f, -0.9f, 0.9f, 0.9f, 0.0f);

    static uint32_t first[PROBE_W * PROBE_H];
    const uint32_t *s = scan_frame();
    for (int i = 0; i < PROBE_W * PROBE_H; i++)
        first[i] = s[i];

    glClear(GL_COLOR_BUFFER_BIT);
    attrib_rect(loc, -0.9f, -0.9f, 0.9f, 0.9f, 0.0f);
    s = scan_frame();
    for (int i = 0; i < PROBE_W * PROBE_H; i++) {
        if (s[i] != first[i])
            return 0;
    }
    /* Something was drawn, so two background frames do not pass. */
    return SCAN_PX(s, MID_X, MID_Y) != PROBE_BG && glGetError() == GL_NO_ERROR;
}

/* -------------------------------------------------------------------------
 * Throughput
 *
 * D014 makes throughput part of what complete means, so it needs an arm. Every check
 * above this one draws in immediate mode, which the resident vertex-buffer path
 * declines - so a suite of them passes whether the vertex stage runs on the GPU or is
 * interpreted per vertex on the CPU. This check is the one that can tell.
 * ------------------------------------------------------------------------- */

/* Two triangles a cell, over the middle of the region. The cells are a few pixels
 * across so the cost is the vertex stage's and not the rasteriser's, and the margin
 * around the grid stays background so a path that shades everything fails too. */
#define THRU_COLS 32
#define THRU_ROWS 32
#define THRU_TRIS (THRU_COLS * THRU_ROWS * 2)
#define THRU_VERTS (THRU_TRIS * 3)
#define THRU_LO (-0.75f)
#define THRU_HI 0.75f

/* What one triangle may cost on the console. The CPU vertex path D014 replaced ran
 * Craft at 150000ns a triangle; a compiled vertex stage is orders below that. The gate
 * is at 10000 so that it names a return to the interpreter rather than a slow frame,
 * and so that the snapshot's synchronisation - which is inside the measurement, since
 * a second glFinish would move the read to the next scanout buffer - cannot trip it. */
#define THRU_NS_PER_TRI 10000u

static float g_thru_verts[THRU_VERTS * 3];

static void thru_fill_grid(void) {
    const float dx = (THRU_HI - THRU_LO) / (float)THRU_COLS;
    const float dy = (THRU_HI - THRU_LO) / (float)THRU_ROWS;
    int v = 0;
    for (int r = 0; r < THRU_ROWS; r++) {
        for (int c = 0; c < THRU_COLS; c++) {
            const float x0 = THRU_LO + dx * (float)c;
            const float x1 = x0 + dx;
            const float y0 = THRU_LO + dy * (float)r;
            const float y1 = y0 + dy;
            const float xs[6] = {x0, x1, x1, x0, x1, x0};
            const float ys[6] = {y0, y0, y1, y0, y1, y1};
            for (int i = 0; i < 6; i++) {
                g_thru_verts[v++] = xs[i];
                g_thru_verts[v++] = ys[i];
                g_thru_verts[v++] = 0.0f;
            }
        }
    }
}

static int check_resident_throughput(void) {
    reset_view();
    const GLuint p =
        use_program("attribute vec3 pos;\n"
                    "void main() { gl_Position = vec4(pos, 1.0); }\n",
                    "void main() { gl_FragColor = vec4(0.0, 0.0, 1.0, 1.0); }\n");
    if (!p)
        return 0;
    const GLint loc = glGetAttribLocation(p, "pos");
    if (loc < 0)
        return 0;

    thru_fill_grid();

    /* A buffer object, not a client array: the resident path declines a client array,
     * and declining it is the regression this check exists to catch. */
    GLuint vbo = 0u;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(g_thru_verts), g_thru_verts,
                 GL_STATIC_DRAW);
    glVertexAttribPointer((GLuint)loc, 3, GL_FLOAT, GL_FALSE, 0, (const void *)0);
    glEnableVertexAttribArray((GLuint)loc);

#ifndef OOPS_HOST_BUILD
    const uint64_t t0 = oops_time_get_ns();
#endif
    glDrawArrays(GL_TRIANGLES, 0, THRU_VERTS);
    const uint32_t *s = scan_frame();
#ifndef OOPS_HOST_BUILD
    const uint64_t ns = oops_time_get_ns() - t0;
#endif

    /* Drawn, and drawn only where the grid is. */
    int ok = near_rgb(SCAN_PX(s, MID_X, MID_Y), 0, 0, 255, 2);
    ok = ok && SCAN_PX(s, 2, 2) == PROBE_BG;
    ok = ok && SCAN_PX(s, PROBE_W - 3, PROBE_H - 3) == PROBE_BG;

#ifndef OOPS_HOST_BUILD
    /* The vertex stage reached the console as machine code. Asked of the program
     * rather than of the clock, because it names the cause where a time names a
     * symptom. Zero here is the interpreter, whatever the measurement says. */
    GLint vs_words = 0;
    glGetProgramiv(p, GL_PROGRAM_HW_VS_WORDS, &vs_words);
    ok = ok && vs_words > 0;
    ok = ok && (ns / (uint64_t)THRU_TRIS) < (uint64_t)THRU_NS_PER_TRI;
#endif

    glDisableVertexAttribArray((GLuint)loc);
    glBindBuffer(GL_ARRAY_BUFFER, 0u);
    glDeleteBuffers(1, &vbo);
    return ok && glGetError() == GL_NO_ERROR;
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
    {"structs", check_structs},
    {"framebuffer-objects", check_framebuffer_objects},
    {"local-arrays", check_local_arrays},
    {"builtin-math", check_builtin_math},
    {"mod-and-int-divide", check_mod_is_floored},
    {"relational-builtins", check_relational_builtins},
    {"short-circuit", check_short_circuit},
    {"constructors", check_constructors},
    {"glsl-120", check_glsl_120},
    {"libultraship-dialect", check_libultraship_dialect},

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
    {"do-while", check_do_while},
    {"loop-uniformity", check_loop_uniformity},
    {"loop-spatial", check_loop_spatial},

    /* Throughput (D014) */
    {"resident-throughput", check_resident_throughput},
};

/* The suite fits in its callers' result array, so no check goes unrun. */
_Static_assert(sizeof(g_cases) / sizeof(g_cases[0]) <= GL2_PROBE_MAX_CASES,
               "more checks than GL2_PROBE_MAX_CASES; raise it in gl2_probe.h");

int gl2_probe_case_count(void) {
    return (int)(sizeof(g_cases) / sizeof(g_cases[0]));
}

const char *gl2_probe_case_name(int i) {
    if (i < 0 || i >= gl2_probe_case_count())
        return "";
    return g_cases[i].name;
}

int gl2_probe_run(gl2_probe_result_t *out, int max) {
    g_disp =
        oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, PROBE_DISPLAY_W, PROBE_DISPLAY_H);
    if (!g_disp)
        return -1;
    /* A display that is not ready is an error, not a NULL framebuffer. */
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
    /* A context defaults to 1.1 and has only its version's entry points, so the suite
     * claims 2.0 as a GL 2.0 program does. */
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
        /* Errors are cleared between checks so one failure cannot cascade. */
        (void)glGetError();
        out[i].name = g_cases[i].name;
        if (gl2_probe_trace)
            gl2_probe_trace(g_cases[i].name, -1);
        out[i].passed = g_cases[i].fn();
        /* The check's own first error, taken before anything below can raise one. */
        const GLenum err = glGetError();
        if (gl2_probe_trace)
            gl2_probe_trace(g_cases[i].name, out[i].passed);
        if (!out[i].passed && gl2_probe_saw) {
            /* Reported from `g_scan`, the snapshot the check compared, not a fresh
             * read: another `glFinish` lands on the next scanout buffer. `drawn` counts
             * the region's pixels that are not the reset colour. */
            int drawn = 0;
            for (int q = 0; q < PROBE_W * PROBE_H; q++) {
                if (g_scan[q] != PROBE_BG)
                    drawn++;
            }
            gl2_probe_saw(g_cases[i].name, SCAN_PX(g_scan, MID_X, MID_Y),
                          (unsigned int)err, drawn, SCAN_PX(g_scan, MID_X - 24, MID_Y),
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
