/*
 * gl-cube: Clean-room 3D Colored Cube Demo using oops-gl (OpenGL 1.3 / GLU)
 * Target: Sony PlayStation 5 (AMD RDNA2 GFX10.3 / Prospero FW 12.40)
 */

#include "GL/gl.h"
#include "GL/glu.h"
#include "oops/display.h"
#include "oops/draw.h"
#include "oops/input.h"
#include "oops/time.h"
#include "oops/syscall.h"
#include "oops/freestd.h"
#include "oops/krw.h"
#include "obj_loader.h"
#include <stdbool.h>

typedef enum {
    GEOM_CUBE = 0,
    GEOM_TORUS = 1,
    GEOM_SPHERE = 2,
    GEOM_MAX
} geom_mode_t;

#ifndef OOPS_HOST_BUILD
static void cube_klog(const char *msg) {
    char buf[160];
    const char *pfx = "[GL-CUBE] ";
    int n = 0;
    while (pfx[n] && n < 16) { buf[n] = pfx[n]; n++; }
    int m = 0;
    while (msg[m] && n < (int)sizeof(buf) - 2) { buf[n++] = msg[m++]; }
    buf[n++] = '\n';
    buf[n] = '\0';
    (void)sys_call(SYS_klog, 7, (long)buf, 0, 0, 0, 0);
}
#endif

/* 36 vertices defining 12 triangles of a 3D unit cube centered at origin */
static const GLfloat s_cube_vertices[36 * 3] = {
    /* Front Face (Z = +1.0) */
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,

    /* Back Face (Z = -1.0) */
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,

    /* Top Face (Y = +1.0) */
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    /* Bottom Face (Y = -1.0) */
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    /* Right Face (X = +1.0) */
     1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,

    /* Left Face (X = -1.0) */
    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f
};

/* Vivid per-vertex colors for hardware Gouraud shading */
static const GLfloat s_cube_colors[36 * 3] = {
    /* Front Face (Z = +1.0) */
    0.15f, 0.15f, 1.00f,
    1.00f, 0.15f, 1.00f,
    1.00f, 1.00f, 1.00f,
    0.15f, 0.15f, 1.00f,
    1.00f, 1.00f, 1.00f,
    0.15f, 1.00f, 1.00f,

    /* Back Face (Z = -1.0) */
    0.15f, 0.15f, 0.15f,
    0.15f, 1.00f, 0.15f,
    1.00f, 1.00f, 0.15f,
    0.15f, 0.15f, 0.15f,
    1.00f, 1.00f, 0.15f,
    1.00f, 0.15f, 0.15f,

    /* Top Face (Y = +1.0) */
    0.15f, 1.00f, 0.15f,
    0.15f, 1.00f, 1.00f,
    1.00f, 1.00f, 1.00f,
    0.15f, 1.00f, 0.15f,
    1.00f, 1.00f, 1.00f,
    1.00f, 1.00f, 0.15f,

    /* Bottom Face (Y = -1.0) */
    0.15f, 0.15f, 0.15f,
    1.00f, 0.15f, 0.15f,
    1.00f, 0.15f, 1.00f,
    0.15f, 0.15f, 0.15f,
    1.00f, 0.15f, 1.00f,
    0.15f, 0.15f, 1.00f,

    /* Right Face (X = +1.0) */
    1.00f, 0.15f, 1.00f,
    1.00f, 0.15f, 0.15f,
    1.00f, 1.00f, 0.15f,
    1.00f, 0.15f, 1.00f,
    1.00f, 1.00f, 0.15f,
    1.00f, 1.00f, 1.00f,

    /* Left Face (X = -1.0) */
    0.15f, 0.15f, 0.15f,
    0.15f, 0.15f, 1.00f,
    0.15f, 1.00f, 1.00f,
    0.15f, 0.15f, 0.15f,
    0.15f, 1.00f, 1.00f,
    0.15f, 1.00f, 0.15f
};

/* UV texture coordinates for 36 vertices (6 faces * 2 triangles * 3 vertices) */
static const GLfloat s_cube_texcoords[36 * 2] = {
    /* Front Face */
    0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
    0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,

    /* Back Face */
    0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
    0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,

    /* Top Face */
    0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
    0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,

    /* Bottom Face */
    0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
    0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,

    /* Right Face */
    0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
    0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,

    /* Left Face */
    0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
    0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f
};

