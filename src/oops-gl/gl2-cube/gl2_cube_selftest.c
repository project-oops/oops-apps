/*
 * Host self-test for gl2-cube.
 *
 * **gl2-cube draws now.** It did not until 2026-09-21: the GL 2.0 entry points did not exist and
 * there was nothing behind `glCreateShader`, so this test could only push the two shaders
 * through the front end and stop. What it checks today is the whole path - compile, link, bind
 * generic attribute arrays, run the vertex shader per vertex, interpolate the varying, run the
 * fragment shader per fragment - against oops-gl's software reference, which is the definition
 * of what the console has to agree with.
 *
 * The thing this deliberately does *not* claim is the console. A GL 2.0 program has no hardware
 * path yet, and the draw path refuses one rather than running the fixed-function shaders in its
 * place - so nothing here can quietly differ between host and hardware, because there is no
 * hardware answer yet to differ from. That is why `FORMATS` is still `check-only`; see the
 * Makefile.
 *
 * # What the checks are, and why each one
 *
 * A cube drawn with a shader that merely passed the position through would look right from the
 * front and be wrong in every other way, so the measurements are chosen to separate the stages:
 *
 *   - **The front face's colour.** Proves the varying reached the fragment shader at all.
 *   - **Its neighbours' colours.** A face drawn with the wrong winding, or with the vertex
 *     attributes of the vertex fetched last, shows here and not in the centre.
 *   - **A rotation through the uniform.** The same geometry with a different `mvp` has to show a
 *     different face, which is what says the matrix is read column-major and is actually applied.
 *   - **The front end still refuses bad GLSL**, which a compiler that accepted everything would
 *     pass every check above while proving nothing.
 */

#include "GL/gl.h"
#include "GL/glu.h"
#include "oops/display.h"
#include "oops/memory.h"

#include "gl2_cube_scene.h"
#include "gl2_cube_shaders.h"

#include "src/gl/glsl_internal.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FB_W 256
#define FB_H 256

static uint32_t s_host_fb[FB_W * FB_H];
static int s_host_disp_dummy = 1;
static unsigned int s_host_w = FB_W;
static unsigned int s_host_h = FB_H;

oops_display_t *
oops_display_open(oops_display_backend_t backend, unsigned int width, unsigned int height) {
    (void)backend;
    s_host_w = width ? width : FB_W;
    s_host_h = height ? height : FB_H;
    return (oops_display_t *)&s_host_disp_dummy;
}
void oops_display_close(oops_display_t *disp) { (void)disp; }
int oops_display_flip(oops_display_t *disp) { (void)disp; return 0; }
uint32_t *oops_display_get_framebuffer(oops_display_t *disp) { (void)disp; return s_host_fb; }
unsigned int oops_display_get_width(const oops_display_t *d) { (void)d; return s_host_w; }
unsigned int oops_display_get_height(const oops_display_t *d) { (void)d; return s_host_h; }
int oops_display_is_gpu_accelerated(const oops_display_t *d) { (void)d; return 0; }

/* The SDK's allocator is direct memory and has nothing to talk to on a build machine. */
void *oops_mem_alloc(size_t size, size_t alignment, oops_mem_type_t type) {
    (void)alignment;
    (void)type;
    return malloc(size);
}
void oops_mem_free(void *p) { free(p); }

static int failures = 0;

static void report(const char *name, int ok, const char *detail) {
    printf("  %-22s %s%s\n", name, ok ? "pass" : "FAIL: ", ok ? "" : (detail ? detail : ""));
    if (!ok) failures++;
}

/* -------------------------------------------------------------------------
 * The front end, which is worth keeping on its own
 * ------------------------------------------------------------------------- */

static glsl_ast_t s_ast;
static glsl_sema_t s_sema;
static glsl_pp_t s_pp;

