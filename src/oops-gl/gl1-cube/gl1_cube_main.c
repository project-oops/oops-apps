/*
 * gl1-cube: a rotating cube, torus and sphere through oops-gl's OpenGL 1.x path.
 *
 * The pinned hardware oracle. The frame a `dump` run records at frame 30 is asserted
 * register by register by test_pm4_gl_honours_the_gl_cube_oracle_record in oops-sdk, so
 * the command stream this title emits is held to one the hardware accepted. The title
 * id stays GLCB00001 because that record names it (app.env).
 */

#include "GL/gl.h"
#include "GL/glu.h"
#include "cube_frame.h"
#include "cube_hud.h"
#include "gl1_cube_scene.h"
#include "obj_loader.h"
#include "oops/display.h"
#include "oops/draw.h"
#include "oops/freestd.h"
#include "oops/fs.h"
#include "oops/gfx.h"
#include "oops/hud.h"
#include "oops/input.h"
#include "oops/memory.h"
#include "oops/syscall.h"
#include "oops/system.h"
#include "oops/time.h"
#include <stdbool.h>

#ifndef OOPS_APP_VERSION
#define OOPS_APP_VERSION "dev"
#endif

#define TAG "GL-CUBE"

typedef enum { GEOM_CUBE = 0, GEOM_TORUS = 1, GEOM_SPHERE = 2, GEOM_MAX } geom_mode_t;

/* Control files pushed to the title directory before launch; existence is the signal.
 */
typedef struct cube_controls {
    bool pause; /* /app0/pause: one fixed orientation, depth toggled at frames 60, 90 */
    bool dump;  /* /app0/dump: frame 30 logs the oracle record */
    bool nodepth; /* /app0/nodepth: start with the depth test off */
    bool tex;     /* /app0/tex: start with the texture on */
} cube_controls_t;

typedef struct cube {
    oops_gfx_t *gfx;
    oops_display_t *disp;
    oops_hud_t *hud;
    GLuint tex_id;
    oops_mesh_t torus;
    oops_mesh_t sphere;
    cube_controls_t ctl;
    char stop_path[64];

    geom_mode_t geom;
    float rot_x, rot_y, rot_z;
    bool auto_rotate, opt_texture, opt_depth, opt_lighting, opt_cull;
    uint32_t prev_buttons;
    uint32_t last_frame_us; /* the previous frame, top to swap, for the HUD */
    uint64_t frame;
    /* The previous frame's pixels, kept only in paused runs so the frames either side
     * of a depth toggle can be compared. */
    uint32_t *prev_frame;
} cube_t;

/* What one frame drew and measured. */
typedef struct cube_frame_info {
    const char *geom_name;
    size_t tri_count;
    GLsizei vcount;
    bool full_scan;
    cube_frame_stats_t stats;
    uint64_t t_top, t_finish, t_hash, t_hud;
} cube_frame_info_t;

static const float k_light_pos[4] = {2.0f, 3.5f, 4.0f, 1.0f};
static const float k_cam_dist = -4.5f;

/* The cube's arrays and procedural texture live in gl1_cube_scene.h, shared with the
 * host self-test. */
static uint32_t s_cube_texture[TEX_DIM * TEX_DIM];

static void set_cap(GLenum cap, bool on) {
    if (on)
        glEnable(cap);
    else
        glDisable(cap);
}

static void bind_arrays(const GLfloat *pos, const GLfloat *nrm, const GLfloat *col,
                        const GLfloat *uv) {
    glVertexPointer(3, GL_FLOAT, 0, pos);
    glNormalPointer(GL_FLOAT, 0, nrm);
    glColorPointer(3, GL_FLOAT, 0, col);
    glTexCoordPointer(2, GL_FLOAT, 0, uv);
}

static void setup_texture(cube_t *c) {
    generate_cube_texture(s_cube_texture);
    glGenTextures(1, &c->tex_id);
    glBindTexture(GL_TEXTURE_2D, c->tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_DIM, TEX_DIM, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, s_cube_texture);
    /* GL_OUT_OF_MEMORY here means no GPU memory for the texture. */
    oops_log_info(TAG, "tex-image-error 0x%08x", (unsigned)glGetError());
    glDisable(GL_TEXTURE_2D);
}