/* 36 surface normal vectors for 6 cube faces */
static const GLfloat s_cube_normals[36 * 3] = {
    /* Front Face (Z = +1.0) */
     0.0f,  0.0f,  1.0f,   0.0f,  0.0f,  1.0f,   0.0f,  0.0f,  1.0f,
     0.0f,  0.0f,  1.0f,   0.0f,  0.0f,  1.0f,   0.0f,  0.0f,  1.0f,

    /* Back Face (Z = -1.0) */
     0.0f,  0.0f, -1.0f,   0.0f,  0.0f, -1.0f,   0.0f,  0.0f, -1.0f,
     0.0f,  0.0f, -1.0f,   0.0f,  0.0f, -1.0f,   0.0f,  0.0f, -1.0f,

    /* Top Face (Y = +1.0) */
     0.0f,  1.0f,  0.0f,   0.0f,  1.0f,  0.0f,   0.0f,  1.0f,  0.0f,
     0.0f,  1.0f,  0.0f,   0.0f,  1.0f,  0.0f,   0.0f,  1.0f,  0.0f,

    /* Bottom Face (Y = -1.0) */
     0.0f, -1.0f,  0.0f,   0.0f, -1.0f,  0.0f,   0.0f, -1.0f,  0.0f,
     0.0f, -1.0f,  0.0f,   0.0f, -1.0f,  0.0f,   0.0f, -1.0f,  0.0f,

    /* Right Face (X = +1.0) */
     1.0f,  0.0f,  0.0f,   1.0f,  0.0f,  0.0f,   1.0f,  0.0f,  0.0f,
     1.0f,  0.0f,  0.0f,   1.0f,  0.0f,  0.0f,   1.0f,  0.0f,  0.0f,

    /* Left Face (X = -1.0) */
    -1.0f,  0.0f,  0.0f,  -1.0f,  0.0f,  0.0f,  -1.0f,  0.0f,  0.0f,
    -1.0f,  0.0f,  0.0f,  -1.0f,  0.0f,  0.0f,  -1.0f,  0.0f,  0.0f
};

#define TEX_DIM 64
static uint32_t s_cube_texture[TEX_DIM * TEX_DIM];

static void generate_cube_texture(void) {
    for (int y = 0; y < TEX_DIM; y++) {
        for (int x = 0; x < TEX_DIM; x++) {
            int cx = (x / 8) & 1;
            int cy = (y / 8) & 1;
            bool tile = (cx ^ cy) != 0;
            bool border = (x % 8 == 0) || (y % 8 == 0);

            uint8_t r, g, b, a = 255;
            if (border) {
                r = 255; g = 215; b = 0;   /* Gold grid lines */
            } else if (tile) {
                r = 240; g = 240; b = 255; /* Bright white-blue tile */
            } else {
                r = 30;  g = 60;  b = 140; /* Deep cobalt tile */
            }
            s_cube_texture[y * TEX_DIM + x] = ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
        }
    }
}