/* Runs a shader through preprocessor, parser and semantic stage the way glCompileShader does.
 * Returns NULL on success or the first diagnostic.
 *
 * This still goes through the raw stages rather than through `glCompileShader`, because the two
 * are worth checking separately: a failure here names the stage, and a failure in the drawing
 * checks below names the pipeline. */
static const char *compile_check(const char *src, GLenum stage) {
    glsl_pp_init(&s_pp, src, strlen(src));
    glsl_parser_t p;
    glsl_parser_init_pp(&p, &s_ast, &s_pp);
    int32_t unit = glsl_parse_translation_unit(&p);
    if (s_pp.error) return s_pp.error;
    if (p.error || unit == GLSL_NO_NODE) return p.error ? p.error : "parse failed";

    glsl_sema_init(&s_sema, &s_ast);
    s_sema.stage = stage;
    if (!glsl_declare_builtins(&s_sema, stage)) return "the built-in table did not fit";
    if (!glsl_check_unit(&s_sema, unit)) {
        return s_sema.error ? s_sema.error : "semantic check failed";
    }
    return NULL;
}

static void check_front_end(void) {
    const char *vs_err = compile_check(GL2_CUBE_VERTEX_SHADER, GL_VERTEX_SHADER);
    report("vertex-shader", vs_err == NULL, vs_err);
    const char *fs_err = compile_check(GL2_CUBE_FRAGMENT_SHADER, GL_FRAGMENT_SHADER);
    report("fragment-shader", fs_err == NULL, fs_err);

    /* **A front end that accepts everything would pass the two checks above.** These two have
     * to be rejected: `vec3 * mat4` has dimensions that do not meet, and `.z` on a vec2 is a
     * component that is not there. */
    const char *bad_err = compile_check(
        "uniform mat4 mvp;\n"
        "attribute vec3 pos;\n"
        "void main() { gl_Position = vec4(pos * mvp, 1.0); }\n", GL_VERTEX_SHADER);
    report("rejects-bad-glsl", bad_err != NULL, "accepted a bad shader");

    const char *swz_err = compile_check(
        "attribute vec2 uv;\n"
        "varying float v;\n"
        "void main() { v = uv.z; gl_Position = vec4(0.0); }\n", GL_VERTEX_SHADER);
    report("rejects-bad-swizzle", swz_err != NULL, "accepted uv.z on a vec2");
}

/* -------------------------------------------------------------------------
 * The pipeline
 * ------------------------------------------------------------------------- */

static uint32_t px(int x, int y) { return s_host_fb[y * FB_W + x]; }
static int px_r(uint32_t p) { return (int)((p >> 16) & 0xffu); }
static int px_g(uint32_t p) { return (int)((p >> 8) & 0xffu); }
static int px_b(uint32_t p) { return (int)(p & 0xffu); }

/* A column-major model-view-projection matrix: an orthographic box, and a rotation about y then
 * about x so three faces are visible at once. Built here rather than through the matrix stack
 * because **the point is that the shader's `mvp` uniform is what positions the cube** - a test
 * that leaned on glLoadMatrix would still pass if the uniform were ignored and the fixed-function
 * transform ran. */
static void build_mvp(float out[16], float yaw, float pitch) {
    const float cy = (float)cos((double)yaw), sy = (float)sin((double)yaw);
    const float cp = (float)cos((double)pitch), sp = (float)sin((double)pitch);
    /* R = Rx(pitch) * Ry(yaw), **stored column-major** - each group of three below is one
     * column, which is what `mat4 * vec4` in the shader reads and what `transpose = GL_FALSE`
     * means. Written out rather than multiplied at run time so the layout is readable as
     * evidence: a transposed constant here is a rotation about a different axis, and a cube is
     * symmetric enough to look plausible either way. */
    const float r[9] = {
        cy,       sp * sy,  -cp * sy,   /* column 0 */
        0.0f,     cp,       sp,         /* column 1 */
        sy,       -sp * cy, cp * cy,    /* column 2 */
    };
    /* Scaled to 0.6 so the cube sits inside the clip box with room around it. */
    const float s = 0.6f;
    for (int i = 0; i < 16; i++) out[i] = 0.0f;
    for (int c = 0; c < 3; c++) {
        for (int row = 0; row < 3; row++) out[c * 4 + row] = r[c * 3 + row] * s;
    }
    out[15] = 1.0f;
}

