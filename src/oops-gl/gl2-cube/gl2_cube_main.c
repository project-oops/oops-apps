/*
 * gl2-cube: the same cube as gl1-cube, drawn by a GL 2.0 shader program through
 * oops-gl's own draw path.
 *
 * It sits in the same place at the same angles as gl1-cube, so the two titles differ in
 * one thing - fixed function against a shader program - and a difference in what they
 * draw is a difference in that.
 */

#include "GL/gl.h"
#include "cube_frame.h"
#include "gl2_cube_scene.h"
#include "gl2_cube_shaders.h"
#include "oops/display.h"
#include "oops/draw.h"
#include "oops/freestd.h"
#include "oops/fs.h"
#include "oops/gfx.h"
#include "oops/input.h"
#include "oops/math.h"
#include "oops/memory.h"
#include "oops/syscall.h"
#include "oops/system.h"
#include "oops/time.h"
#include <stdbool.h>

#ifndef OOPS_APP_VERSION
#define OOPS_APP_VERSION "dev"
#endif

#define TAG "GL2-CUBE"
#define DEG_TO_RAD 0.017453292519943295f

typedef struct cube {
    oops_gfx_t *gfx;
    oops_display_t *disp;
    GLuint prog;
    GLint loc_mvp;
    char stop_path[64];
    bool ctl_pause; /* /app0/pause: no animation */
    bool ctl_dump; /* /app0/dump: frame 30 logs the command stream and the frame numbers
                    */
    bool auto_rotate;
    float rot_x, rot_y, rot_z;
    uint32_t prev_buttons;
    uint32_t last_frame_us;
    uint64_t frame;
} cube_t;

/*
 * The model-view-projection matrix is built here: a GL 2.0 program has no matrix stack,
 * and a port written against GL 2.0 sends its own matrices as uniforms. Column-major,
 * which is what glUniformMatrix4fv expects with transpose false.
 */
static void mat4_identity(float *m) {
    for (int i = 0; i < 16; i++)
        m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat4_multiply(float *out, const float *a, const float *b) {
    float r[16];
    for (int c = 0; c < 4; c++) {
        for (int row = 0; row < 4; row++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++)
                sum += a[k * 4 + row] * b[c * 4 + k];
            r[c * 4 + row] = sum;
        }
    }
    for (int i = 0; i < 16; i++)
        out[i] = r[i];
}

/* The field of view, aspect and clip planes gluPerspective gives gl1-cube. */
static void mat4_perspective(float *m, float fov_y_rad, float aspect, float znear,
                             float zfar) {
    const float f = oops_cosf(fov_y_rad * 0.5f) / oops_sinf(fov_y_rad * 0.5f);
    mat4_identity(m);
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (zfar + znear) / (znear - zfar);
    m[11] = -1.0f;
    m[14] = (2.0f * zfar * znear) / (znear - zfar);
    m[15] = 0.0f;
}

static void mat4_translate(float *m, float x, float y, float z) {
    mat4_identity(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
}

static void mat4_rotate_x(float *m, float rad) {
    mat4_identity(m);
    const float c = oops_cosf(rad), s = oops_sinf(rad);
    m[5] = c;
    m[6] = s;
    m[9] = -s;
    m[10] = c;
}

static void mat4_rotate_y(float *m, float rad) {
    mat4_identity(m);
    const float c = oops_cosf(rad), s = oops_sinf(rad);
    m[0] = c;
    m[2] = -s;
    m[8] = s;
    m[10] = c;
}

static void mat4_rotate_z(float *m, float rad) {
    mat4_identity(m);
    const float c = oops_cosf(rad), s = oops_sinf(rad);
    m[0] = c;
    m[1] = s;
    m[4] = -s;
    m[5] = c;
}

/* Compiles one stage, logging the info log when it fails. */
static GLuint compile_stage(GLenum type, const char *src, const char *what) {
    GLuint sh = glCreateShader(type);
    const GLchar *strings[1] = {src};
    glShaderSource(sh, 1, strings, (const GLint *)0);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[256] = {0};
        glGetShaderInfoLog(sh, (GLsizei)sizeof(log), (GLsizei *)0, log);
        oops_log_info(TAG, "%s", what);
        oops_log_info(TAG, "%s", log);
        glDeleteShader(sh);
        return 0u;
    }
    return sh;
}

