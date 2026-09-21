/*
 * mesa-cube - the example title for oops-mesa.
 *
 * # What this is
 *
 * The three titles beside this one are probes: each asks a question, prints the answer and stops.
 * This one asks nothing. It is roadmap unit 7's "example title", and its job is to be the file a
 * title author reads before writing their own - so it is deliberately ordinary. Create a GL
 * context, upload some geometry and a texture, and draw a frame in a loop until the host closes
 * the title.
 *
 * Everything here is plain OpenGL 3.3 core-profile practice. Nothing in it is specific to this
 * platform except three lines: `oops_gl_create` instead of a windowing library,
 * `oops_mesa_run_init_array` at the top, and parking instead of returning at the bottom. Each is
 * commented where it appears, because those three are the whole difference between this and the
 * same program on a desktop.
 *
 * # What it exercises, and why that matters right now
 *
 * `dri-probe` draws one untextured triangle with no depth buffer. That is the right shape for a
 * gate and the wrong shape for an example. This adds what a real title reaches for on day one and
 * nothing here had touched yet: a texture and a sampler, a depth buffer with the test enabled, an
 * element (index) buffer, a matrix pipeline feeding a uniform, and a frame loop that presents
 * continuously rather than once.
 *
 * Unit 8's CTS subset is blocked on a C++ standard library this repository does not have (worklog
 * 067), so until that question is answered this title is the broadest exercise of the GL stack
 * that exists here. That is a statement about coverage, not about conformance: **this is an
 * example, not a conformance test.** A suite written here to test this stack would prove nothing
 * that matters - the value of a CTS result is that the tests are Khronos's and a failure is an
 * upstream bug. Nothing in this directory should ever be described as conformance.
 *
 * Its second use is measurement. `oops_gl_present` reports its cost in four parts for the first
 * ten frames and every sixtieth after (oops-mesa D012), and a title that presents once produces a
 * single sample. This one produces a profile, which is what decides D012's open question: whether
 * the `glReadPixels` detile or the display's re-tile dominates the present.
 */

#include "oops/system.h"
#include "oops/time.h"

#include "oops_platform.h"

/*
 * GL's own headers. Included with the conversion warnings off for the reason the probes' are:
 * this title compiles at `-Wconversion -Wsign-conversion -Werror` and upstream's headers are not
 * ours to make clean. `GL_GLEXT_PROTOTYPES` plus `<GL/glext.h>` is what brings in everything past
 * GL 1.1 - the legacy `<GL/gl.h>` alone has no `glCreateShader`. The suppression covers the
 * includes and nothing after them.
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#pragma clang diagnostic pop

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define CUBE_WIDTH  1920u
#define CUBE_HEIGHT 1080u
#define TEX_N       64u

static void say(const char *msg)
{
    oops_klog("MESA-CUBE", msg);
}

/*
 * How a title finishes here, which is by not finishing.
 *
 * obSCEne `REQ-20260917T1450Z-2e71`: no userland call terminates a `big-app` process. `_exit`
 * raises `SIGSYS` for want of permission on syscall 1, and returning from the entry point faults
 * at `rip: 0x0` because the dynamic linker provides no caller frame. Printing a last line and
 * idling for the host to close the app is the conforming pattern, and it produces no coredump and
 * no hung GPU ring. This is one of the three lines that differ from the same program on a desktop.
 */
_Noreturn static void park(const char *why)
{
    say(why);
    oops_system_park_until_closed();
}

/* --- matrices ------------------------------------------------------------------------------
 *
 * Column-major, `m[column * 4 + row]`, which is what `glUniformMatrix4fv` expects with `transpose`
 * false. Written out here rather than pulled from a library because the example should not send a
 * reader looking for one, and because there is no maths library on this target beyond FreeBSD's
 * msun (oops-mesa D011), which is where `sinf`, `cosf` and `tanf` come from.
 *
 * **Checked on the host before this ever ran on the console**, because every way of getting these
 * wrong produces the same symptom - a black screen - and finding that out on hardware costs a
 * deploy, a launch and a manual close. These functions were compiled unmodified into a host
 * program that reproduced the frame loop's exact composition and transformed all eight cube
 * corners over 400 frames. It confirmed: `A*I == I*A == A`; the translation lands in the last
 * column (column-major, not transposed); both rotations have unit-length basis vectors and
 * determinant +1, so they rotate rather than mirror or scale; `mat4_mul(out, a, b)` means "apply
 * b, then a", verified by +X rotating to -Z about Y, which is right-handed; every corner stays in
 * front of the camera with `w > 0` and inside the frustum in all three axes for every frame
 * sampled; and the MVP actually differs between frames, so "animated" is not a claim about code
 * that produces a still image.
 *
 * At frame 0 the corner `(1, 1, 1)` comes out at clip `w = 5.0000`, NDC
 * `(0.2329, 0.4140, 0.9639)`. Those are the numbers to reproduce if any of this is ever edited.
 *
 * One consequence worth knowing rather than fixing: with `znear` at 0.1 and the cube five to seven
 * units away, the whole cube occupies NDC depth 0.96 to 0.98 - under 2% of the range, because a
 * perspective divide spends most of its depth precision near the eye. That is harmless here (the
 * chosen config has a 32-bit depth buffer, so even 2% is tens of millions of distinct values) and
 * it is why `znear` is not tuned. A scene that needed the precision would raise `znear`, not the
 * bit depth.
 */