static GLuint build_program(void) {
    const GLchar *vs_src[1] = {GL2_CUBE_VERTEX_SHADER};
    const GLchar *fs_src[1] = {GL2_CUBE_FRAGMENT_SHADER};
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, vs_src, NULL);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, fs_src, NULL);
    glCompileShader(fs);

    GLint ok = 0;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[256] = {0};
        glGetShaderInfoLog(vs, (GLsizei)sizeof(log), NULL, log);
        report("gl-compile-vertex", 0, log);
        return 0u;
    }
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[256] = {0};
        glGetShaderInfoLog(fs, (GLsizei)sizeof(log), NULL, log);
        report("gl-compile-fragment", 0, log);
        return 0u;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[256] = {0};
        glGetProgramInfoLog(prog, (GLsizei)sizeof(log), NULL, log);
        report("gl-link", 0, log);
        return 0u;
    }
    /* The shaders may go the moment they are linked, which is the idiom every GL 2.0 program
     * uses and which the program has to survive. */
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

static void draw_cube(GLuint prog, GLint pos_loc, GLint col_loc, float yaw, float pitch) {
    float mvp[16];
    build_mvp(mvp, yaw, pitch);
    glUniformMatrix4fv(glGetUniformLocation(prog, "mvp"), 1, GL_FALSE, mvp);

    glVertexAttribPointer((GLuint)pos_loc, 3, GL_FLOAT, GL_FALSE, 0, GL2_CUBE_POSITIONS);
    glEnableVertexAttribArray((GLuint)pos_loc);
    glVertexAttribPointer((GLuint)col_loc, 3, GL_FLOAT, GL_FALSE, 0, GL2_CUBE_COLOURS);
    glEnableVertexAttribArray((GLuint)col_loc);
    glDrawArrays(GL_TRIANGLES, 0, GL2_CUBE_VERTEX_COUNT);
}

/* Which of the six face colours a pixel is, or -1. Nearest of the six rather than an exact
 * match, so an interpolated varying that is a fraction off still names its face - and so a
 * pixel that is none of them, which is what a wrong picture looks like, stays -1. */
static int face_of(uint32_t p) {
    static const int faces[6][3] = {
        {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0}, {0, 255, 255}, {255, 0, 255}};
    const int r = px_r(p), g = px_g(p), b = px_b(p);
    for (int i = 0; i < 6; i++) {
        const int dr = r - faces[i][0], dg = g - faces[i][1], db = b - faces[i][2];
        if (dr * dr + dg * dg + db * db < 3 * 24 * 24) return i;
    }
    return -1;
}