/* Builds the program and binds the scene's attribute arrays. Returns 0 on failure. */
static GLuint build_program(GLint *loc_mvp) {
    const GLuint vs = compile_stage(GL_VERTEX_SHADER, GL2_CUBE_VERTEX_SHADER,
                                    "the vertex shader did not compile:");
    const GLuint fs = compile_stage(GL_FRAGMENT_SHADER, GL2_CUBE_FRAGMENT_SHADER,
                                    "the fragment shader did not compile:");
    if (!vs || !fs)
        return 0u;

    const GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[256] = {0};
        glGetProgramInfoLog(prog, (GLsizei)sizeof(log), (GLsizei *)0, log);
        oops_log_info(TAG, "the program did not link:");
        oops_log_info(TAG, "%s", log);
        return 0u;
    }
    /* Deleted now, and the program keeps drawing: GL 2.0's deferred deletion, exercised
     * the way a port uses it. */
    glDeleteShader(vs);
    glDeleteShader(fs);
    glUseProgram(prog);

    const GLint loc_pos = glGetAttribLocation(prog, "pos");
    const GLint loc_colour = glGetAttribLocation(prog, "colour");
    *loc_mvp = glGetUniformLocation(prog, "mvp");
    if (loc_pos < 0 || loc_colour < 0 || *loc_mvp < 0) {
        oops_log_info(TAG, "the program's interface is missing a name the draw needs");
        oops_log_info(TAG, "loc-pos 0x%08x", (unsigned)loc_pos);
        oops_log_info(TAG, "loc-colour 0x%08x", (unsigned)loc_colour);
        oops_log_info(TAG, "loc-mvp 0x%08x", (unsigned)*loc_mvp);
        return 0u;
    }
    glVertexAttribPointer((GLuint)loc_pos, 3, GL_FLOAT, GL_FALSE, 0,
                          GL2_CUBE_POSITIONS);
    glEnableVertexAttribArray((GLuint)loc_pos);
    glVertexAttribPointer((GLuint)loc_colour, 3, GL_FLOAT, GL_FALSE, 0,
                          GL2_CUBE_COLOURS);
    glEnableVertexAttribArray((GLuint)loc_colour);

    /* The fragment stage's size on the hardware, logged once. Zero words is a refusal:
     * the draw then fails with GL_INVALID_OPERATION rather than drawing the
     * fixed-function cube in its place. */
    GLint ps_words = 0;
    glGetProgramiv(prog, GL_PROGRAM_HW_PS_WORDS, &ps_words);
    oops_log_info(TAG, "compiled-ps-words 0x%08x", (unsigned)ps_words);
    if (ps_words == 0) {
        char log[256] = {0};
        glGetProgramHardwareLog(prog, (GLsizei)sizeof(log), (GLsizei *)0, log);
        oops_log_info(TAG, "the fragment stage has no console code:");
        oops_log_info(TAG, "%s", log);
    }
    return prog;
}

static bool cube_init(cube_t *c, const payload_args_t *args) {
    memset(c, 0, sizeof(*c));
    if (args)
        sys_call_init(args);
    oops_log_info(
        TAG, "starting gl2-cube (oops-gl 2.0, a shader program through gl_draw.c)...");
    cube_stop_path(c->stop_path, sizeof(c->stop_path),
                   (int)sys_call(SYS_getpid, 0, 0, 0, 0, 0, 0));
    oops_log_info(TAG, "%s", c->stop_path);

    c->gfx = oops_gfx_create(&(oops_gfx_desc_t){
        .width = CUBE_FRAME_W, .height = CUBE_FRAME_H, .depth = true, .vsync = true});
    if (!c->gfx) {
        oops_log_info(TAG, "failed to bring up the renderer (display or context)");
        return false;
    }
    c->disp = oops_gfx_display(c->gfx);
    if (!c->disp || !oops_display_is_ready(c->disp)) {
        oops_log_info(TAG, "display not ready");
        oops_gfx_destroy(c->gfx);
        return false;
    }

    /* A context gets the entry points its version names and the default is 1.1, so this
     * comes before any shader call. */
    glContextSetVersion(2, 0);
    glViewport(0, 0, (GLsizei)CUBE_FRAME_W, (GLsizei)CUBE_FRAME_H);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    c->prog = build_program(&c->loc_mvp);
    if (!c->prog) {
        oops_gfx_destroy(c->gfx);
        return false;
    }

    oops_input_init();
    oops_system_install_close_handler(); /* cooperate with the dashboard Close */
    c->rot_x = 25.0f;
    c->rot_y = 35.0f;
    c->rot_z = 10.0f;
    c->ctl_pause = oops_fs_exists("/app0/pause");
    c->ctl_dump = oops_fs_exists("/app0/dump");
    c->auto_rotate = !c->ctl_pause;
    oops_log_info(TAG, "entering the GL 2.0 rendering loop...");
    return true;
}

