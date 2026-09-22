/*
 * gl2-cube: the same cube as gl1-cube, drawn by a shader program.
 *
 * **What this is for.** oops-gl's GL 2.0 back end compiles a fragment shader to gfx1030 and the
 * draw path binds it, and obSCEne has measured the pieces of that on hardware - a generated
 * shader retires (`REQ-...-4e77`), its parameters arrive, its uniform block loads
 * (`REQ-...-6c0d`), and it can sample a texture (`REQ-...-2a45`). What none of those measured is
 * **oops-gl's own draw path** putting a GL 2.0 program on screen: every one of them bound a
 * shader from obSCEne's fixture. This is the title that closes that gap, and the first picture
 * drawn by this compiler through `gl_draw.c`.
 *
 * It is deliberately the same cube as gl1-cube, in the same place, at the same angles. The two
 * titles then differ in exactly one thing - fixed function against a shader program - so a
 * difference in what they draw is a difference in that, and the fixed-function half is already
 * pinned to a measured console frame.
 *
 * Target: Sony PlayStation 5 (AMD RDNA2 GFX10.3 / Prospero FW 12.40)
 */

#include "GL/gl.h"
#include "oops/gfx.h"
#include "oops/display.h"
#include "oops/draw.h"
#include "oops/input.h"
#include "oops/time.h"
#include "oops/memory.h"
#include "oops/syscall.h"
#include "oops/freestd.h"
#include "oops/math.h"
#include "oops/fs.h"
#include "oops/system.h"
#include "gl2_cube_scene.h"
#include "gl2_cube_shaders.h"
#include <stdbool.h>

#ifndef OOPS_HOST_BUILD
/* A file pushed to the title directory ends the session cleanly - for a target driven over the
 * network, where no controller is at hand and no signal ends a process holding a GPU queue. The
 * name carries this run's process id, so a file left behind by an earlier run is ignored and
 * nothing has to be read or unlinked: existence is the whole signal. The same mechanism
 * gl1-cube uses, and the reason it exists is recorded there. */
#define GL2_CUBE_STOP_PREFIX "/app0/stop."

static void cube_klog(const char *msg) { oops_klog("GL2-CUBE", msg); }
#endif

static void int_to_str(int val, char *buf) {
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[16];
    int pos = 0;
    int v = val < 0 ? -val : val;
    while (v > 0) { tmp[pos++] = (char)('0' + (v % 10)); v /= 10; }
    int out = 0;
    if (val < 0) buf[out++] = '-';
    for (int i = pos - 1; i >= 0; i--) buf[out++] = tmp[i];
    buf[out] = '\0';
}

static void hex_to_str(uint32_t val, char *buf) {
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 7; i >= 0; i--) {
        uint8_t d = (uint8_t)(val & 0xf);
        buf[2 + i] = (char)(d < 10 ? ('0' + d) : ('a' + d - 10));
        val >>= 4;
    }
    buf[10] = '\0';
}

#ifndef OOPS_HOST_BUILD
static void cube_klog_hex(const char *tag, uint32_t v) {
    char msg[64];
    int n = 0;
    while (tag[n] && n < 40) { msg[n] = tag[n]; n++; }
    msg[n++] = ' ';
    hex_to_str(v, &msg[n]);
    cube_klog(msg);
}
#endif

static int cube_append(char *dst, int at, const char *s) {
    while (*s && at < 126) dst[at++] = *s++;
    dst[at] = '\0';
    return at;
}

/*
 * The model-view-projection matrix, built here rather than with `glLoadIdentity` and friends.
 *
 * **A GL 2.0 program has no matrix stack.** `gl_ModelViewProjectionMatrix` exists for a shader
 * that wants the fixed-function state, and this deliberately does not use it: a port written
 * against GL 2.0 builds its own matrices and sends them as uniforms, and that is the path this
 * title is here to exercise. Column-major, which is what GL means by a `mat4` and what
 * `glUniformMatrix4fv` with `transpose` false expects.
 */
