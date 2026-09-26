/*
 * mesa-cube - the example title for oops-mesa.
 *
 * Plain OpenGL 3.3 core-profile practice: a texture and a sampler, a depth test, an
 * element buffer, a matrix pipeline feeding a uniform, a frame loop that presents every
 * frame, the pad, and a GPU overlay drawn over the scene (oops/hud.h). Three things
 * differ from the same program on a desktop, and each is commented where it appears:
 * oops_mesa_run_init_array at the top, oops_gfx_create instead of a windowing library,
 * and parking instead of returning at the end. It is an example, not a conformance
 * test.
 */

#include "cube_hud.h"
#include "oops/fs.h"
#include "oops/gfx.h"
#include "oops/hud.h"
#include "oops/input.h"
#include "oops/math.h"
#include "oops/system.h"
#include "oops/time.h"

/* GL's own headers, with the conversion warnings off for them only: this title compiles
 * at -Wconversion -Wsign-conversion -Werror and upstream's headers are not ours.
 * GL_GLEXT_PROTOTYPES with <GL/glext.h> brings in everything past GL 1.1. */
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

#define TAG "MESA-CUBE"
#define CUBE_WIDTH 1920u
#define CUBE_HEIGHT 1080u
#define TEX_N 64u
#define INDEX_COUNT 36u

/* 10*pi: the Y spin has made five turns and the X tilt (a * 0.6) three, so wrapping the
 * angle here is invisible, and a float angle that never wraps loses precision. */
#define ANGLE_WRAP 31.41592653589793f

/* Pad-driven pipeline toggles, the same set gl1-cube has. */
typedef struct cube_opts {
    bool texture, depth, cull, lit, paused;
} cube_opts_t;

typedef struct cube {
    oops_gfx_t *gfx;
    oops_hud_t *hud;
    uint32_t w, h;
    GLint mvp_loc, textured_loc, lit_loc;
    cube_opts_t opt;
    uint32_t prev_buttons;
    unsigned last_us; /* per frame over the last 300-frame window, for the HUD */
    uint64_t t_window;
    float angle;
} cube_t;

/* A big-app process cannot end itself: _exit is refused and returning from the entry
 * point has no caller frame. It logs a last line and idles until the host closes it
 * (oops_system_park_until_closed). */
_Noreturn static void park(const char *why) {
    oops_log_info(TAG, "%s", why);
    oops_system_park_until_closed();
}

/* Six faces, four corners each, as position (3) and texture coordinate (2): each corner
 * needs a different texture coordinate per face. */
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

/* A checkerboard generated in memory: this title has no data files. */
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

/* The status and info log are reported, since a shader that fails to compile still
 * links to a program that draws nothing. The line is kept under 128 bytes, past which
 * the system log drops it. */
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
        oops_log_info(TAG, "%s", msg);
        return 0;
    }
    oops_log_info(TAG, "%s shader compiled", label);
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

static GLuint build_program(void) {
    const GLuint vs = compile_shader(GL_VERTEX_SHADER, k_vs_src, "vertex");
    const GLuint fs = compile_shader(GL_FRAGMENT_SHADER, k_fs_src, "fragment");
    if (vs == 0 || fs == 0)
        park("a shader did not compile, so there is nothing to draw");

    const GLuint prog = glCreateProgram();
    GLint linked = 0;
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (linked == 0)
        park("the program did not link, so there is nothing to draw");
    glUseProgram(prog);
    oops_log_info(TAG, "program linked");
    return prog;
}

/* The vertex array, the vertex buffer and the element buffer: two triangles per face
 * over its four corners. */
static void upload_geometry(void) {
    GLushort idx[INDEX_COUNT];
    for (unsigned f = 0; f < 6u; f++) {
        const unsigned b = f * 4u;
        idx[f * 6u + 0u] = (GLushort)(b + 0u);
        idx[f * 6u + 1u] = (GLushort)(b + 1u);
        idx[f * 6u + 2u] = (GLushort)(b + 2u);
        idx[f * 6u + 3u] = (GLushort)(b + 0u);
        idx[f * 6u + 4u] = (GLushort)(b + 2u);
        idx[f * 6u + 5u] = (GLushort)(b + 3u);
    }

    GLuint vao = 0, vbo = 0, ebo = 0;
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
}