/* Reads the pad. Returns false when the run should end. */
static bool cube_input(cube_t *c) {
    oops_pad_state_t pad;
    if (oops_input_poll(0, &pad) != 0 || !pad.connected) {
        c->prev_buttons = 0;
        return true;
    }
    const uint32_t pressed = pad.buttons & ~c->prev_buttons;
    c->prev_buttons = pad.buttons;
    if (pad.buttons & (OOPS_BUTTON_CIRCLE | OOPS_BUTTON_OPTIONS)) {
        oops_log_info(TAG, "exit combo received, terminating cleanly");
        return false;
    }
    if (pressed & OOPS_BUTTON_CROSS)
        c->auto_rotate = !c->auto_rotate;
    if (pad.buttons & OOPS_BUTTON_LEFT)
        c->rot_y -= 2.0f;
    if (pad.buttons & OOPS_BUTTON_RIGHT)
        c->rot_y += 2.0f;
    if (pad.buttons & OOPS_BUTTON_UP)
        c->rot_x -= 2.0f;
    if (pad.buttons & OOPS_BUTTON_DOWN)
        c->rot_x += 2.0f;
    return true;
}

/* projection * translate * rotX * rotY * rotZ: the composition gl1-cube's matrix stack
 * builds with gluPerspective, glTranslatef and three glRotatef calls. */