static void mat4_identity(float m[16])
{
    for (int i = 0; i < 16; i++) {
        m[i] = 0.0f;
    }
    m[0] = 1.0f;
    m[5] = 1.0f;
    m[10] = 1.0f;
    m[15] = 1.0f;
}

static void mat4_mul(float out[16], const float a[16], const float b[16])
{
    float r[16];

    for (int c = 0; c < 4; c++) {
        for (int i = 0; i < 4; i++) {
            r[c * 4 + i] = a[0 * 4 + i] * b[c * 4 + 0] + a[1 * 4 + i] * b[c * 4 + 1] +
                           a[2 * 4 + i] * b[c * 4 + 2] + a[3 * 4 + i] * b[c * 4 + 3];
        }
    }
    for (int i = 0; i < 16; i++) {
        out[i] = r[i];
    }
}

static void mat4_perspective(float m[16], float fovy_rad, float aspect, float znear, float zfar)
{
    const float f = 1.0f / tanf(fovy_rad * 0.5f);

    for (int i = 0; i < 16; i++) {
        m[i] = 0.0f;
    }
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (zfar + znear) / (znear - zfar);
    m[11] = -1.0f;
    m[14] = (2.0f * zfar * znear) / (znear - zfar);
}

static void mat4_rotate_x(float m[16], float a)
{
    const float s = sinf(a);
    const float c = cosf(a);

    mat4_identity(m);
    m[5] = c;
    m[6] = s;
    m[9] = -s;
    m[10] = c;
}

static void mat4_rotate_y(float m[16], float a)
{
    const float s = sinf(a);
    const float c = cosf(a);

    mat4_identity(m);
    m[0] = c;
    m[2] = -s;
    m[8] = s;
    m[10] = c;
}

