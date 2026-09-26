/*
 * mesa-cube - the example title for oops-mesa.
 *
 * # What this is
 *
 * The two probes beside this one each ask a question, print the answer and stop. This
 * one asks nothing. It is roadmap unit 7's "example title", and its job is to be the
 * file a title author reads before writing their own - so it is deliberately ordinary.
 * Create a GL context, upload some geometry and a texture, draw a frame in a loop, read
 * the pad, and stop cleanly when asked.
 *
 * Everything here is plain OpenGL 3.3 core-profile practice, and everything a real
 * title needs on day one - a texture, a depth test, a matrix pipeline, a controller, a
 * way to stop - it reaches for through the SDK the same way any oops title would:
 * `oops/math.h` for the matrices, `oops/input.h` for the pad, `oops/fs.h` for the stop
 * file. Only three lines are specific to *this* platform in a way a desktop program
 * would not have: `oops_gfx_create` instead of a windowing library,
 * `oops_mesa_run_init_array` at the top, and parking instead of returning at the
 * bottom. Each is commented where it appears, because those three are the whole
 * difference between this and the same program on a desktop - the pad and the stop file
 * are not platform quirks, they are what the desktop version would do with GLFW and a
 * window-close event.
 *
 * # What it exercises, and why that matters right now
 *
 * `mesa-dri-probe` draws one untextured triangle with no depth buffer. That is the
 * right shape for a gate and the wrong shape for an example. This adds what a real
 * title reaches for on day one and nothing here had touched yet: a texture and a
 * sampler, a depth buffer with the test enabled, an element (index) buffer, a matrix
 * pipeline feeding a uniform, a frame loop that presents continuously rather than once,
 * and a 2D overlay drawn over the scene (`oops/hud.h`) - which on this stack is a
 * second, fixed-function pass composited over the title's own shaders, the only way to
 * put text on a frame whose buffer the CPU never touches.
 *
 * Unit 8's CTS subset is blocked on a C++ standard library this repository does not
 * have (worklog 067), so until that question is answered this title is the broadest
 * exercise of the GL stack that exists here. That is a statement about coverage, not
 * about conformance: **this is an example, not a conformance test.** A suite written
 * here to test this stack would prove nothing that matters - the value of a CTS result
 * is that the tests are Khronos's and a failure is an upstream bug. Nothing in this
 * directory should ever be described as conformance.
 *
 * Its second use is measurement. `oops_gfx_present` reports its cost in four parts for
 * the first ten frames and every sixtieth after (oops-mesa D012), and a title that
 * presents once produces a single sample. This one produces a profile over a continuous
 * loop, which is what settled D012: the present now scans out Mesa's own tiled buffer
 * directly, 59.94 fps vsync-locked with the CPU touching no pixel of the frame. A title
 * that presents once could never have shown that.
 */

#include "oops/system.h"
#include "oops/time.h"
#include "oops/math.h"  /* the SDK's matrix library, rather than a copy in this file */
#include "oops/input.h" /* the pad, so the cube answers a controller like a real title */
#include "oops/fs.h"    /* a stop file, so a headless run ends without JetKVM keys */
#include "oops/hud.h" /* the GPU overlay - text on the frame the same way the scene is drawn */
#include "cube_hud.h" /* the shared dashboard both cubes draw */

#include "oops/gfx.h"