static void draw_cube(cube_t *c) {
    if (c->auto_rotate) {
        c->rot_x += 0.75f;
        c->rot_y += 1.25f;
        c->rot_z += 0.50f;
        if (c->rot_x >= 360.0f)
            c->rot_x -= 360.0f;
        if (c->rot_y >= 360.0f)
            c->rot_y -= 360.0f;
        if (c->rot_z >= 360.0f)
            c->rot_z -= 360.0f;
    }

    glClearColor(0.05f, 0.07f, 0.12f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float mvp[16], proj[16], view[16], rx[16], ry[16], rz[16], tmp[16];
    mat4_perspective(proj, 45.0f * DEG_TO_RAD, 1920.0f / 1080.0f, 0.1f, 100.0f);
    mat4_translate(view, 0.0f, 0.0f, -4.5f);
    mat4_rotate_x(rx, c->rot_x * DEG_TO_RAD);
    mat4_rotate_y(ry, c->rot_y * DEG_TO_RAD);
    mat4_rotate_z(rz, c->rot_z * DEG_TO_RAD);
    mat4_multiply(tmp, proj, view);
    mat4_multiply(tmp, tmp, rx);
    mat4_multiply(tmp, tmp, ry);
    mat4_multiply(mvp, tmp, rz);
    glUniformMatrix4fv(c->loc_mvp, 1, GL_FALSE, mvp);

    glDrawArrays(GL_TRIANGLES, 0, GL2_CUBE_VERTEX_COUNT);
    /* A program the back end refused draws nothing and records GL_INVALID_OPERATION. */
    if (c->frame == 0u)
        oops_log_info(TAG, "first-draw-error 0x%08x", (unsigned)glGetError());
    glFinish();
}

/* The render target's numbers, the same three gl1-cube reports: one word in 64 unless
 * this run reads the frame back in full. */
static cube_frame_stats_t measure_frame(cube_t *c, bool full_scan) {
    const uint32_t *src = oops_display_get_framebuffer(c->disp);
    const GLuint *rb = glGetFrameReadbackSampled(full_scan ? 1u : 4u);
    if (rb)
        src = rb;
    const cube_frame_stats_t st =
        cube_frame_measure(src, full_scan ? (size_t)1 : (size_t)64, rb == NULL);
    if (c->ctl_dump && c->frame == 30u) {
        oops_log_info(TAG, "oracle-hash 0x%08x", (unsigned)st.hash);
        oops_log_info(TAG, "oracle-mod-pixels 0x%08x", (unsigned)st.mod_pixels);
        oops_log_info(TAG, "oracle-center-pixel 0x%08x", (unsigned)st.center);
    }
    return st;
}

/* The CPU-drawn HUD: magenta rather than gl1-cube's cyan, so a screenshot says which of
 * the pair it is. */
static void draw_hud(cube_t *c, const cube_frame_stats_t *st, bool full_scan) {
    oops_surface_t surf = oops_display_get_surface(c->disp);
    oops_draw_rect(&surf, 0, 0, 1920, 12, OOPS_COLOR_MAGENTA);
    oops_draw_rect(&surf, 0, 1068, 1920, 12, OOPS_COLOR_MAGENTA);
    oops_draw_rect(&surf, 0, 0, 12, 1080, OOPS_COLOR_MAGENTA);
    oops_draw_rect(&surf, 1908, 0, 12, 1080, OOPS_COLOR_MAGENTA);
    oops_draw_rect_blend(&surf, 40, 30, 820, 220, 0xd008101cu);
    oops_draw_rect(&surf, 40, 30, 820, 3, OOPS_COLOR_MAGENTA);

    oops_draw_text(&surf, 60, 48, "OOPS-GL 2.0: SHADER CUBE (RDNA2)", OOPS_COLOR_WHITE,
                   2);
    oops_draw_text(&surf, 60, 75, "GLSL compiled to gfx1030 by glsl_ps.c", 0xffff88ffu,
                   1);
    oops_draw_text(&surf, 60, 96, "Target: Prospero/Trinity (FW 12.40)", 0xffaaaaaau,
                   1);
    oops_draw_text(&surf, 380, 96, "Build: " OOPS_APP_VERSION, 0xffaaaaaau, 1);

    gl_hw_status_t hw;
    memset(&hw, 0, sizeof(hw));
    glGetHardwareStatus(&hw);
    const uint32_t badge_col =
        hw.verified ? 0xff44ff88u : (hw.failed ? 0xffff5544u : 0xffffcc44u);
    oops_draw_text(&surf, 640, 48,
                   hw.verified ? "GPU" : (hw.failed ? "GPU FAILED" : "GPU ?"),
                   badge_col, 2);

    /* How many gfx1030 instructions the fragment shader became, and its registers. */
    char buf[128];
    GLint ps_words = 0, ps_vgprs = 0;
    glGetProgramiv(c->prog, GL_PROGRAM_HW_PS_WORDS, &ps_words);
    glGetProgramiv(c->prog, GL_PROGRAM_HW_PS_VGPRS, &ps_vgprs);
    oops_snprintf(buf, sizeof(buf), "Compiled PS: %d words, %d regs", (int)ps_words,
                  (int)ps_vgprs);
    oops_draw_text(&surf, 60, 118, buf,
                   ps_words > 0 ? OOPS_COLOR_GREEN : OOPS_COLOR_RED, 1);

    oops_snprintf(buf, sizeof(buf), "%s0x%08x  |  %s%d  |  Center: 0x%08x",
                  full_scan ? "Hash: " : "Hash(1:64): ", (unsigned)st->hash,
                  full_scan ? "Mod Pix: " : "Mod Pix(1:64): ", (int)st->mod_pixels,
                  (unsigned)st->center);
    oops_draw_text(&surf, 60, 140, buf,
                   (st->mod_pixels > 0) ? OOPS_COLOR_GREEN : OOPS_COLOR_RED, 1);

    oops_snprintf(
        buf, sizeof(buf), "RotX: %d deg  RotY: %d deg  |  Frame: %u (%u ms/frame)%s",
        (int)c->rot_x, (int)c->rot_y, (unsigned int)c->frame,
        (unsigned int)(c->last_frame_us / 1000u), !c->auto_rotate ? "  PAUSED" : "");
    oops_draw_text(&surf, 60, 162, buf, OOPS_COLOR_WHITE, 1);

    oops_snprintf(buf, sizeof(buf), "Program: %u  |  Tris: 12  |  Verts: %u",
                  (unsigned int)c->prog, (unsigned int)GL2_CUBE_VERTEX_COUNT);
    oops_draw_text(&surf, 60, 184, buf, 0xffffaa33u, 1);

    oops_draw_rect_blend(&surf, 40, 1000, 900, 50, 0xd008101cu);
    oops_draw_rect(&surf, 40, 1000, 900, 2, 0xff773355u);
    oops_draw_text(&surf, 60, 1018, "Controls: D-Pad=Orbit | [X]=Rotate | (O)=Exit",
                   0xffeeeeeeu, 1);
}

/* One frame, top to swap. Returns false when the run should end. */
static bool cube_frame(cube_t *c) {
    const uint64_t t_top = oops_time_get_us();
    if (oops_system_close_requested()) {
        oops_log_info(TAG, "dashboard close received, terminating cleanly");
        return false;
    }
    if (!cube_input(c))
        return false;
    if ((c->frame % 30u) == 0u && oops_fs_exists(c->stop_path)) {
        oops_log_info(TAG, "stop file found, terminating cleanly");
        return false;
    }
    if (c->ctl_dump && c->frame == 30u)
        glRequestHardwareDump();

    draw_cube(c);
    const bool full_scan = c->ctl_pause || c->ctl_dump;
    const cube_frame_stats_t st = measure_frame(c, full_scan);
    draw_hud(c, &st, full_scan);

    (void)oops_gfx_present(c->gfx);
    c->last_frame_us = (uint32_t)(oops_time_get_us() - t_top);
    if (c->frame == 5u || (c->frame % 60u) == 0u)
        oops_log_info(TAG, "t-frame-us 0x%08x", (unsigned)c->last_frame_us);
    c->frame++;
    return true;
}

int gl2_cube_start(const payload_args_t *args);

__attribute__((visibility("default"))) int gl2_cube_start(const payload_args_t *args) {
    static cube_t cube;
    if (!cube_init(&cube, args))
        return -1;
    while (cube_frame(&cube)) {
    }
    glUseProgram(0u);
    glDeleteProgram(cube.prog);
    oops_gfx_destroy(cube.gfx);
    oops_log_info(TAG, "gl2-cube session closed cleanly");
    return 0;
}