static void mat4_translate(float m[16], float x, float y, float z)
{
    mat4_identity(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
}

/* --- geometry and texture ------------------------------------------------------------------- */

/* Six faces, four corners each, as position (3) and texture coordinate (2). Four rather than the
 * shared eight because each corner needs a different texture coordinate per face. */
static const GLfloat k_cube[] = {
    /*  x      y      z      u     v */
    /* +Z */
    -1.0f, -1.0f,  1.0f,  0.0f, 0.0f,   1.0f, -1.0f,  1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f,  1.0f, 1.0f,  -1.0f,  1.0f,  1.0f,  0.0f, 1.0f,
    /* -Z */
     1.0f, -1.0f, -1.0f,  0.0f, 0.0f,  -1.0f, -1.0f, -1.0f,  1.0f, 0.0f,
    -1.0f,  1.0f, -1.0f,  1.0f, 1.0f,   1.0f,  1.0f, -1.0f,  0.0f, 1.0f,
    /* +X */
     1.0f, -1.0f,  1.0f,  0.0f, 0.0f,   1.0f, -1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f, -1.0f,  1.0f, 1.0f,   1.0f,  1.0f,  1.0f,  0.0f, 1.0f,
    /* -X */
    -1.0f, -1.0f, -1.0f,  0.0f, 0.0f,  -1.0f, -1.0f,  1.0f,  1.0f, 0.0f,
    -1.0f,  1.0f,  1.0f,  1.0f, 1.0f,  -1.0f,  1.0f, -1.0f,  0.0f, 1.0f,
    /* +Y */
    -1.0f,  1.0f,  1.0f,  0.0f, 0.0f,   1.0f,  1.0f,  1.0f,  1.0f, 0.0f,
     1.0f,  1.0f, -1.0f,  1.0f, 1.0f,  -1.0f,  1.0f, -1.0f,  0.0f, 1.0f,
    /* -Y */
    -1.0f, -1.0f, -1.0f,  0.0f, 0.0f,   1.0f, -1.0f, -1.0f,  1.0f, 0.0f,
     1.0f, -1.0f,  1.0f,  1.0f, 1.0f,  -1.0f, -1.0f,  1.0f,  0.0f, 1.0f,
};

/* A checkerboard, generated rather than loaded: this title has no data files, and a texture that
 * comes out of arithmetic is one less thing between the reader and the GL. */
static void make_checker(unsigned char *px)
{
    for (unsigned y = 0; y < TEX_N; y++) {
        for (unsigned x = 0; x < TEX_N; x++) {
            const unsigned on = ((x >> 3) ^ (y >> 3)) & 1u;
            unsigned char *p = px + ((size_t)y * (size_t)TEX_N + (size_t)x) * 4u;

            p[0] = on ? (unsigned char)235 : (unsigned char)45;
            p[1] = on ? (unsigned char)225 : (unsigned char)70;
            p[2] = on ? (unsigned char)120 : (unsigned char)170;
            p[3] = (unsigned char)255;
        }
    }
}

/* --- shaders --------------------------------------------------------------------------------
 *
 * Reported rather than assumed. A shader that fails to compile still gives a program that links to
 * nothing and draws nothing, and the frame that results is black with no error - so the status is
 * checked and the info log is said, which is how a GLSL problem is diagnosed on a console with no
 * debugger attached. The log is truncated because a klog line is dropped in silence past about 128
 * bytes (orbistoun worklog 539).
 */
static GLuint compile_shader(GLenum kind, const char *src, const char *label)
{
    const GLuint sh = glCreateShader(kind);
    GLint ok = 0;
    char msg[128];

    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

    if (ok == 0) {
        char info[96];
        GLsizei got = 0;

        info[0] = '\0';
        glGetShaderInfoLog(sh, (GLsizei)sizeof info, &got, info);
        info[sizeof info - 1] = '\0';
        (void)snprintf(msg, sizeof msg, "%s shader did not compile: %s", label, info);
        say(msg);
        return 0;
    }

    (void)snprintf(msg, sizeof msg, "%s shader compiled", label);
    say(msg);
    return sh;
}

static const char *const k_vs_src =
    "#version 330\n"
    "layout(location=0) in vec3 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "uniform mat4 u_mvp;\n"
    "out vec2 v_uv;\n"
    "out vec3 v_dir;\n"
    "void main(){ v_uv = a_uv; v_dir = a_pos; gl_Position = u_mvp * vec4(a_pos, 1.0); }\n";

static const char *const k_fs_src =
    "#version 330\n"
    "in vec2 v_uv;\n"
    "in vec3 v_dir;\n"
    "uniform sampler2D u_tex;\n"
    "out vec4 o_col;\n"
    "void main(){\n"
    "  vec3 t = texture(u_tex, v_uv).rgb;\n"
    "  float shade = 0.55 + 0.45 * abs(normalize(v_dir).y);\n"
    "  o_col = vec4(t * shade, 1.0);\n"
    "}\n";

/*
 * Run the C++ dynamic initialisers. A hosted title has no crt start-up object to walk
 * `.init_array`, and Mesa has globals that stay zeroed until something does - ACO's opcode table
 * `instr_info` among them, which left every emitted instruction with opcode 0 and faulted the GPU
 * (oops-mesa worklog 062). Defined in oops-mesa's runtime shim (`abi.c`). This is the second of
 * the three lines that differ from the same program on a desktop, and it must come first.
 */
extern void oops_mesa_run_init_array(void);

void mesa_cube_start(void);

void mesa_cube_start(void)
{
    oops_mesa_run_init_array();

    say("mesa-cube: a textured cube through upstream Mesa (v" OOPS_APP_VERSION ")");

    /* The third and last line that differs from a desktop: no windowing library, no EGL. The
     * extent is the title's to choose and the compositor scales; 1920x1080 matches the display's
     * own scanout size so the frame fills it without a promotion. */
    struct oops_gl *gl = oops_gl_create(CUBE_WIDTH, CUBE_HEIGHT);
    if (gl == NULL) {
        park("GL did not come up; the shim's last line above names the step");
    }

    uint32_t w = 0;
    uint32_t h = 0;
    oops_gl_extent(gl, &w, &h);

    const GLuint vs = compile_shader(GL_VERTEX_SHADER, k_vs_src, "vertex");
    const GLuint fs = compile_shader(GL_FRAGMENT_SHADER, k_fs_src, "fragment");
    if (vs == 0 || fs == 0) {
        park("a shader did not compile, so there is nothing to draw");
    }

    const GLuint prog = glCreateProgram();
    GLint linked = 0;
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (linked == 0) {
        park("the program did not link, so there is nothing to draw");
    }
    glUseProgram(prog);
    say("program linked");

    /* Geometry. The element buffer is built here rather than written out: each face is two
     * triangles over its four corners, which is the same six indices shifted by four each time. */
    GLushort idx[36];
    for (unsigned f = 0; f < 6u; f++) {
        const unsigned b = f * 4u;
        idx[f * 6u + 0u] = (GLushort)(b + 0u);
        idx[f * 6u + 1u] = (GLushort)(b + 1u);
        idx[f * 6u + 2u] = (GLushort)(b + 2u);
        idx[f * 6u + 3u] = (GLushort)(b + 0u);
        idx[f * 6u + 4u] = (GLushort)(b + 2u);
        idx[f * 6u + 5u] = (GLushort)(b + 3u);
    }

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof k_cube, k_cube, GL_STATIC_DRAW);
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)sizeof idx, idx, GL_STATIC_DRAW);

    const GLsizei stride = (GLsizei)(5u * sizeof(GLfloat));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (const void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                          (const void *)(3u * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    /* Texture. Mipmaps are generated because the cube is minified when it rotates away, and a
     * sampler left at its default `GL_NEAREST_MIPMAP_LINEAR` with no mip chain samples nothing. */
    static unsigned char checker[TEX_N * TEX_N * 4u];
    GLuint tex = 0;
    make_checker(checker);
    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)TEX_N, (GLsizei)TEX_N, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, checker);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    const GLint tex_loc = glGetUniformLocation(prog, "u_tex");
    const GLint mvp_loc = glGetUniformLocation(prog, "u_mvp");

    /* A missing uniform is -1, and GL accepts -1 silently - `glUniform*` on it is a no-op by
     * specification. So a mistyped name here would compile, link, draw, and produce a cube with no
     * transform and no texture rather than an error. Checked because a silent no-op on a console
     * with no debugger is the worst kind of failure to chase. */
    if (tex_loc < 0 || mvp_loc < 0) {
        park("a uniform location came back -1; the linked program is not the one this file expects");
    }
    glUniform1i(tex_loc, 0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glViewport(0, 0, (GLsizei)w, (GLsizei)h);
    glClearColor(0.06f, 0.07f, 0.10f, 1.0f);

    {
        char msg[128];
        (void)snprintf(msg, sizeof msg, "set up: %ux%u, depth test on, %u indices - drawing",
                       (unsigned)w, (unsigned)h, (unsigned)(sizeof idx / sizeof idx[0]));
        say(msg);
    }

    const float aspect = (h != 0u) ? ((float)w / (float)h) : 1.0f;
    const uint64_t t_first = oops_time_get_us();
    uint64_t t_window = t_first;

    /*
     * The angle accumulates and wraps, rather than being `frame * step`.
     *
     * `frame` never stops, because the title runs until the host closes it. A float angle derived
     * from it grows without bound, and `float` carries about seven significant digits - so after
     * an hour at 60 Hz the angle is past 2700 radians and its precision is coarser than the 0.0125
     * step being added to it. The rotation would quantise and judder, and it would get worse the
     * longer it ran, which is a poor look for the one title meant to be left running.
     *
     * 10*pi is the wrap point rather than 2*pi because the X tilt uses `a * 0.6`: at 10*pi the Y
     * spin has made five whole turns and the tilt exactly three, so both are back where they
     * started and the wrap is invisible. Wrapping at 2*pi would step the tilt by 1.2*pi and show.
     */
    const float a_wrap = 31.41592653589793f; /* 10*pi */
    float a = 0.0f;

    for (uint32_t frame = 0;; frame++) {
        float proj[16];
        float rx[16];
        float ry[16];
        float tr[16];
        float mv[16];
        float mvp[16];

        mat4_perspective(proj, 0.9f, aspect, 0.1f, 50.0f);
        mat4_rotate_x(rx, a * 0.6f);
        mat4_rotate_y(ry, a);
        mat4_translate(tr, 0.0f, 0.0f, -6.0f);
        mat4_mul(mv, rx, ry);   /* spin about Y, then tilt about X */
        mat4_mul(mv, tr, mv);   /* push it away from the eye */
        mat4_mul(mvp, proj, mv);
        glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, mvp);

        glClear((GLbitfield)(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
        glDrawElements(GL_TRIANGLES, (GLsizei)(sizeof idx / sizeof idx[0]), GL_UNSIGNED_SHORT,
                       (const void *)0);

        if (!oops_gl_present(gl)) {
            park("presentation refused; the shim's lines above name the step");
        }

        /* One line every 300 frames with the rate over that window. `oops_gl_present` reports
         * where the time inside a present goes (D012); this reports what the whole loop achieves,
         * which is the number a title author actually asks about. */
        if (frame != 0u && (frame % 300u) == 0u) {
            const uint64_t now = oops_time_get_us();
            const uint64_t span = now - t_window;
            char msg[128];

            t_window = now;
            (void)snprintf(msg, sizeof msg, "frame %u: %u us a frame over the last 300",
                           (unsigned)frame,
                           (unsigned)(span / 300u));
            say(msg);
        }

        a += 0.0125f;
        if (a >= a_wrap) {
            a -= a_wrap;
        }
    }
}