/* Mipmapped, because the cube is minified as it turns and the default minification
 * filter samples nothing without a mip chain. */
static void upload_texture(void) {
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
}

/* A missing uniform is -1 and glUniform* on -1 is silently a no-op, so each location is
 * checked. */
static void find_uniforms(cube_t *c, GLuint prog) {
    const GLint tex_loc = glGetUniformLocation(prog, "u_tex");
    c->mvp_loc = glGetUniformLocation(prog, "u_mvp");
    c->textured_loc = glGetUniformLocation(prog, "u_textured");
    c->lit_loc = glGetUniformLocation(prog, "u_lit");
    if (tex_loc < 0 || c->mvp_loc < 0 || c->textured_loc < 0 || c->lit_loc < 0)
        park("a uniform location came back -1; the linked program is not the one this "
             "file expects");
    glUniform1i(tex_loc, 0);
}

static void cube_init(cube_t *c) {
    /* A hosted title has no crt start-up object to walk .init_array, and Mesa's
     * globals (ACO's opcode table among them) stay zeroed until something does. Defined
     * in oops-mesa's runtime shim; it must come first. */
    extern void oops_mesa_run_init_array(void);
    oops_mesa_run_init_array();
    oops_log_info(TAG,
                  "mesa-cube: a textured cube through upstream Mesa (v" OOPS_APP_VERSION
                  ")");

    /* No windowing library and no EGL: one renderer call opens the display and brings
     * up the context (oops/gfx.h). 1920x1080 is the display's own scanout size. */
    c->gfx = oops_gfx_create(&(oops_gfx_desc_t){
        .width = CUBE_WIDTH, .height = CUBE_HEIGHT, .depth = true, .vsync = true});
    if (c->gfx == NULL)
        park("GL did not come up; the shim's last line above names the step");
    oops_gfx_extent(c->gfx, &c->w, &c->h);

    const GLuint prog = build_program();
    upload_geometry();
    upload_texture();
    find_uniforms(c, prog);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glViewport(0, 0, (GLsizei)c->w, (GLsizei)c->h);
    glClearColor(0.06f, 0.07f, 0.10f, 1.0f);

    /* An absent pad is not an error: a run left going on its own has none. */
    oops_input_init();
    /* The dashboard's Close sets a flag the loop checks, so the title quiesces before
     * the system kills it (oops/system.h). */
    oops_system_install_close_handler();

    /* The overlay is drawn by the GPU: the present scans out Mesa's tiled buffer
     * directly, so there is no CPU surface to write text into. */
    c->hud = oops_hud_create((int)c->w, (int)c->h);
    if (c->hud == NULL)
        oops_log_info(TAG, "the overlay did not come up; drawing the cube without it");

    oops_log_info(TAG, "set up: %ux%u, depth test on, %u indices - drawing",
                  (unsigned)c->w, (unsigned)c->h, INDEX_COUNT);
    c->opt = (cube_opts_t){.texture = true, .depth = true, .cull = true, .lit = true};
    c->t_window = oops_time_get_us();
}

/* The pad, the stop file and the dashboard's Close. Returns false when the run should
 * end. Options or Circle stops, Cross pauses the spin. */
static bool cube_input(cube_t *c) {
    bool stopped = false;
    oops_pad_state_t pad;
    if (oops_input_poll(0, &pad) == 0 && pad.connected) {
        const uint32_t pressed = pad.buttons & ~c->prev_buttons;
        c->prev_buttons = pad.buttons;
        if (pad.buttons & (OOPS_BUTTON_OPTIONS | OOPS_BUTTON_CIRCLE))
            stopped = true;
        if (pressed & OOPS_BUTTON_CROSS)
            c->opt.paused = !c->opt.paused;
        if (pressed & OOPS_BUTTON_TRIANGLE)
            c->opt.texture = !c->opt.texture;
        if (pressed & OOPS_BUTTON_SQUARE)
            c->opt.depth = !c->opt.depth;
        if (pressed & OOPS_BUTTON_R1)
            c->opt.cull = !c->opt.cull;
        if (pressed & OOPS_BUTTON_L1)
            c->opt.lit = !c->opt.lit;
    }
    /* `pros sh touch /app0/stop` ends a run with nobody at the console. */
    if (oops_fs_exists("/app0/stop"))
        stopped = true;
    if (oops_system_close_requested())
        stopped = true;
    return !stopped;
}