static void setup_lighting(void) {
    static const float diffuse[4] = {1.0f, 0.96f, 0.90f, 1.0f};
    static const float ambient[4] = {0.25f, 0.25f, 0.30f, 1.0f};
    static const float specular[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    static const float mat_spec[4] = {0.9f, 0.9f, 0.9f, 1.0f};

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glLightfv(GL_LIGHT0, GL_POSITION, k_light_pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 32.0f);
}

static void setup_gl(cube_t *c) {
    glViewport(0, 0, (GLsizei)CUBE_FRAME_W, (GLsizei)CUBE_FRAME_H);
    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glShadeModel(GL_SMOOTH);

    setup_texture(c);
    setup_lighting();

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    bind_arrays(s_cube_vertices, s_cube_normals, s_cube_colors, s_cube_texcoords);

    memset(&c->torus, 0, sizeof(c->torus));
    (void)oops_mesh_create_torus(&c->torus, 24, 16, 0.75f, 0.35f);
    memset(&c->sphere, 0, sizeof(c->sphere));
    (void)oops_mesh_create_sphere(&c->sphere, 20, 20, 0.95f);
}

/* Brings up the renderer, the overlay and the GL state. Returns false when there is no
 * display to draw on. */
static bool cube_init(cube_t *c, const payload_args_t *args) {
    memset(c, 0, sizeof(*c));
    if (args)
        sys_call_init(args);
    oops_log_info(TAG, "starting gl1-cube (oops-gl 1.x 3D cube, the pinned oracle)...");
    cube_stop_path(c->stop_path, sizeof(c->stop_path),
                   (int)sys_call(SYS_getpid, 0, 0, 0, 0, 0, 0));
    oops_log_info(TAG, "%s", c->stop_path);

    /* The display and the GL context in one call, made current (oops/gfx.h). */
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

    /* The GPU overlay (common/cube_hud.c); drawing with a NULL one does nothing. */
    c->hud = oops_hud_create((int)CUBE_FRAME_W, (int)CUBE_FRAME_H);
    if (c->hud == NULL)
        oops_log_info(TAG, "overlay did not come up; drawing without it");

    setup_gl(c);
    oops_input_init();
    /* The dashboard's Close sets a flag the loop checks, so the title tears down
     * instead of being killed mid-frame (oops/system.h). */
    oops_system_install_close_handler();

    c->ctl.pause = oops_fs_exists("/app0/pause");
    c->ctl.dump = oops_fs_exists("/app0/dump");
    c->ctl.nodepth = oops_fs_exists("/app0/nodepth");
    c->ctl.tex = oops_fs_exists("/app0/tex");

    c->geom = GEOM_CUBE;
    c->rot_x = 25.0f;
    c->rot_y = 35.0f;
    c->rot_z = 10.0f;
    c->auto_rotate = !c->ctl.pause;
    c->opt_texture = c->ctl.tex;
    c->opt_depth = !c->ctl.nodepth;
    c->opt_lighting = true;
    c->opt_cull = true;
    set_cap(GL_DEPTH_TEST, c->opt_depth);
    if (c->opt_texture)
        glEnable(GL_TEXTURE_2D);
    oops_log_info(TAG, "entering 3D rendering loop at 60 FPS...");
    return true;
}

/* Reads the pad and applies its toggles. Returns false when the run should end. */
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
    if (pressed & OOPS_BUTTON_TRIANGLE)
        set_cap(GL_TEXTURE_2D, c->opt_texture = !c->opt_texture);
    if (pressed & OOPS_BUTTON_SQUARE)
        set_cap(GL_DEPTH_TEST, c->opt_depth = !c->opt_depth);
    if (pressed & OOPS_BUTTON_L1)
        set_cap(GL_LIGHTING, c->opt_lighting = !c->opt_lighting);
    if (pressed & OOPS_BUTTON_R1)
        set_cap(GL_CULL_FACE, c->opt_cull = !c->opt_cull);
    if (pressed & (OOPS_BUTTON_R2 | OOPS_BUTTON_L2))
        c->geom = (geom_mode_t)((c->geom + 1) % GEOM_MAX);

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

static void advance_rotation(cube_t *c) {
    if (!c->auto_rotate)
        return;
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

/* Binds the active mesh and draws it. A mesh that did not allocate draws nothing and
 * reports zero triangles under its own name. */
static void draw_geometry(cube_t *c, cube_frame_info_t *fi) {
    glClearColor(0.05f, 0.07f, 0.12f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, 1920.0 / 1080.0, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, k_cam_dist);
    /* The light is set in camera space, before the rotation, so it stays put. */
    glLightfv(GL_LIGHT0, GL_POSITION, k_light_pos);
    glRotatef(c->rot_x, 1.0f, 0.0f, 0.0f);
    glRotatef(c->rot_y, 0.0f, 1.0f, 0.0f);
    glRotatef(c->rot_z, 0.0f, 0.0f, 1.0f);

    const oops_mesh_t *mesh = NULL;
    fi->vcount = 0;
    fi->geom_name = "CUBE";
    fi->tri_count = 12;
    if (c->geom == GEOM_CUBE) {
        bind_arrays(s_cube_vertices, s_cube_normals, s_cube_colors, s_cube_texcoords);
        fi->vcount = 36;
    } else if (c->geom == GEOM_TORUS || c->geom == GEOM_SPHERE) {
        mesh = c->geom == GEOM_TORUS ? &c->torus : &c->sphere;
        fi->geom_name = c->geom == GEOM_TORUS ? "TORUS" : "SPHERE";
        fi->tri_count = 0;
    }
    if (mesh && mesh->positions) {
        bind_arrays(mesh->positions, mesh->normals, mesh->colors, mesh->texcoords);
        fi->vcount = (GLsizei)mesh->vertex_count;
        fi->tri_count = mesh->triangle_count;
    }
    if (fi->vcount > 0)
        glDrawArrays(GL_TRIANGLES, 0, fi->vcount);
    glFinish();
    fi->t_finish = oops_time_get_us();
}

/* Logs the frame numbers of a paused or dump run, and in a paused run the pixel count
 * a depth toggle changed. */
static void log_full_scan(cube_t *c, const cube_frame_info_t *fi, const uint32_t *src) {
    const cube_frame_stats_t *st = &fi->stats;
    if (c->frame < 300u || (c->frame % 60u) == 0u)
        oops_log_info(TAG, "frame-hash 0x%08x", (unsigned)st->hash);
    if (c->frame == 0u) {
        oops_log_info(TAG, "frame-rot-x-deg 0x%08x", (unsigned)(int32_t)c->rot_x);
        oops_log_info(TAG, "frame-rot-y-deg 0x%08x", (unsigned)(int32_t)c->rot_y);
        oops_log_info(TAG, "frame-rot-z-deg 0x%08x", (unsigned)(int32_t)c->rot_z);
    }
    if (c->ctl.dump && c->frame == 30u) {
        oops_log_info(TAG, "oracle-hash 0x%08x", (unsigned)st->hash);
        oops_log_info(TAG, "oracle-mod-pixels 0x%08x", (unsigned)st->mod_pixels);
        oops_log_info(TAG, "oracle-center-pixel 0x%08x", (unsigned)st->center);
        oops_log_info(TAG, "oracle-depth-on 0x%08x", c->opt_depth ? 1u : 0u);
        oops_log_info(TAG, "oracle-cull-on 0x%08x", c->opt_cull ? 1u : 0u);
    }
    if (!c->ctl.pause || !src)
        return;
    const size_t words = (size_t)CUBE_FRAME_W * CUBE_FRAME_H;
    if (!c->prev_frame)
        c->prev_frame = (uint32_t *)oops_mem_alloc(words * 4, 64, OOPS_MEM_WB_ONION);
    if (!c->prev_frame)
        return;
    if (c->frame == 61u || c->frame == 91u) {
        uint32_t diff = 0;
        for (size_t p = 0; p < words; p++) {
            if (src[p] != c->prev_frame[p])
                diff++;
        }
        oops_log_info(TAG, "%s 0x%08x",
                      c->opt_depth ? "depth-on-minus-off-pixels"
                                   : "depth-off-minus-on-pixels",
                      (unsigned)diff);
    }
    memcpy(c->prev_frame, src, words * 4);
}

/* Measures the render target after the fence and before the HUD or the flip touch it.
 * Paused and dump runs hash every word; other runs sample one word in 64, and the HUD
 * labels which. The command processor's cached copy is read where there is one: a full
 * read of the uncached target drops the demo to two frames a second. */
static void measure_frame(cube_t *c, cube_frame_info_t *fi) {
    fi->full_scan = c->ctl.pause || c->ctl.dump;
    const uint32_t *src = oops_display_get_framebuffer(c->disp);
    /* Only the lines about to be read are invalidated: one in four when sampling, since
     * 64 words span four cache lines. */
    const GLuint *rb = glGetFrameReadbackSampled(fi->full_scan ? 1u : 4u);
    if (rb)
        src = rb;
    const uint64_t t0 = oops_time_get_us();
    fi->stats =
        cube_frame_measure(src, fi->full_scan ? (size_t)1 : (size_t)64, rb == NULL);
    fi->t_hash = oops_time_get_us();
    if (c->frame == 1u || (c->frame % 300u) == 0u)
        oops_log_info(TAG, "hash-us 0x%08x", (unsigned)(oops_time_get_us() - t0));
    if (fi->full_scan)
        log_full_scan(c, fi, src);
}

/* The shared dashboard (common/cube_hud.c), drawn after the measurement so the overlay
 * never enters it. */
static void draw_hud(cube_t *c, const cube_frame_info_t *fi) {
    gl_hw_status_t hw;
    memset(&hw, 0, sizeof(hw));
    glGetHardwareStatus(&hw);
    const uint32_t badge_col =
        hw.verified ? 0xff44ff88u : (hw.failed ? 0xffff5544u : 0xffffcc44u);

    /* This stack's GPU-verified badge, the frame hash (1:64 when sampled) and which
     * scanout tiler is live. */
    char status[96];
    oops_snprintf(status, sizeof(status), "%s%s0x%08x  tile %s",
                  hw.verified ? "GPU verified  hash "
                              : (hw.failed ? "GPU FAILED  hash " : "GPU ?  hash "),
                  fi->full_scan ? "" : "(1:64) ", (unsigned)fi->stats.hash,
                  oops_display_is_gpu_accelerated(c->disp) ? "GPU" : "CPU");

    oops_cube_hud_draw(c->hud,
                       &(oops_cube_hud_t){
                           .title = "GL1 CUBE",
                           .backend = oops_gfx_backend_name(),
                           .api = "OpenGL 1.x",
                           .build = OOPS_APP_VERSION,
                           .width = CUBE_FRAME_W,
                           .height = CUBE_FRAME_H,
                           .frame = (unsigned)c->frame,
                           .us_per_frame = (unsigned)c->last_frame_us,
                           .paused = !c->auto_rotate,
                           .mesh = fi->geom_name,
                           .tris = (unsigned)fi->tri_count,
                           .verts = (unsigned)fi->vcount,
                           .cull = c->opt_cull,
                           .depth = c->opt_depth,
                           .texture = c->opt_texture,
                           .lighting = c->opt_lighting,
                           .status = status,
                           .status_color = badge_col,
                           .controls =
                               "D-Pad orbit  X rotate  R2 mesh  /_\\ tex  [] depth  L1 "
                               "light  R1 cull  (O) quit",
                       });
}

/* Frame timing in phases at frame 5 and every 60th, and at frames 5 and 35 the GPU
 * clock the end-of-pipe event writes against the CPU clock: its rate is data. */
static void log_timing(cube_t *c, const cube_frame_info_t *fi) {
    static uint64_t clk0 = 0, us0 = 0;
    if (c->frame == 5u || c->frame == 35u) {
        gl_hw_status_t st;
        glGetHardwareStatus(&st);
        const uint64_t clk = ((uint64_t)st.timestamp_hi << 32) | st.timestamp_lo;
        const uint64_t us = oops_time_get_us();
        if (c->frame == 5u) {
            clk0 = clk;
            us0 = us;
        } else if (us > us0) {
            oops_log_info(TAG, "gpu-clock-delta-lo 0x%08x", (unsigned)(clk - clk0));
            oops_log_info(TAG, "cpu-us-delta 0x%08x", (unsigned)(us - us0));
            oops_log_info(TAG, "gpu-clock-per-us 0x%08x",
                          (unsigned)((clk - clk0) / (us - us0)));
        }
    }
    if (c->frame == 5u || (c->frame % 60u) == 0u) {
        const uint64_t t_end = oops_time_get_us();
        c->last_frame_us = (uint32_t)(t_end - fi->t_top);
        oops_log_info(TAG, "t-draw-finish-us 0x%08x",
                      (unsigned)(fi->t_finish - fi->t_top));
        oops_log_info(TAG, "t-hash-us 0x%08x", (unsigned)(fi->t_hash - fi->t_finish));
        oops_log_info(TAG, "t-hud-us 0x%08x", (unsigned)(fi->t_hud - fi->t_hash));
        oops_log_info(TAG, "t-swap-us 0x%08x", (unsigned)(t_end - fi->t_hud));
    }
}

/* One frame, top to swap. Returns false when the run should end. */
static bool cube_frame(cube_t *c) {
    cube_frame_info_t fi;
    memset(&fi, 0, sizeof(fi));
    fi.t_top = oops_time_get_us();

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
    if (c->ctl.dump && c->frame == 30u)
        glRequestHardwareDump(); /* this frame's stream becomes the oracle record */
    if (c->ctl.pause && (c->frame == 60u || c->frame == 90u))
        set_cap(GL_DEPTH_TEST, c->opt_depth = !c->opt_depth);
    advance_rotation(c);

    draw_geometry(c, &fi);
    measure_frame(c, &fi);
    draw_hud(c, &fi);
    fi.t_hud = oops_time_get_us();

    (void)oops_gfx_present(c->gfx);
    c->last_frame_us = (uint32_t)(oops_time_get_us() - fi.t_top);
    log_timing(c, &fi);
    c->frame++;
    return true;
}

static void cube_shutdown(cube_t *c) {
    oops_mesh_free(&c->torus);
    oops_mesh_free(&c->sphere);
    glDeleteTextures(1, &c->tex_id);
    oops_hud_destroy(c->hud);
    oops_gfx_destroy(c->gfx);
    oops_log_info(TAG, "gl1-cube session closed cleanly");
}

int gl1_cube_start(const payload_args_t *args);

__attribute__((visibility("default"))) int gl1_cube_start(const payload_args_t *args) {
    static cube_t cube;
    if (!cube_init(&cube, args))
        return -1;
    while (cube_frame(&cube)) {
    }
    cube_shutdown(&cube);
    return 0;
}