/*
 * GL's own headers. Included with the conversion warnings off for the reason the
 * probes' are: this title compiles at `-Wconversion -Wsign-conversion -Werror` and
 * upstream's headers are not ours to make clean. `GL_GLEXT_PROTOTYPES` plus
 * `<GL/glext.h>` is what brings in everything past GL 1.1 - the legacy `<GL/gl.h>`
 * alone has no `glCreateShader`. The suppression covers the includes and nothing after
 * them.
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#pragma clang diagnostic pop

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define CUBE_WIDTH 1920u
#define CUBE_HEIGHT 1080u
#define TEX_N 64u

static void say(const char *msg) {
    oops_klog("MESA-CUBE", msg);
}

/*
 * How a title finishes here, which is by not finishing.
 *
 * obSCEne `REQ-20260917T1450Z-2e71`: no userland call terminates a `big-app` process.
 * `_exit` raises `SIGSYS` for want of permission on syscall 1, and returning from the
 * entry point faults at `rip: 0x0` because the dynamic linker provides no caller frame.
 * Printing a last line and idling for the host to close the app is the conforming
 * pattern, and it produces no coredump and no hung GPU ring. This is one of the three
 * lines that differ from the same program on a desktop.
 */
_Noreturn static void park(const char *why) {
    say(why);
    oops_system_park_until_closed();
}

/* --- matrices
 * ------------------------------------------------------------------------------
 *
 * From `oops/math.h`, not hand-rolled here. An earlier draft of this file carried its
 * own six matrix functions on the grounds that an example should not send a reader
 * looking for a library. That was the wrong call for this collection: a title author
 * writing an oops title *should* reach for `oops/math.h`, so the example that pretends
 * the library does not exist teaches the wrong habit. `oops_mat4_t` is a bare `float
 * m[16]`, column-major (`m[column * 4 + row]`), which is exactly what
 * `glUniformMatrix4fv` wants with `transpose` false - so `mvp.m` is handed straight to
 * the uniform.
 *
 * The SDK's rotate and translate multiply *into* their target (`out = out * op`), which
 * is the chaining form; to get a single fresh transform this file starts each from
 * identity. The one behavioural difference from the old code is the trig: `oops/math.h`
 * uses its own freestanding polynomial `sinf`/`cosf`/`tanf` rather than FreeBSD's msun.
 * It is not a maths library dependency this title has to think about, and it was
 * checked to matter nowhere it can be seen.
 *
 * **The frame-0 regression numbers are unchanged and were re-verified against the SDK
 * maths before the swap:** corner `(1, 1, 1)` at frame 0 lands at clip `w = 5.0000`,
 * NDC
 * `(0.2329, 0.4140, 0.9639)`. The largest NDC deviation from the old msun path over the
 * whole `a = 0 .. 10*pi` animation, across all eight corners, is `1.2e-06` - six
 * ten-thousandths of one 1080p pixel. Those are the numbers to reproduce if any of this
 * is ever edited.
 *
 * One consequence worth knowing rather than fixing: with `znear` at 0.1 and the cube
 * five to seven units away, the whole cube occupies NDC depth 0.96 to 0.98 - under 2%
 * of the range, because a perspective divide spends most of its depth precision near
 * the eye. That is harmless here (the chosen config has a 32-bit depth buffer, so even
 * 2% is tens of millions of distinct values) and it is why `znear` is not tuned. A
 * scene that needed the precision would raise `znear`, not the bit depth.
 */

/* --- geometry and texture
 * ------------------------------------------------------------------- */

/* Six faces, four corners each, as position (3) and texture coordinate (2). Four rather
 * than the shared eight because each corner needs a different texture coordinate per
 * face. */