static void int_to_str(int val, char *buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char tmp[16];
    int pos = 0;
    int v = val < 0 ? -val : val;
    while (v > 0) {
        tmp[pos++] = (char)('0' + (v % 10));
        v /= 10;
    }
    int out = 0;
    if (val < 0) buf[out++] = '-';
    for (int i = pos - 1; i >= 0; i--) {
        buf[out++] = tmp[i];
    }
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

int gl_cube_start(const payload_args_t *args);

__attribute__((visibility("default"))) int gl_cube_start(const payload_args_t *args) {
#ifndef OOPS_HOST_BUILD
    if (args) {
        sys_call_init(args);
    }
    cube_klog("starting gl-cube (Stage 2 oops-gl 3D Cube demo)...");
#endif

    /* 1. Open display via AGC backend */
    oops_display_t *disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, 1920, 1080);
    if (!disp || !oops_display_is_ready(disp)) {
#ifndef OOPS_HOST_BUILD
        cube_klog("failed to open display");
#endif
        return -1;
    }

    /* 2. Create OpenGL context */
    void *gl_ctx = glContextCreate(disp);
    if (!gl_ctx) {
#ifndef OOPS_HOST_BUILD
        cube_klog("failed to create oops-gl context");
#endif
        oops_display_close(disp);
        return -2;
    }
    glContextMakeCurrent(gl_ctx);

    /* 3. Configure OpenGL state machine */
    /* Configure initial OpenGL state */
    glViewport(0, 0, 1920, 1080);
    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glShadeModel(GL_SMOOTH);

    /* Generate and upload 2D procedural test texture */
    generate_cube_texture();
    GLuint tex_id = 0;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_DIM, TEX_DIM, 0, GL_RGBA, GL_UNSIGNED_BYTE, s_cube_texture);
    glDisable(GL_TEXTURE_2D);

    /* 4. Configure Fixed-Function Lighting & Materials (Stage 6) */
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    float light_pos[4] = {2.0f, 3.5f, 4.0f, 1.0f};
    float light_diff[4] = {1.0f, 0.96f, 0.90f, 1.0f};
    float light_amb[4] = {0.25f, 0.25f, 0.30f, 1.0f};
    float light_spec[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diff);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_amb);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_spec);

    float mat_spec[4] = {0.9f, 0.9f, 0.9f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 32.0f);

    /* Bind vertex, normal, color, and texture coordinate arrays */
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, s_cube_vertices);
    glNormalPointer(GL_FLOAT, 0, s_cube_normals);
    glColorPointer(3, GL_FLOAT, 0, s_cube_colors);
    glTexCoordPointer(2, GL_FLOAT, 0, s_cube_texcoords);

    /* Initialize procedural meshes (Torus & Sphere) */
    oops_mesh_t torus_mesh;
    memset(&torus_mesh, 0, sizeof(torus_mesh));
    (void)oops_mesh_create_torus(&torus_mesh, 24, 16, 0.75f, 0.35f);

    oops_mesh_t sphere_mesh;
    memset(&sphere_mesh, 0, sizeof(sphere_mesh));
    (void)oops_mesh_create_sphere(&sphere_mesh, 20, 20, 0.95f);

    /* Initialize controller input */
    oops_input_init();

    geom_mode_t geom_mode = GEOM_CUBE;
    float rot_x = 25.0f;
    float rot_y = 35.0f;
    float rot_z = 10.0f;
    float cam_dist = -4.5f;
    bool auto_rotate = true;
    bool opt_texture = false;
    bool opt_depth = false;
    bool opt_lighting = true;
    bool opt_cull = true;
    uint32_t prev_buttons = 0;
    uint64_t frame = 0;
    bool running = true;

#ifndef OOPS_HOST_BUILD
    cube_klog("entering 3D rendering loop at 60 FPS...");