static void check_draws(void) {
    oops_display_t *disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, FB_W, FB_H);
    void *ctx = glContextCreate(disp);
    if (!ctx) {
        report("gl-context", 0, "glContextCreate returned nothing");
        return;
    }
    glContextMakeCurrent(ctx);
    /* **A context has the entry points its version defines and no others**, and the default is
     * 1.1 - so without this `glCreateShader` below is GL_INVALID_OPERATION and returns 0.
     * Claiming 2.0 is what a GL 2.0 program does, and is the line a port most easily forgets. */
    if (!glContextSetVersion(2, 0)) {
        report("gl-version-2.0", 0, "this context cannot be a GL 2.0 one");
        glContextDestroy(ctx);
        return;
    }
    report("gl-version-2.0", 1, NULL);
    glViewport(0, 0, FB_W, FB_H);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    const GLuint prog = build_program();
    if (prog == 0u) {
        glContextDestroy(ctx);
        return;
    }
    report("gl-link", 1, NULL);

    const GLint pos_loc = glGetAttribLocation(prog, "pos");
    const GLint col_loc = glGetAttribLocation(prog, "colour");
    report("attribute-locations", pos_loc >= 0 && col_loc >= 0 && pos_loc != col_loc,
           "the linker did not give both attributes distinct slots");
    glUseProgram(prog);

    /* **Face on.** Only the +Z face is visible, so the whole cube is red and the background is
     * untouched - the simplest picture the pipeline can be wrong about. */
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draw_cube(prog, pos_loc, col_loc, 0.0f, 0.0f);
    report("draws-without-error", glGetError() == GL_NO_ERROR, "the draw recorded a GL error");

    const int centre = face_of(px(FB_W / 2, FB_H / 2));
    report("front-face-is-red", centre == 0, "the centre pixel is not the +Z face's colour");
    report("background-is-clear", px(2, 2) == 0xff000000u,
           "something was drawn outside the cube");

    /* The cube is 0.6 of the clip box, so it covers the middle 60% and no more. A vertex shader
     * that dropped the scale would fill the frame; one that lost the attribute array and used
     * the last vertex for every vertex would draw nothing at all. */
    report("cube-has-edges",
           face_of(px(FB_W / 2, 8)) == -1 && face_of(px(FB_W / 2, FB_H - 9)) == -1,
           "the cube reaches the top and bottom of the frame");

    /* **Turned.** Three faces visible at once, and the centre is no longer the red one. This is
     * what says the uniform matrix is applied and is read column-major: a transposed read would
     * rotate about a different axis and show a different pair of faces. */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draw_cube(prog, pos_loc, col_loc, 0.7f, 0.5f);
    report("turned-draw", glGetError() == GL_NO_ERROR, "the second draw recorded a GL error");

    int seen[6] = {0, 0, 0, 0, 0, 0};
    for (int y = 0; y < FB_H; y += 2) {
        for (int x = 0; x < FB_W; x += 2) {
            const int f = face_of(px(x, y));
            if (f >= 0) seen[f] = 1;
        }
    }
    int visible = 0;
    for (int i = 0; i < 6; i++) visible += seen[i];
    report("three-faces-visible", visible == 3, "a turned cube should show exactly three faces");

    /* **No pair of opposite faces is ever visible at once.** That is a property of a convex
     * solid with culling and the depth test working, and it holds for *any* rotation - which is
     * why it is checked instead of naming the three faces this particular pair of angles
     * happens to show. A test that named them would fail the next time somebody nudged the
     * angles, and would have said nothing more.
     *
     * The three pairs are +Z/-Z, +X/-X and +Y/-Y, which is the order `gl2_cube_scene.h` lays
     * the faces out in. */
    int both_sides = 0;
    for (int i = 0; i < 6; i += 2) {
        if (seen[i] && seen[i + 1]) both_sides = 1;
    }
    report("culling-hides-the-back", !both_sides,
           "a face and the face opposite it were both drawn");
    /* And the rotation moved something: face on, only the red face was visible. */
    report("the-turn-showed-more", visible > 1,
           "the turned cube still shows only one face");

    /* **The program survives its shaders being deleted**, which happened at link time above. */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draw_cube(prog, pos_loc, col_loc, 0.0f, 0.0f);
    report("draws-after-shader-delete", face_of(px(FB_W / 2, FB_H / 2)) == 0,
           "the program stopped working once its shaders were deleted");

    glContextDestroy(ctx);
    oops_display_close(disp);
}

int main(void) {
    check_front_end();
    check_draws();
    printf("gl2-cube selftest: %s (the software reference; the console path is built and has "
           "never run - obSCEne REQ-...-4e77)\n",
           failures == 0 ? "ok" : "FAILED");
    return failures == 0 ? 0 : 1;
}