static const GLfloat k_cube[] = {
    /*  x      y      z      u     v */
    /* +Z */
    -1.0f,
    -1.0f,
    1.0f,
    0.0f,
    0.0f,
    1.0f,
    -1.0f,
    1.0f,
    1.0f,
    0.0f,
    1.0f,
    1.0f,
    1.0f,
    1.0f,
    1.0f,
    -1.0f,
    1.0f,
    1.0f,
    0.0f,
    1.0f,
    /* -Z */
    1.0f,
    -1.0f,
    -1.0f,
    0.0f,
    0.0f,
    -1.0f,
    -1.0f,
    -1.0f,
    1.0f,
    0.0f,
    -1.0f,
    1.0f,
    -1.0f,
    1.0f,
    1.0f,
    1.0f,
    1.0f,
    -1.0f,
    0.0f,
    1.0f,
    /* +X */
    1.0f,
    -1.0f,
    1.0f,
    0.0f,
    0.0f,
    1.0f,
    -1.0f,
    -1.0f,
    1.0f,
    0.0f,
    1.0f,
    1.0f,
    -1.0f,
    1.0f,
    1.0f,
    1.0f,
    1.0f,
    1.0f,
    0.0f,
    1.0f,
    /* -X */
    -1.0f,
    -1.0f,
    -1.0f,
    0.0f,
    0.0f,
    -1.0f,
    -1.0f,
    1.0f,
    1.0f,
    0.0f,
    -1.0f,
    1.0f,
    1.0f,
    1.0f,
    1.0f,
    -1.0f,
    1.0f,
    -1.0f,
    0.0f,
    1.0f,
    /* +Y */
    -1.0f,
    1.0f,
    1.0f,
    0.0f,
    0.0f,
    1.0f,
    1.0f,
    1.0f,
    1.0f,
    0.0f,
    1.0f,
    1.0f,
    -1.0f,
    1.0f,
    1.0f,
    -1.0f,
    1.0f,
    -1.0f,
    0.0f,
    1.0f,
    /* -Y */
    -1.0f,
    -1.0f,
    -1.0f,
    0.0f,
    0.0f,
    1.0f,
    -1.0f,
    -1.0f,
    1.0f,
    0.0f,
    1.0f,
    -1.0f,
    1.0f,
    1.0f,
    1.0f,
    -1.0f,
    -1.0f,
    1.0f,
    0.0f,
    1.0f,
};

/* A checkerboard, generated rather than loaded: this title has no data files, and a
 * texture that comes out of arithmetic is one less thing between the reader and the GL.
 */