static void draw_scene(const cube_t *c) {
    const float aspect = (c->h != 0u) ? ((float)c->w / (float)c->h) : 1.0f;
    oops_mat4_t proj, rx, ry, tr, mv, mvp;

    oops_mat4_perspective(&proj, 0.9f, aspect, 0.1f, 50.0f);
    /* oops/math.h's rotate and translate multiply into their target, so each starts
     * from identity. */
    oops_mat4_identity(&rx);
    oops_mat4_rotate(&rx, c->angle * 0.6f, 1.0f, 0.0f, 0.0f);
    oops_mat4_identity(&ry);
    oops_mat4_rotate(&ry, c->angle, 0.0f, 1.0f, 0.0f);
    oops_mat4_identity(&tr);
    oops_mat4_translate(&tr, 0.0f, 0.0f, -6.0f);
    oops_mat4_mul(&mv, &rx, &ry); /* spin about Y, then tilt about X */
    oops_mat4_mul(&mv, &tr, &mv); /* push it away from the eye */
    oops_mat4_mul(&mvp, &proj, &mv);
    glUniformMatrix4fv(c->mvp_loc, 1, GL_FALSE, mvp.m);

    if (c->opt.depth)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (c->opt.cull)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    glUniform1i(c->textured_loc, c->opt.texture ? 1 : 0);
    glUniform1i(c->lit_loc, c->opt.lit ? 1 : 0);

    glClear((GLbitfield)(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    glDrawElements(GL_TRIANGLES, (GLsizei)INDEX_COUNT, GL_UNSIGNED_SHORT,
                   (const void *)0);
}

/* The dashboard gl1-cube draws (common/cube_hud.c). It saves and restores the GL state
 * it touches, the bound program included. */
static void draw_hud(const cube_t *c, uint32_t frame) {
    oops_cube_hud_draw(
        c->hud, &(oops_cube_hud_t){
                    .title = "MESA CUBE",
                    .backend = oops_gfx_backend_name(),
                    .api = "OpenGL 3.3",
                    .build = OOPS_APP_VERSION,
                    .width = (unsigned)c->w,
                    .height = (unsigned)c->h,
                    .frame = (unsigned)frame,
                    .us_per_frame = c->last_us,
                    .paused = c->opt.paused,
                    .mesh = "cube",
                    .tris = 12u,
                    .verts = 24u,
                    .cull = c->opt.cull,
                    .depth = c->opt.depth,
                    .texture = c->opt.texture,
                    .lighting = c->opt.lit,
                    .status = "direct scanout - vsync-locked - 0 CPU px",
                    .status_color = 0xFF44FF88u,
                    .controls = "X pause  /_\\ tex  [] depth  R1 cull  L1 light  "
                                "(O) quit",
                });
}

/* One line every 300 frames with the loop's rate over that window. */
static void log_rate(cube_t *c, uint32_t frame) {
    if (frame == 0u || (frame % 300u) != 0u)
        return;
    const uint64_t now = oops_time_get_us();
    const uint64_t span = now - c->t_window;
    c->t_window = now;
    c->last_us = (unsigned)(span / 300u);
    oops_log_info(TAG, "frame %u: %u us a frame over the last 300", (unsigned)frame,
                  c->last_us);
}

void mesa_cube_start(void);

void mesa_cube_start(void) {
    static cube_t cube;
    cube_init(&cube);

    bool running = true;
    for (uint32_t frame = 0; running; frame++) {
        running = cube_input(&cube);
        draw_scene(&cube);
        draw_hud(&cube, frame);
        if (!oops_gfx_present(cube.gfx))
            park("presentation refused; the shim's lines above name the step");
        log_rate(&cube, frame);
        if (!cube.opt.paused) {
            cube.angle += 0.0125f;
            if (cube.angle >= ANGLE_WRAP)
                cube.angle -= ANGLE_WRAP;
        }
    }

    /* Quiesced before parking, so the system's kill lands on an idle process: the
     * overlay first, then the renderer. */
    oops_hud_destroy(cube.hud);
    oops_gfx_destroy(cube.gfx);
    park("stopped - torn down and idle, awaiting the system close");
}
