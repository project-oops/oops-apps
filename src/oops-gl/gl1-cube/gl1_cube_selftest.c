/*
 * Host self-test for gl1-cube.
 *
 * Runs the payload's scene through the real oops-gl software path: the same vertices,
 * colours, texture coordinates, normals and procedural texture the console draws, from
 * gl1_cube_scene.h, with the same state the payload configures - depth, culling,
 * texturing and lighting. Lighting (GL_LIGHTING with GL_COLOR_MATERIAL and a specular
 * term) is checked to change the frame, so a rasteriser that ignored it would fail.
 */

#include "GL/gl.h"
#include "GL/glu.h"
#include "oops/display.h"
#include "oops/memory.h"
#include "obj_loader.h"
#include "gl1_cube_scene.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define FB_W 640
#define FB_H 480

static uint32_t s_test_texture[TEX_DIM * TEX_DIM];
static uint32_t s_host_fb[FB_W * FB_H];
static int s_host_disp_dummy = 1;
static unsigned int s_host_w = FB_W;
static unsigned int s_host_h = FB_H;

oops_display_t *oops_display_open(oops_display_backend_t backend, unsigned int width,
                                  unsigned int height) {
    (void)backend;
    s_host_w = width ? width : FB_W;
    s_host_h = height ? height : FB_H;
    return (oops_display_t *)&s_host_disp_dummy;
}

void oops_display_close(oops_display_t *disp) {
    (void)disp;
}

int oops_display_flip(oops_display_t *disp) {
    (void)disp;
    return 0;
}

uint32_t *oops_display_get_framebuffer(oops_display_t *disp) {
    (void)disp;
    return s_host_fb;
}

unsigned int oops_display_get_width(const oops_display_t *disp) {
    (void)disp;
    return s_host_w;
}

unsigned int oops_display_get_height(const oops_display_t *disp) {
    (void)disp;
    return s_host_h;
}

/* obj_loader allocates through the SDK's direct-memory allocator, which has no host
 * implementation. Alignment is dropped: these are arrays of floats, not GPU resources.
 */
void *oops_mem_alloc(size_t size, size_t alignment, oops_mem_type_t type) {
    (void)alignment;
    (void)type;
    return malloc(size);
}

void oops_mem_free(void *ptr) {
    free(ptr);
}

static uint32_t px(int x, int y) {
    return s_host_fb[(size_t)y * FB_W + (size_t)x];
}

/* Pixels outside the clear colour: "did this draw anything". */
static size_t drawn_pixels(uint32_t clear) {
    size_t n = 0;
    for (size_t p = 0; p < (size_t)FB_W * FB_H; p++) {
        if (s_host_fb[p] != clear)
            n++;
    }
    return n;
}

static size_t differing_pixels(const uint32_t *a, const uint32_t *b) {
    size_t n = 0;
    for (size_t p = 0; p < (size_t)FB_W * FB_H; p++) {
        if (a[p] != b[p])
            n++;
    }
    return n;
}

/* The payload's scene state, applied to the current context. */
static void bind_cube_arrays(void) {
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, s_cube_vertices);
    glNormalPointer(GL_FLOAT, 0, s_cube_normals);
    glColorPointer(3, GL_FLOAT, 0, s_cube_colors);
    glTexCoordPointer(2, GL_FLOAT, 0, s_cube_texcoords);
}

static void bind_mesh_arrays(const oops_mesh_t *m) {
    glVertexPointer(3, GL_FLOAT, 0, m->positions);
    glNormalPointer(GL_FLOAT, 0, m->normals);
    glColorPointer(3, GL_FLOAT, 0, m->colors);
    glTexCoordPointer(2, GL_FLOAT, 0, m->texcoords);
}

static void load_scene_matrices(float rot_deg) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)FB_W / (double)FB_H, 0.1, 100.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -4.5f);
    glRotatef(rot_deg, 1.0f, 1.0f, 0.0f);
}