static void make_checker(unsigned char *px) {
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

/* --- shaders
 * --------------------------------------------------------------------------------
 *
 * Reported rather than assumed. A shader that fails to compile still gives a program
 * that links to nothing and draws nothing, and the frame that results is black with no
 * error - so the status is checked and the info log is said, which is how a GLSL
 * problem is diagnosed on a console with no debugger attached. The log is truncated
 * because a klog line is dropped in silence past about 128 bytes (orbistoun worklog
 * 539).
 */
static GLuint compile_shader(GLenum kind, const char *src, const char *label) {
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

static const char *const k_vs_src = "#version 330\n"
                                    "layout(location=0) in vec3 a_pos;\n"
                                    "layout(location=1) in vec2 a_uv;\n"
                                    "uniform mat4 u_mvp;\n"
                                    "out vec2 v_uv;\n"
                                    "out vec3 v_dir;\n"
                                    "void main(){ v_uv = a_uv; v_dir = a_pos; "
                                    "gl_Position = u_mvp * vec4(a_pos, 1.0); }\n";

static const char *const k_fs_src =
    "#version 330\n"
    "in vec2 v_uv;\n"
    "in vec3 v_dir;\n"
    "uniform sampler2D u_tex;\n"
    "uniform int u_textured;\n" /* pad toggle: sample the texture, or a flat colour */
    "uniform int u_lit;\n"      /* pad toggle: face shading, or full brightness */
    "out vec4 o_col;\n"
    "void main(){\n"
    "  vec3 base = (u_textured != 0) ? texture(u_tex, v_uv).rgb : vec3(0.80, 0.80, "
    "0.86);\n"
    "  float shade = (u_lit != 0) ? (0.55 + 0.45 * abs(normalize(v_dir).y)) : 1.0;\n"
    "  o_col = vec4(base * shade, 1.0);\n"
    "}\n";

/*
 * Run the C++ dynamic initialisers. A hosted title has no crt start-up object to walk
 * `.init_array`, and Mesa has globals that stay zeroed until something does - ACO's
 * opcode table `instr_info` among them, which left every emitted instruction with
 * opcode 0 and faulted the GPU (oops-mesa worklog 062). Defined in oops-mesa's runtime
 * shim (`abi.c`). This is the second of the three lines that differ from the same
 * program on a desktop, and it must come first.
 */
extern void oops_mesa_run_init_array(void);

void mesa_cube_start(void);

void mesa_cube_start(void) {
    oops_mesa_run_init_array();

    say("mesa-cube: a textured cube through upstream Mesa (v" OOPS_APP_VERSION ")");

    /* The third and last line that differs from a desktop: no windowing library, no
     * EGL, just the one renderer call (`oops/gfx.h`) that opens the display and brings
     * up the context together. The extent is the title's to choose and the compositor
     * scales; 1920x1080 matches the display's own scanout size so the frame fills it
     * without a promotion. The same call, and the same source below it, builds against
     * oops-gl instead by a switch - here it resolves to the Mesa backend because this
     * title links oops-mesa. */
    oops_gfx_t *gfx = oops_gfx_create(&(oops_gfx_desc_t){
        .width = CUBE_WIDTH, .height = CUBE_HEIGHT, .depth = true, .vsync = true});
    if (gfx == NULL) {
        park("GL did not come up; the shim's last line above names the step");
    }

    uint32_t w = 0;
    uint32_t h = 0;
    oops_gfx_extent(gfx, &w, &h);

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

    /* Geometry. The element buffer is built here rather than written out: each face is
     * two triangles over its four corners, which is the same six indices shifted by
     * four each time. */
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

    /* Texture. Mipmaps are generated because the cube is minified when it rotates away,
     * and a sampler left at its default `GL_NEAREST_MIPMAP_LINEAR` with no mip chain
     * samples nothing. */
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
    const GLint textured_loc = glGetUniformLocation(prog, "u_textured");
    const GLint lit_loc = glGetUniformLocation(prog, "u_lit");

    /* A missing uniform is -1, and GL accepts -1 silently - `glUniform*` on it is a
     * no-op by specification. So a mistyped name here would compile, link, draw, and
     * produce a cube with no transform and no texture rather than an error. Checked
     * because a silent no-op on a console with no debugger is the worst kind of failure
     * to chase. */
    if (tex_loc < 0 || mvp_loc < 0 || textured_loc < 0 || lit_loc < 0) {
        park("a uniform location came back -1; the linked program is not the one this "
             "file expects");
    }
    glUniform1i(tex_loc, 0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glViewport(0, 0, (GLsizei)w, (GLsizei)h);
    glClearColor(0.06f, 0.07f, 0.10f, 1.0f);

    /* The pad. A real title reads input, so the example does too - and its absence is
     * not an error, because a run left going on its own has no controller.
     * `oops_input_init` reports its own pad setup to the log; the loop below asks it
     * for state each frame and does not care whether one is connected. */
    oops_input_init();

    /* Cooperate with the dashboard's Close (oops/system.h): the handler sets a flag the
     * loop checks, so the title stops drawing and quiesces before the system kills it,
     * rather than being killed mid-frame. A hosted title cannot exit, but a quiesced
     * one is killed cleanly. */
    oops_system_install_close_handler();

    /* The overlay. It draws the frame counter and controls on top of the cube through
     * the GPU, which is the only way that works here: the present scans out Mesa's
     * tiled buffer directly (oops-mesa D012), so there is no CPU-addressable surface
     * for `oops_draw_text` to write into. A NULL result is not fatal - the cube still
     * runs, just without the readout. */
    oops_hud_t *hud = oops_hud_create((int)w, (int)h);
    if (hud == NULL) {
        say("the overlay did not come up; drawing the cube without it");
    }

    {
        char msg[128];
        (void)snprintf(
            msg, sizeof msg, "set up: %ux%u, depth test on, %u indices - drawing",
            (unsigned)w, (unsigned)h, (unsigned)(sizeof idx / sizeof idx[0]));
        say(msg);
    }

    const float aspect = (h != 0u) ? ((float)w / (float)h) : 1.0f;
    const uint64_t t_first = oops_time_get_us();
    uint64_t t_window = t_first;

    /*
     * The angle accumulates and wraps, rather than being `frame * step`.
     *
     * `frame` never stops, because the title runs until the host closes it. A float
     * angle derived from it grows without bound, and `float` carries about seven
     * significant digits - so after an hour at 60 Hz the angle is past 2700 radians and
     * its precision is coarser than the 0.0125 step being added to it. The rotation
     * would quantise and judder, and it would get worse the longer it ran, which is a
     * poor look for the one title meant to be left running.
     *
     * 10*pi is the wrap point rather than 2*pi because the X tilt uses `a * 0.6`: at
     * 10*pi the Y spin has made five whole turns and the tilt exactly three, so both
     * are back where they started and the wrap is invisible. Wrapping at 2*pi would
     * step the tilt by 1.2*pi and show.
     */
    const float a_wrap = 31.41592653589793f; /* 10*pi */
    float a = 0.0f;

    /* Loop-persistent input state. `prev_buttons` is what makes CROSS a toggle rather
     * than a per-frame flip: a press is a bit set now that was clear last frame, not a
     * bit that is merely held. */
    uint32_t prev_buttons = 0u;
    bool paused = false;
    bool stopped = false;

    /* Live pipeline toggles, driven by the pad - the same set gl1-cube has, so the two
     * demos behave the same. They start matching the enables set just above. */
    bool opt_texture = true;
    bool opt_depth = true;
    bool opt_cull = true;
    bool opt_lit = true;

    /* The per-frame microseconds from the last 300-frame window, for the HUD. Zero
     * until the first window closes, which the readout shows as "measuring" rather than
     * a nonsense rate. */
    unsigned last_us = 0u;

    for (uint32_t frame = 0; !stopped; frame++) {
        oops_mat4_t proj;
        oops_mat4_t rx;
        oops_mat4_t ry;
        oops_mat4_t tr;
        oops_mat4_t mv;
        oops_mat4_t mvp;

        /* Input, once a frame. Options or Circle stops the title the polite way - the
         * loop ends and the code below parks - and CROSS pauses the spin so a still
         * frame can be looked at. A disconnected or absent pad simply reports nothing
         * and the cube keeps turning. */
        oops_pad_state_t pad;
        if (oops_input_poll(0, &pad) == 0 && pad.connected) {
            const uint32_t pressed = pad.buttons & ~prev_buttons;
            prev_buttons = pad.buttons;

            if (pad.buttons & (OOPS_BUTTON_OPTIONS | OOPS_BUTTON_CIRCLE)) {
                stopped = true;
            }
            if (pressed & OOPS_BUTTON_CROSS) {
                paused = !paused;
            }
            if (pressed & OOPS_BUTTON_TRIANGLE) {
                opt_texture = !opt_texture;
            }
            if (pressed & OOPS_BUTTON_SQUARE) {
                opt_depth = !opt_depth;
            }
            if (pressed & OOPS_BUTTON_R1) {
                opt_cull = !opt_cull;
            }
            if (pressed & OOPS_BUTTON_L1) {
                opt_lit = !opt_lit;
            }
        }

        /* A stop file, checked each frame. It is how a run with nobody at the console
         * ends: `pros sh touch /app0/stop` before or during the run, and the loop
         * leaves cleanly rather than needing the app closed from the host. The gl-cube
         * demo uses the same path. */
        if (oops_fs_exists("/app0/stop")) {
            stopped = true;
        }

        /* The dashboard asked to close. Leave the loop; the teardown below stops GPU
         * submission and closes the display, so the process is idle when the system
         * kills it. */
        if (oops_system_close_requested()) {
            stopped = true;
        }

        oops_mat4_perspective(&proj, 0.9f, aspect, 0.1f, 50.0f);
        /* Each transform starts from identity because the SDK's rotate/translate
         * multiply into their target; from identity, `out = I * op` is just `op`. */
        oops_mat4_identity(&rx);
        oops_mat4_rotate(&rx, a * 0.6f, 1.0f, 0.0f, 0.0f);
        oops_mat4_identity(&ry);
        oops_mat4_rotate(&ry, a, 0.0f, 1.0f, 0.0f);
        oops_mat4_identity(&tr);
        oops_mat4_translate(&tr, 0.0f, 0.0f, -6.0f);
        oops_mat4_mul(&mv, &rx, &ry); /* spin about Y, then tilt about X */
        oops_mat4_mul(&mv, &tr, &mv); /* push it away from the eye */
        oops_mat4_mul(&mvp, &proj, &mv);
        glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, mvp.m);

        /* Apply the pad toggles: two are pipeline state, two are shader uniforms. */
        if (opt_depth) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
        if (opt_cull) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
        glUniform1i(textured_loc, opt_texture ? 1 : 0);
        glUniform1i(lit_loc, opt_lit ? 1 : 0);

        glClear((GLbitfield)(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
        glDrawElements(GL_TRIANGLES, (GLsizei)(sizeof idx / sizeof idx[0]),
                       GL_UNSIGNED_SHORT, (const void *)0);

        /* The overlay - the same dashboard gl1-cube draws (common/cube_hud.c), fed this
         * title's values. It draws after the scene and before the present, and
         * saves/restores the GL state it touches (the bound shader program included),
         * so the cube's pipeline is untouched next frame. */
        oops_cube_hud_draw(
            hud,
            &(oops_cube_hud_t){
                .title = "MESA CUBE",
                .backend = oops_gfx_backend_name(),
                .api = "OpenGL 3.3",
                .build = OOPS_APP_VERSION,
                .width = (unsigned)w,
                .height = (unsigned)h,
                .frame = (unsigned)frame,
                .us_per_frame = last_us,
                .paused = paused,
                .mesh = "cube",
                .tris = 12u,
                .verts = 24u,
                .cull = opt_cull,
                .depth = opt_depth,
                .texture = opt_texture,
                .lighting = opt_lit,
                .status = "direct scanout - vsync-locked - 0 CPU px",
                .status_color = 0xFF44FF88u,
                .controls = "X pause  /_\\ tex  [] depth  R1 cull  L1 light  (O) quit",
            });

        if (!oops_gfx_present(gfx)) {
            park("presentation refused; the shim's lines above name the step");
        }

        /* One line every 300 frames with the rate over that window. `oops_gl_present`
         * reports where the time inside a present goes (D012); this reports what the
         * whole loop achieves, which is the number a title author actually asks about.
         */
        if (frame != 0u && (frame % 300u) == 0u) {
            const uint64_t now = oops_time_get_us();
            const uint64_t span = now - t_window;
            char msg[128];

            t_window = now;
            last_us = (unsigned)(span / 300u); /* also what the HUD reads */
            (void)snprintf(msg, sizeof msg, "frame %u: %u us a frame over the last 300",
                           (unsigned)frame, last_us);
            say(msg);
        }

        /* Held still while paused. `frame` still advances, so the rate line keeps
         * reporting. */
        if (!paused) {
            a += 0.0125f;
            if (a >= a_wrap) {
                a -= a_wrap;
            }
        }
    }

    /* Quiesce before parking: stop GPU submission and close the display so that when
     * the system kills this process (a hosted big-app cannot exit itself - obSCEne
     * `REQ-20260917T1450Z-2e71`) the kill lands on an idle process rather than one
     * mid-frame, which is the difference between a clean dashboard Close and a crash.
     * The overlay first, then the renderer. */
    oops_hud_destroy(hud);
    oops_gfx_destroy(gfx);

    /* Parked, not returned: this is a `big-app`, and there is no caller frame to return
     * into (the comment on `park` above). Parking idles until the system kills it. */
    park("stopped - torn down and idle, awaiting the system close");
}