static void mat4_identity(float *m) {
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat4_multiply(float *out, const float *a, const float *b) {
    float r[16];
    for (int c = 0; c < 4; c++) {
        for (int row = 0; row < 4; row++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) sum += a[k * 4 + row] * b[c * 4 + k];
            r[c * 4 + row] = sum;
        }
    }
    for (int i = 0; i < 16; i++) out[i] = r[i];
}

/* The same 45-degree vertical field of view, aspect and clip planes `gluPerspective` gives
 * gl1-cube, written out because there is no GLU on this path either. */
static void mat4_perspective(float *m, float fov_y_rad, float aspect, float znear, float zfar) {
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
    m[12] = x; m[13] = y; m[14] = z;
}

static void mat4_rotate_x(float *m, float rad) {
    mat4_identity(m);
    const float c = oops_cosf(rad), s = oops_sinf(rad);
    m[5] = c; m[6] = s; m[9] = -s; m[10] = c;
}

static void mat4_rotate_y(float *m, float rad) {
    mat4_identity(m);
    const float c = oops_cosf(rad), s = oops_sinf(rad);
    m[0] = c; m[2] = -s; m[8] = s; m[10] = c;
}

static void mat4_rotate_z(float *m, float rad) {
    mat4_identity(m);
    const float c = oops_cosf(rad), s = oops_sinf(rad);
    m[0] = c; m[1] = s; m[4] = -s; m[5] = c;
}

#define DEG_TO_RAD 0.017453292519943295f

/*
 * Compiles one stage and says what went wrong if it did not.
 *
 * **The log is printed, not swallowed.** A shader that fails to compile on a console with no
 * console is otherwise a black screen, and the whole reason `glGetShaderInfoLog` exists is that
 * the message names the line.
 */
static GLuint compile_stage(GLenum type, const char *src, const char *what) {
    GLuint sh = glCreateShader(type);
    const GLchar *strings[1];
    strings[0] = src;
    glShaderSource(sh, 1, strings, (const GLint *)0);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
#ifndef OOPS_HOST_BUILD
        char log[256] = {0};
        glGetShaderInfoLog(sh, (GLsizei)sizeof(log), (GLsizei *)0, log);
        cube_klog(what);
        cube_klog(log);
#else
        (void)what;
#endif
        glDeleteShader(sh);
        return 0u;
    }
    return sh;
}

int gl2_cube_start(const payload_args_t *args);