#endif

    while (running) {
        /* Poll DualSense controller */
        oops_pad_state_t pad;
        if (oops_input_poll(0, &pad) == 0 && pad.connected) {
            uint32_t pressed = pad.buttons & ~prev_buttons;
            prev_buttons = pad.buttons;

            /* Exit combo: L1 + R1 + OPTIONS or CIRCLE */
            if (((pad.buttons & OOPS_BUTTON_L1) &&
                 (pad.buttons & OOPS_BUTTON_R1) &&
                 (pad.buttons & OOPS_BUTTON_OPTIONS)) ||
                (pad.buttons & OOPS_BUTTON_CIRCLE) ||
                (pad.buttons & OOPS_BUTTON_OPTIONS)) {
#ifndef OOPS_HOST_BUILD
                cube_klog("exit combo received, terminating cleanly");
#endif
                running = false;
                break;
            }

            /* Toggle auto-rotation with CROSS */
            if (pressed & OOPS_BUTTON_CROSS) {
                auto_rotate = !auto_rotate;
            }

            /* Toggle Texture 2D with TRIANGLE */
            if (pressed & OOPS_BUTTON_TRIANGLE) {
                opt_texture = !opt_texture;
                if (opt_texture) glEnable(GL_TEXTURE_2D);
                else glDisable(GL_TEXTURE_2D);
            }

            /* Toggle Depth Test with SQUARE */
            if (pressed & OOPS_BUTTON_SQUARE) {
                opt_depth = !opt_depth;
                if (opt_depth) glEnable(GL_DEPTH_TEST);
                else glDisable(GL_DEPTH_TEST);
            }

            /* Toggle Lighting with L1 */
            if (pressed & OOPS_BUTTON_L1) {
                opt_lighting = !opt_lighting;
                if (opt_lighting) glEnable(GL_LIGHTING);
                else glDisable(GL_LIGHTING);
            }

            /* Toggle Culling with R1 */
            if (pressed & OOPS_BUTTON_R1) {
                opt_cull = !opt_cull;
                if (opt_cull) glEnable(GL_CULL_FACE);
                else glDisable(GL_CULL_FACE);
            }

            /* Cycle geometry mesh with R2 or L2 */
            if (pressed & (OOPS_BUTTON_R2 | OOPS_BUTTON_L2)) {
                geom_mode = (geom_mode_t)((geom_mode + 1) % GEOM_MAX);
            }

            /* Manual control: D-Pad / Analog sticks */
            if (pad.buttons & OOPS_BUTTON_LEFT)  rot_y -= 2.0f;
            if (pad.buttons & OOPS_BUTTON_RIGHT) rot_y += 2.0f;
            if (pad.buttons & OOPS_BUTTON_UP)    rot_x -= 2.0f;
            if (pad.buttons & OOPS_BUTTON_DOWN)  rot_x += 2.0f;
        } else {
            prev_buttons = 0;
        }

        if (auto_rotate) {
            rot_x += 0.75f;
            rot_y += 1.25f;
            rot_z += 0.50f;
            if (rot_x >= 360.0f) rot_x -= 360.0f;
            if (rot_y >= 360.0f) rot_y -= 360.0f;
            if (rot_z >= 360.0f) rot_z -= 360.0f;
        }

        /* 1. Clear Color and Depth Buffers */
        glClearColor(0.05f, 0.07f, 0.12f, 1.0f);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        /* 2. Setup Perspective Projection Matrix */
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0, 1920.0 / 1080.0, 0.1, 100.0);

        /* 3. Setup ModelView Matrix Stack */
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, cam_dist);

        /* Set light in camera space before rotation so light remains stationary while cube rotates */
        glLightfv(GL_LIGHT0, GL_POSITION, light_pos);

        glRotatef(rot_x, 1.0f, 0.0f, 0.0f);
        glRotatef(rot_y, 0.0f, 1.0f, 0.0f);
        glRotatef(rot_z, 0.0f, 0.0f, 1.0f);

        /* 4. Render Active 3D Geometry */
        GLsizei vcount = 0;
        const char *geom_name = "CUBE";
        size_t tri_count = 12;

        switch (geom_mode) {
        case GEOM_CUBE:
            glVertexPointer(3, GL_FLOAT, 0, s_cube_vertices);
            glNormalPointer(GL_FLOAT, 0, s_cube_normals);
            glColorPointer(3, GL_FLOAT, 0, s_cube_colors);
            glTexCoordPointer(2, GL_FLOAT, 0, s_cube_texcoords);
            vcount = 36;
            geom_name = "CUBE";
            tri_count = 12;
            break;
        case GEOM_TORUS:
            if (torus_mesh.positions) {
                glVertexPointer(3, GL_FLOAT, 0, torus_mesh.positions);
                glNormalPointer(GL_FLOAT, 0, torus_mesh.normals);
                glColorPointer(3, GL_FLOAT, 0, torus_mesh.colors);
                glTexCoordPointer(2, GL_FLOAT, 0, torus_mesh.texcoords);
                vcount = (GLsizei)torus_mesh.vertex_count;
                geom_name = "TORUS";
                tri_count = torus_mesh.triangle_count;
            }
            break;
        case GEOM_SPHERE:
            if (sphere_mesh.positions) {
                glVertexPointer(3, GL_FLOAT, 0, sphere_mesh.positions);
                glNormalPointer(GL_FLOAT, 0, sphere_mesh.normals);
                glColorPointer(3, GL_FLOAT, 0, sphere_mesh.colors);
                glTexCoordPointer(2, GL_FLOAT, 0, sphere_mesh.texcoords);
                vcount = (GLsizei)sphere_mesh.vertex_count;
                geom_name = "SPHERE";
                tri_count = sphere_mesh.triangle_count;
            }
            break;
        default:
            break;
        }

        if (vcount > 0) {
            glDrawArrays(GL_TRIANGLES, 0, vcount);
        }
        glFinish();
        (void)geom_name;
        (void)tri_count;

        /* Sample center pixel and modified pixel count AFTER GPU hardware rasterization */
        uint32_t *fb = oops_display_get_framebuffer(disp);
        uint32_t center_pix = 0;
        uint32_t mod_pixels = 0;
        if (fb) {
#if defined(__x86_64__)
            __builtin_ia32_clflush((const void *)&fb[540 * 1920 + 960]);
            for (size_t p = 0; p < (size_t)1920 * 1080; p += 64) {
                __builtin_ia32_clflush((const void *)&fb[p]);
            }
#endif
            center_pix = fb[540 * 1920 + 960];
            for (size_t p = 0; p < (size_t)1920 * 1080; p += 64) {
                if (fb[p] != 0xff0d121fu) {
                    mod_pixels++;
                }
            }
        }

        /* 5. Render 2D Telemetry & Diagnostics HUD */
        oops_surface_t surf = oops_display_get_surface(disp);

        /* Outer high-contrast framing borders (12px) */
        oops_draw_rect(&surf, 0, 0, 1920, 12, OOPS_COLOR_CYAN);
        oops_draw_rect(&surf, 0, 1068, 1920, 12, OOPS_COLOR_CYAN);
        oops_draw_rect(&surf, 0, 0, 12, 1080, OOPS_COLOR_CYAN);
        oops_draw_rect(&surf, 1908, 0, 12, 1080, OOPS_COLOR_CYAN);

        /* Header banner panel */
        oops_draw_rect_blend(&surf, 40, 30, 820, 260, 0xd008101cu);
        oops_draw_rect(&surf, 40, 30, 820, 3, OOPS_COLOR_CYAN);

        oops_draw_text(&surf, 60, 48, "OOPS-GL 2.0: 3D MULTI-MESH (RDNA2)", OOPS_COLOR_WHITE, 2);
        oops_draw_text(&surf, 60, 75, "RDNA2 Freestanding Translation Layer", 0xff66ccffu, 1);
        oops_draw_text(&surf, 60, 96, "Target: Prospero/Trinity (FW 12.40)", 0xffaaaaaau, 1);
        oops_draw_text(&surf, 60, 116, "Pipeline: AGC RDNA2 HW Lit (Blinn-Phong) & NGG", 0xffffcc44u, 1);

        /* Canaries */
        char h_vs[16], h_ps[16];
        GLuint can_vs = 0, can_ps = 0, can_vs_s0 = 0, can_ps_s0 = 0;