int main(void) {
    int rc = 1;
    void *ctx = NULL;
    GLuint tex_id = 0;
    uint32_t *first_pass = NULL;
    oops_mesh_t torus, sphere;
    memset(&torus, 0, sizeof(torus));
    memset(&sphere, 0, sizeof(sphere));

    /* 1. Verify procedural texture synthesis */
    generate_cube_texture(s_test_texture);
    if (s_test_texture[0] == 0) {
        fprintf(stderr, "gl1-cube selftest: failed texture generation\n");
        return 1;
    }

    /* 2. Open display and initialize OpenGL context */
    oops_display_t *disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, FB_W, FB_H);
    if (!disp) {
        fprintf(stderr, "gl1-cube selftest: failed to open display\n");
        return 1;
    }

    ctx = glContextCreate(disp);
    if (!ctx) {
        fprintf(stderr, "gl1-cube selftest: failed to create gl context\n");
        oops_display_close(disp);
        return 1;
    }
    glContextMakeCurrent(ctx);

    /* 3. Setup viewport, depth, culling, and state - as gl1_cube_main.c does */
    glViewport(0, 0, FB_W, FB_H);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glShadeModel(GL_SMOOTH);

    /* 4. Texture upload */
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_DIM, TEX_DIM, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, s_test_texture);
    if (glGetError() != GL_NO_ERROR) {
        fprintf(stderr, "gl1-cube selftest: texture upload raised an error\n");
        goto done;
    }
    glEnable(GL_TEXTURE_2D);

    /* 5. Fixed-function lighting and materials - the payload's own values */
    {
        float light_pos[4] = {2.0f, 3.5f, 4.0f, 1.0f};
        float light_diff[4] = {1.0f, 0.96f, 0.90f, 1.0f};
        float light_amb[4] = {0.25f, 0.25f, 0.30f, 1.0f};
        float light_spec[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        float mat_spec[4] = {0.9f, 0.9f, 0.9f, 1.0f};

        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_NORMALIZE);
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diff);
        glLightfv(GL_LIGHT0, GL_AMBIENT, light_amb);
        glLightfv(GL_LIGHT0, GL_SPECULAR, light_spec);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_spec);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 32.0f);
    }
    if (glGetError() != GL_NO_ERROR) {
        fprintf(stderr, "gl1-cube selftest: lighting setup raised an error\n");
        goto done;
    }

    /* 6. Render the lit, textured cube through client vertex arrays */
    load_scene_matrices(30.0f);
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    bind_cube_arrays();
    glDrawArrays(GL_TRIANGLES, 0, 36);

    /* 7. Verify framebuffer has rendered output */
    if (!oops_display_get_framebuffer(disp)) {
        fprintf(stderr, "gl1-cube selftest: no framebuffer\n");
        goto done;
    }

    /* Check clear color background at corner (x=10, y=10) (RGBA8 unpacked from col) */
    uint32_t corner_pixel = px(10, 10);
    uint32_t corner_b = corner_pixel & 0xffu;
    uint32_t corner_g = (corner_pixel >> 8) & 0xffu;
    uint32_t corner_r = (corner_pixel >> 16) & 0xffu;
    if (corner_b < 40 || corner_r > 35 || corner_g > 35) {
        fprintf(stderr, "gl1-cube selftest: corner clear color unexpected (0x%08x)\n",
                corner_pixel);
        goto done;
    }

    /* Check center pixel: cube should have drawn over background */
    uint32_t lit_center = px(FB_W / 2, FB_H / 2);
    if (lit_center == corner_pixel || lit_center == 0) {
        fprintf(stderr, "gl1-cube selftest: center pixel not rendered (got 0x%08x)\n",
                lit_center);
        goto done;
    }

    size_t lit_drawn = drawn_pixels(corner_pixel);
    if (lit_drawn < 1000) {
        fprintf(stderr, "gl1-cube selftest: only %zu pixels drawn, expected a cube\n",
                lit_drawn);
        goto done;
    }

    /* Keep this frame: step 11 redraws it and the two must match word for word. */
    first_pass = (uint32_t *)malloc(sizeof(s_host_fb));
    if (!first_pass) {
        fprintf(stderr, "gl1-cube selftest: out of memory\n");
        goto done;
    }
    memcpy(first_pass, s_host_fb, sizeof(s_host_fb));

    /* 8. Lighting changes the frame: the same geometry drawn unlit must differ, since
     * the cube is coloured either way. */
    glDisable(GL_LIGHTING);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    size_t lighting_delta = differing_pixels(first_pass, s_host_fb);
    if (lighting_delta == 0) {
        fprintf(
            stderr,
            "gl1-cube selftest: GL_LIGHTING changed no pixel; the light is ignored\n");
        goto done;
    }
    glEnable(GL_LIGHTING);

    /* 9. Verify dynamic color mask and culling state */
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_TEXTURE_2D);
    /* Mask out Red and Blue, allowing only Green and Alpha */
    glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    uint32_t masked_pixel = px(FB_W / 2, FB_H / 2);
    uint32_t masked_b = masked_pixel & 0xffu;
    uint32_t masked_r = (masked_pixel >> 16) & 0xffu;
    uint32_t masked_g = (masked_pixel >> 8) & 0xffu;
    if (masked_b != 0 || masked_r != 0 || masked_g == 0) {
        fprintf(stderr, "gl1-cube selftest: color mask failure (got 0x%08x)\n",
                masked_pixel);
        goto done;
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    /* 10. The torus and sphere R2 cycles to build real, non-degenerate meshes. */
    glEnable(GL_TEXTURE_2D);
    if (oops_mesh_create_torus(&torus, 24, 16, 0.75f, 0.35f) != 0 || !torus.positions) {
        fprintf(stderr, "gl1-cube selftest: torus generation failed\n");
        goto done;
    }
    if (oops_mesh_create_sphere(&sphere, 20, 20, 0.95f) != 0 || !sphere.positions) {
        fprintf(stderr, "gl1-cube selftest: sphere generation failed\n");
        goto done;
    }
    if (torus.vertex_count != torus.triangle_count * 3u ||
        sphere.vertex_count != sphere.triangle_count * 3u) {
        fprintf(stderr,
                "gl1-cube selftest: mesh vertex and triangle counts disagree "
                "(torus %zu/%zu, sphere %zu/%zu)\n",
                torus.vertex_count, torus.triangle_count, sphere.vertex_count,
                sphere.triangle_count);
        goto done;
    }

    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    const struct {
        const char *name;
        const oops_mesh_t *mesh;
    } meshes[] = {{"torus", &torus}, {"sphere", &sphere}};
    for (size_t i = 0; i < sizeof(meshes) / sizeof(meshes[0]); i++) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        bind_mesh_arrays(meshes[i].mesh);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)meshes[i].mesh->vertex_count);
        size_t n = drawn_pixels(px(10, 10));
        if (n < 1000) {
            fprintf(stderr, "gl1-cube selftest: %s drew only %zu pixels\n",
                    meshes[i].name, n);
            goto done;
        }
    }

    /* 11. The same scene drawn again is the same frame: no state carries between draws,
     * so the oracle record's frame hash is reproducible. */
    load_scene_matrices(30.0f);
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    bind_cube_arrays();
    glDrawArrays(GL_TRIANGLES, 0, 36);
    size_t repeat_delta = differing_pixels(first_pass, s_host_fb);
    if (repeat_delta != 0) {
        fprintf(stderr,
                "gl1-cube selftest: redrawing the same scene changed %zu pixels\n",
                repeat_delta);
        goto done;
    }

    printf("gl1-cube selftest: ok (lit textured cube, %zu pixels drawn, lighting moved "
           "%zu of "
           "them, torus and sphere rasterised, redraw identical)\n",
           lit_drawn, lighting_delta);
    rc = 0;

done:
    free(first_pass);
    oops_mesh_free(&torus);
    oops_mesh_free(&sphere);
    if (tex_id)
        glDeleteTextures(1, &tex_id);
    if (ctx)
        glContextDestroy(ctx);
    oops_display_close(disp);
    return rc;
}