__attribute__((visibility("default"))) int gl2_cube_start(const payload_args_t *args) {
#ifndef OOPS_HOST_BUILD
    if (args) sys_call_init(args);
    cube_klog("starting gl2-cube (oops-gl 2.0, a shader program through gl_draw.c)...");
    char stop_path[64];
    {
        char pid_str[16];
        int_to_str((int)sys_call(SYS_getpid, 0, 0, 0, 0, 0, 0), pid_str);
        int sp = 0;
        for (int i = 0; GL2_CUBE_STOP_PREFIX[i]; i++) stop_path[sp++] = GL2_CUBE_STOP_PREFIX[i];
        for (int i = 0; pid_str[i] && sp < (int)sizeof(stop_path) - 1; i++) stop_path[sp++] = pid_str[i];
        stop_path[sp] = '\0';
        cube_klog(stop_path);
    }
#else
    (void)args;
#endif

    oops_gfx_t *gfx = oops_gfx_create(&(oops_gfx_desc_t){ .width = 1920, .height = 1080,
                                                          .depth = true, .vsync = true });
    if (!gfx) {
#ifndef OOPS_HOST_BUILD
        cube_klog("failed to bring up the renderer (display or context)");
#endif
        return -1;
    }
    oops_display_t *disp = oops_gfx_display(gfx);
    if (!disp || !oops_display_is_ready(disp)) {
#ifndef OOPS_HOST_BUILD
        cube_klog("display not ready");
#endif
        oops_gfx_destroy(gfx);
        return -1;
    }

    /*
     * **The context's version, which is not decoration.**
     *
     * oops-gl gives a context the entry points its version names and no others, and the default
     * is 1.1 - so without this every `glCreateShader` below is `GL_INVALID_OPERATION` and this
     * title draws nothing at all. It is the first thing after the context because everything
     * after it depends on it.
     */
    glContextSetVersion(2, 0);

    glViewport(0, 0, 1920, 1080);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    /* ---------------------------------------------------------------------
     * The program
     * --------------------------------------------------------------------- */
    const GLuint vs = compile_stage(GL_VERTEX_SHADER, GL2_CUBE_VERTEX_SHADER,
                                    "the vertex shader did not compile:");
    const GLuint fs = compile_stage(GL_FRAGMENT_SHADER, GL2_CUBE_FRAGMENT_SHADER,
                                    "the fragment shader did not compile:");
    if (!vs || !fs) {
        oops_gfx_destroy(gfx);
        return -1;
    }

    const GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    {
        GLint linked = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &linked);
        if (!linked) {
#ifndef OOPS_HOST_BUILD
            char log[256] = {0};
            glGetProgramInfoLog(prog, (GLsizei)sizeof(log), (GLsizei *)0, log);
            cube_klog("the program did not link:");
            cube_klog(log);
#endif
            oops_gfx_destroy(gfx);
            return -1;
        }
    }
    /* **The shader objects are deleted now and the program keeps working.** That is GL 2.0's
     * deferred deletion, and doing it here is both what a real port does and a live check of it:
     * if the program stopped drawing after this, the reference counting would be wrong. */
    glDeleteShader(vs);
    glDeleteShader(fs);

    glUseProgram(prog);

    const GLint loc_pos = glGetAttribLocation(prog, "pos");
    const GLint loc_colour = glGetAttribLocation(prog, "colour");
    const GLint loc_mvp = glGetUniformLocation(prog, "mvp");
    if (loc_pos < 0 || loc_colour < 0 || loc_mvp < 0) {
#ifndef OOPS_HOST_BUILD
        cube_klog("the program's interface is missing a name the draw needs");
        cube_klog_hex("loc-pos", (uint32_t)loc_pos);
        cube_klog_hex("loc-colour", (uint32_t)loc_colour);
        cube_klog_hex("loc-mvp", (uint32_t)loc_mvp);
#endif
        oops_gfx_destroy(gfx);
        return -1;
    }

    glVertexAttribPointer((GLuint)loc_pos, 3, GL_FLOAT, GL_FALSE, 0, GL2_CUBE_POSITIONS);
    glEnableVertexAttribArray((GLuint)loc_pos);
    glVertexAttribPointer((GLuint)loc_colour, 3, GL_FLOAT, GL_FALSE, 0, GL2_CUBE_COLOURS);
    glEnableVertexAttribArray((GLuint)loc_colour);

#ifndef OOPS_HOST_BUILD
    /* **What the console back end made of the fragment stage**, logged once. Zero words means
     * the compiler refused it - the draw will then fail with GL_INVALID_OPERATION rather than
     * drawing the fixed-function cube in its place, which is deliberate, and this line is how a
     * run says so before the first frame instead of after a black one. */
    {
        GLint ps_words = 0;
        glGetProgramiv(prog, GL_PROGRAM_HW_PS_WORDS, &ps_words);
        cube_klog_hex("compiled-ps-words", (uint32_t)ps_words);
        if (ps_words == 0) {
            char log[256] = {0};
            glGetProgramHardwareLog(prog, (GLsizei)sizeof(log), (GLsizei *)0, log);
            cube_klog("the fragment stage has no console code:");
            cube_klog(log);
        }
    }
#endif

    oops_input_init();
    oops_system_install_close_handler(); /* cooperate with the dashboard Close (oops/system.h) */

    float rot_x = 25.0f, rot_y = 35.0f, rot_z = 10.0f;
    const float cam_dist = -4.5f;