#ifndef OOPS_HOST_BUILD
        glGetCanaryEx(&can_vs, &can_ps, &can_vs_s0, &can_ps_s0);
#endif
        hex_to_str(can_vs, h_vs);
        hex_to_str(can_ps, h_ps);

        char stat_buf[128];
        char num_buf[16];

        /* VS & PS Canary row */
        memcpy(stat_buf, "VS Can: ", 8);
        int sl = 8;
        for (int i = 0; h_vs[i]; i++) stat_buf[sl++] = h_vs[i];
        memcpy(&stat_buf[sl], "  |  PS Can: ", 13);
        sl += 13;
        for (int i = 0; h_ps[i]; i++) stat_buf[sl++] = h_ps[i];
        stat_buf[sl] = '\0';
        oops_draw_text(&surf, 60, 138, stat_buf, (can_vs == 0xbeef0001u) ? OOPS_COLOR_GREEN : OOPS_COLOR_YELLOW, 1);

        /* Pixels Mod & Center Pix */
        char h_pix[16];
        hex_to_str(center_pix, h_pix);
        int_to_str((int)mod_pixels, num_buf);
        memcpy(stat_buf, "Mod Pix: ", 9);
        sl = 9;
        for (int i = 0; num_buf[i]; i++) stat_buf[sl++] = num_buf[i];
        memcpy(&stat_buf[sl], "  |  Center: ", 13);
        sl += 13;
        for (int i = 0; h_pix[i]; i++) stat_buf[sl++] = h_pix[i];
        stat_buf[sl] = '\0';
        oops_draw_text(&surf, 60, 160, stat_buf, (mod_pixels > 0) ? OOPS_COLOR_GREEN : OOPS_COLOR_RED, 1);

        /* Pipeline State switches */
        memcpy(stat_buf, "Cull: ", 6);
        sl = 6;
        if (opt_cull) { memcpy(&stat_buf[sl], "ON(CCW)", 7); sl += 7; }
        else { memcpy(&stat_buf[sl], "OFF", 3); sl += 3; }
        memcpy(&stat_buf[sl], " | Light: ", 10); sl += 10;
        if (opt_lighting) { memcpy(&stat_buf[sl], "ON", 2); sl += 2; }
        else { memcpy(&stat_buf[sl], "OFF", 3); sl += 3; }
        memcpy(&stat_buf[sl], " | Tex: ", 8); sl += 8;
        if (opt_texture) { memcpy(&stat_buf[sl], "ON", 2); sl += 2; }
        else { memcpy(&stat_buf[sl], "OFF", 3); sl += 3; }
        memcpy(&stat_buf[sl], " | Depth: ", 10); sl += 10;
        if (opt_depth) { memcpy(&stat_buf[sl], "ON", 2); sl += 2; }
        else { memcpy(&stat_buf[sl], "OFF", 3); sl += 3; }
        stat_buf[sl] = '\0';
        oops_draw_text(&surf, 60, 182, stat_buf, OOPS_COLOR_CYAN, 1);

        /* Rotation & Frame telemetry */
        int_to_str((int)rot_x, num_buf);
        memcpy(stat_buf, "RotX: ", 6);
        sl = 6;
        for (int i = 0; num_buf[i]; i++) stat_buf[sl++] = num_buf[i];
        memcpy(&stat_buf[sl], " deg  RotY: ", 12);
        sl += 12;
        int_to_str((int)rot_y, num_buf);
        for (int i = 0; num_buf[i]; i++) stat_buf[sl++] = num_buf[i];
        memcpy(&stat_buf[sl], " deg  |  Frame: ", 16);
        sl += 16;
        int_to_str((int)frame, num_buf);
        for (int i = 0; num_buf[i]; i++) stat_buf[sl++] = num_buf[i];
        memcpy(&stat_buf[sl], " (60 FPS)", 9);
        sl += 9;
        stat_buf[sl] = '\0';
        oops_draw_text(&surf, 60, 204, stat_buf, OOPS_COLOR_WHITE, 1);

        /* Geometry & Complexity telemetry */
        memcpy(stat_buf, "Mesh: ", 6);
        sl = 6;
        for (int i = 0; geom_name[i]; i++) stat_buf[sl++] = geom_name[i];
        memcpy(&stat_buf[sl], "  |  Tris: ", 11);
        sl += 11;
        int_to_str((int)tri_count, num_buf);
        for (int i = 0; num_buf[i]; i++) stat_buf[sl++] = num_buf[i];
        memcpy(&stat_buf[sl], "  |  Verts: ", 12);
        sl += 12;
        int_to_str((int)vcount, num_buf);
        for (int i = 0; num_buf[i]; i++) stat_buf[sl++] = num_buf[i];
        stat_buf[sl] = '\0';
        oops_draw_text(&surf, 60, 226, stat_buf, 0xffffaa33u, 1);

        /* Footer controls banner */
        oops_draw_rect_blend(&surf, 40, 1000, 1100, 50, 0xd008101cu);
        oops_draw_rect(&surf, 40, 1000, 1100, 2, 0xff335577u);
        oops_draw_text(&surf, 60, 1018,
                       "Controls: D-Pad=Orbit | [X]=Rotate | [R2]=Mesh | [/\\ ]=Tex | [][]=Depth | [L1]=Light | [R1]=Cull | (O)=Exit",
                       0xffeeeeeeu, 1);

        /* 6. Present Frame */
        glSwapBuffers();

        frame++;
    }

    /* Clean up */
    oops_mesh_free(&torus_mesh);
    oops_mesh_free(&sphere_mesh);
    glDeleteTextures(1, &tex_id);
    glContextDestroy(gl_ctx);
    oops_display_close(disp);

#ifndef OOPS_HOST_BUILD
    cube_klog("gl-cube session closed cleanly");
#endif
    return 0;
}