#ifndef OOPS_HOST_BUILD
    const bool ctl_pause = oops_fs_exists("/app0/pause");
    const bool ctl_dump = oops_fs_exists("/app0/dump");
#else
    const bool ctl_pause = false, ctl_dump = false;
#endif
    bool auto_rotate = !ctl_pause;
    uint32_t last_frame_us = 0;
    uint32_t prev_buttons = 0;
    uint64_t frame = 0;
    bool running = true;

#ifndef OOPS_HOST_BUILD
    cube_klog("entering the GL 2.0 rendering loop...");
#endif

    while (running) {
#ifndef OOPS_HOST_BUILD
        const uint64_t t_top = oops_time_get_us();
        if (oops_system_close_requested()) {
            cube_klog("dashboard close received, terminating cleanly");
            running = false;
            break;
        }
#endif
        oops_pad_state_t pad;
        if (oops_input_poll(0, &pad) == 0 && pad.connected) {
            const uint32_t pressed = pad.buttons & ~prev_buttons;
            prev_buttons = pad.buttons;
            if ((pad.buttons & OOPS_BUTTON_CIRCLE) || (pad.buttons & OOPS_BUTTON_OPTIONS)) {
#ifndef OOPS_HOST_BUILD
                cube_klog("exit combo received, terminating cleanly");
#endif
                running = false;
                break;
            }
            if (pressed & OOPS_BUTTON_CROSS) auto_rotate = !auto_rotate;
            if (pad.buttons & OOPS_BUTTON_LEFT)  rot_y -= 2.0f;
            if (pad.buttons & OOPS_BUTTON_RIGHT) rot_y += 2.0f;
            if (pad.buttons & OOPS_BUTTON_UP)    rot_x -= 2.0f;
            if (pad.buttons & OOPS_BUTTON_DOWN)  rot_x += 2.0f;
        } else {
            prev_buttons = 0;
        }

#ifndef OOPS_HOST_BUILD
        if ((frame % 30u) == 0u && oops_fs_exists(stop_path)) {
            cube_klog("stop file found, terminating cleanly");
            running = false;
            break;
        }
        if (ctl_dump && frame == 30u) glRequestHardwareDump();
#endif

        if (auto_rotate) {
            rot_x += 0.75f; rot_y += 1.25f; rot_z += 0.50f;
            if (rot_x >= 360.0f) rot_x -= 360.0f;
            if (rot_y >= 360.0f) rot_y -= 360.0f;
            if (rot_z >= 360.0f) rot_z -= 360.0f;
        }

        glClearColor(0.05f, 0.07f, 0.12f, 1.0f);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        /* projection * translate * rotX * rotY * rotZ, in that order, so the cube spins about
         * its own axes in front of the camera - the same composition gl1-cube's matrix stack
         * builds with `gluPerspective`, `glTranslatef` and three `glRotatef`s. */
        float mvp[16], proj[16], view[16], rx[16], ry[16], rz[16], tmp[16];
        mat4_perspective(proj, 45.0f * DEG_TO_RAD, 1920.0f / 1080.0f, 0.1f, 100.0f);
        mat4_translate(view, 0.0f, 0.0f, cam_dist);
        mat4_rotate_x(rx, rot_x * DEG_TO_RAD);
        mat4_rotate_y(ry, rot_y * DEG_TO_RAD);
        mat4_rotate_z(rz, rot_z * DEG_TO_RAD);
        mat4_multiply(tmp, proj, view);
        mat4_multiply(tmp, tmp, rx);
        mat4_multiply(tmp, tmp, ry);
        mat4_multiply(mvp, tmp, rz);
        glUniformMatrix4fv(loc_mvp, 1, GL_FALSE, mvp);

        glDrawArrays(GL_TRIANGLES, 0, GL2_CUBE_VERTEX_COUNT);
#ifndef OOPS_HOST_BUILD
        /* **The first draw's error, once.** A program the back end refused draws nothing and
         * records GL_INVALID_OPERATION; without this the run is a black screen with no reason
         * attached to it. */
        if (frame == 0u) cube_klog_hex("first-draw-error", (uint32_t)glGetError());
#endif
        glFinish();

        /* The render target's own numbers, the same three gl1-cube reports, so the two titles'
         * logs can be read side by side. One word in 64 unless this run is reading the frame
         * back, which is what the label says. */
        uint32_t *fb = oops_display_get_framebuffer(disp);
        const uint32_t *src = fb;
        const bool full_scan = ctl_pause || ctl_dump;
        const size_t scan_step = full_scan ? (size_t)1 : (size_t)64;
#ifndef OOPS_HOST_BUILD
        const GLuint *rb = glGetFrameReadbackSampled(full_scan ? 1u : 4u);
        if (rb) src = rb;
        const bool flush_src = (rb == NULL);
#endif
        uint32_t center_pix = 0, mod_pixels = 0, frame_hash = 0x811c9dc5u;
        if (src) {
#ifndef OOPS_HOST_BUILD
            __builtin_ia32_clflush((const void *)&src[540 * 1920 + 960]);
#endif
            center_pix = src[540 * 1920 + 960];
            for (size_t p = 0; p < (size_t)1920 * 1080; p += scan_step) {
#ifndef OOPS_HOST_BUILD
                if (flush_src && (p & 15u) == 0u) __builtin_ia32_clflush((const void *)&src[p]);
#endif
                const uint32_t v = src[p];
                if (v != 0xff0d121fu) mod_pixels++;
                frame_hash ^= v;
                frame_hash *= 0x01000193u;
            }
        }
#ifndef OOPS_HOST_BUILD
        if (ctl_dump && frame == 30u) {
            cube_klog_hex("oracle-hash", frame_hash);
            cube_klog_hex("oracle-mod-pixels", mod_pixels);
            cube_klog_hex("oracle-center-pixel", center_pix);
        }
#endif

        /* ---------------------------------------------------------------------
         * The HUD
         * --------------------------------------------------------------------- */
        oops_surface_t surf = oops_display_get_surface(disp);
        oops_draw_rect(&surf, 0, 0, 1920, 12, OOPS_COLOR_MAGENTA);
        oops_draw_rect(&surf, 0, 1068, 1920, 12, OOPS_COLOR_MAGENTA);
        oops_draw_rect(&surf, 0, 0, 12, 1080, OOPS_COLOR_MAGENTA);
        oops_draw_rect(&surf, 1908, 0, 12, 1080, OOPS_COLOR_MAGENTA);
        oops_draw_rect_blend(&surf, 40, 30, 820, 220, 0xd008101cu);
        oops_draw_rect(&surf, 40, 30, 820, 3, OOPS_COLOR_MAGENTA);

#ifndef OOPS_APP_VERSION
#define OOPS_APP_VERSION "dev"
#endif
        /* Magenta rather than gl1-cube's cyan, so a screenshot says which of the pair it is
         * without anybody reading the text. */
        oops_draw_text(&surf, 60, 48, "OOPS-GL 2.0: SHADER CUBE (RDNA2)", OOPS_COLOR_WHITE, 2);
        oops_draw_text(&surf, 60, 75, "GLSL compiled to gfx1030 by glsl_ps.c", 0xffff88ffu, 1);
        oops_draw_text(&surf, 60, 96, "Target: Prospero/Trinity (FW 12.40)", 0xffaaaaaau, 1);
        oops_draw_text(&surf, 380, 96, "Build: " OOPS_APP_VERSION, 0xffaaaaaau, 1);

        gl_hw_status_t hw;
        memset(&hw, 0, sizeof(hw));
#ifndef OOPS_HOST_BUILD
        glGetHardwareStatus(&hw);
#endif
        const uint32_t badge_col = hw.verified ? 0xff44ff88u : (hw.failed ? 0xffff5544u : 0xffffcc44u);
        oops_draw_text(&surf, 640, 48, hw.verified ? "GPU" : (hw.failed ? "GPU FAILED" : "GPU ?"),
                       badge_col, 2);

        char stat_buf[128];
        char num_buf[16];
        char hx[16];
        int sl;

        /* **The number this title exists to show.** How many gfx1030 instructions the fragment
         * shader became, and how many registers it wants. Zero words is a refusal. */
        {
            GLint ps_words = 0, ps_vgprs = 0;
#ifndef OOPS_HOST_BUILD
            glGetProgramiv(prog, GL_PROGRAM_HW_PS_WORDS, &ps_words);
            glGetProgramiv(prog, GL_PROGRAM_HW_PS_VGPRS, &ps_vgprs);
#endif
            sl = cube_append(stat_buf, 0, "Compiled PS: ");
            int_to_str((int)ps_words, num_buf); sl = cube_append(stat_buf, sl, num_buf);
            sl = cube_append(stat_buf, sl, " words, ");
            int_to_str((int)ps_vgprs, num_buf); sl = cube_append(stat_buf, sl, num_buf);
            sl = cube_append(stat_buf, sl, " regs");
            oops_draw_text(&surf, 60, 118, stat_buf,
                           ps_words > 0 ? OOPS_COLOR_GREEN : OOPS_COLOR_RED, 1);
        }

        {
            sl = cube_append(stat_buf, 0, full_scan ? "Hash: " : "Hash(1:64): ");
            hex_to_str(frame_hash, hx); sl = cube_append(stat_buf, sl, hx);
            sl = cube_append(stat_buf, sl, full_scan ? "  |  Mod Pix: " : "  |  Mod Pix(1:64): ");
            int_to_str((int)mod_pixels, num_buf); sl = cube_append(stat_buf, sl, num_buf);
            sl = cube_append(stat_buf, sl, "  |  Center: ");
            hex_to_str(center_pix, hx); sl = cube_append(stat_buf, sl, hx);
            oops_draw_text(&surf, 60, 140, stat_buf,
                           (mod_pixels > 0) ? OOPS_COLOR_GREEN : OOPS_COLOR_RED, 1);
        }

        oops_snprintf(stat_buf, sizeof(stat_buf),
                      "RotX: %d deg  RotY: %d deg  |  Frame: %u (%u ms/frame)%s",
                      (int)rot_x, (int)rot_y, (unsigned int)frame,
                      (unsigned int)(last_frame_us / 1000u), !auto_rotate ? "  PAUSED" : "");
        oops_draw_text(&surf, 60, 162, stat_buf, OOPS_COLOR_WHITE, 1);

        oops_snprintf(stat_buf, sizeof(stat_buf), "Program: %u  |  Tris: 12  |  Verts: %u",
                      (unsigned int)prog, (unsigned int)GL2_CUBE_VERTEX_COUNT);
        oops_draw_text(&surf, 60, 184, stat_buf, 0xffffaa33u, 1);

        oops_draw_rect_blend(&surf, 40, 1000, 900, 50, 0xd008101cu);
        oops_draw_rect(&surf, 40, 1000, 900, 2, 0xff773355u);
        oops_draw_text(&surf, 60, 1018,
                       "Controls: D-Pad=Orbit | [X]=Rotate | (O)=Exit",
                       0xffeeeeeeu, 1);

        (void)oops_gfx_present(gfx);
#ifndef OOPS_HOST_BUILD
        last_frame_us = (uint32_t)(oops_time_get_us() - t_top);
        if (frame == 5u || (frame % 60u) == 0u) cube_klog_hex("t-frame-us", last_frame_us);
#endif
        frame++;
    }

    glUseProgram(0u);
    glDeleteProgram(prog);
    oops_gfx_destroy(gfx);

#ifndef OOPS_HOST_BUILD
    cube_klog("gl2-cube session closed cleanly");
#endif
    return 0;
}
