/*
 * Host self-test for gl-cube.
 *
 * Validates oops-gl 3D matrix transforms, procedural texture synthesis,
 * texture upload via glTexImage2D, and vertex array cube rasterization.
 */

#include "GL/gl.h"
#include "GL/glu.h"
#include "oops/display.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define TEX_DIM 64
static uint32_t s_test_texture[TEX_DIM * TEX_DIM];

static uint32_t s_host_fb[640 * 480];
static int s_host_disp_dummy = 1;
static unsigned int s_host_w = 640;
static unsigned int s_host_h = 480;

oops_display_t *
oops_display_open(oops_display_backend_t backend, unsigned int width, unsigned int height) {
    (void)backend;
    s_host_w = width ? width : 640;
    s_host_h = height ? height : 480;
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

static void generate_test_texture(void) {
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
            s_test_texture[y * TEX_DIM + x] = ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
        }
    }
}

/* 36 vertices defining 12 triangles of a 3D unit cube centered at origin */
static const GLfloat s_cube_vertices[36 * 3] = {
    /* Front Face (Z = +1.0) */
    -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,
    /* Back Face (Z = -1.0) */
     1.0f, -1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,
    /* Top Face (Y = +1.0) */
    -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
    /* Bottom Face (Y = -1.0) */
    -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
    /* Right Face (X = +1.0) */
     1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
    /* Left Face (X = -1.0) */
    -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f
};

static const GLfloat s_cube_colors[36 * 3] = {
    /* Front Face */
    0.15f, 0.15f, 1.00f,  1.00f, 0.15f, 1.00f,  1.00f, 1.00f, 1.00f,
    0.15f, 0.15f, 1.00f,  1.00f, 1.00f, 1.00f,  0.15f, 1.00f, 1.00f,
    /* Back Face */
    0.15f, 0.15f, 0.15f,  0.15f, 1.00f, 0.15f,  1.00f, 1.00f, 0.15f,
    0.15f, 0.15f, 0.15f,  1.00f, 1.00f, 0.15f,  1.00f, 0.15f, 0.15f,
    /* Top Face */
    0.15f, 1.00f, 0.15f,  0.15f, 1.00f, 1.00f,  1.00f, 1.00f, 1.00f,
    0.15f, 1.00f, 0.15f,  1.00f, 1.00f, 1.00f,  0.15f, 1.00f, 0.15f,
    /* Bottom Face */
    0.15f, 0.15f, 0.15f,  1.00f, 0.15f, 0.15f,  1.00f, 0.15f, 1.00f,
    0.15f, 0.15f, 0.15f,  1.00f, 0.15f, 1.00f,  0.15f, 0.15f, 1.00f,
    /* Right Face */
    1.00f, 0.15f, 1.00f,  1.00f, 0.15f, 0.15f,  1.00f, 1.00f, 0.15f,
    1.00f, 0.15f, 1.00f,  1.00f, 1.00f, 0.15f,  1.00f, 1.00f, 1.00f,
    /* Left Face */
    0.15f, 0.15f, 0.15f,  0.15f, 0.15f, 1.00f,  0.15f, 1.00f, 1.00f,
    0.15f, 0.15f, 0.15f,  0.15f, 1.00f, 1.00f,  0.15f, 1.00f, 0.15f
};

static const GLfloat s_cube_texcoords[36 * 2] = {
    0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,  0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,
    0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,  0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,
    0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,  0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,
    0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,  0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,
    0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,  0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,
    0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,  0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f
};

int main(void) {
    /* 1. Verify procedural texture synthesis */
    generate_test_texture();
    if (s_test_texture[0] == 0) {
        fprintf(stderr, "gl-cube selftest: failed texture generation\n");
        return 1;
    }

    /* 2. Open display and initialize OpenGL context */
    oops_display_t *disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, 640, 480);
    if (!disp) {
        fprintf(stderr, "gl-cube selftest: failed to open display\n");
        return 1;
    }

    void *ctx = glContextCreate(disp);
    if (!ctx) {
        fprintf(stderr, "gl-cube selftest: failed to create gl context\n");
        oops_display_close(disp);
        return 1;
    }
    glContextMakeCurrent(ctx);

    /* 3. Setup viewport, depth, culling, and state */
    glViewport(0, 0, 640, 480);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glShadeModel(GL_SMOOTH);

    /* 4. Texture upload */
    GLuint tex_id = 0;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_DIM, TEX_DIM, 0, GL_RGBA, GL_UNSIGNED_BYTE, s_test_texture);
    glEnable(GL_TEXTURE_2D);

    /* 5. Matrix transformations */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, 640.0f / 480.0f, 0.1f, 100.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -4.5f);
    glRotatef(30.0f, 1.0f, 1.0f, 0.0f);

    /* 6. Clear and render 3D cube via client vertex arrays */
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, s_cube_vertices);

    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(3, GL_FLOAT, 0, s_cube_colors);

    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, s_cube_texcoords);

    glDrawArrays(GL_TRIANGLES, 0, 36);

    /* 7. Verify framebuffer has rendered output */
    uint32_t *fb = oops_display_get_framebuffer(disp);
    if (!fb) {
        fprintf(stderr, "gl-cube selftest: no framebuffer\n");
        glContextDestroy(ctx);
        oops_display_close(disp);
        return 1;
    }

    /* Check clear color background at corner (x=10, y=10) (RGBA8 unpacked from col) */
    uint32_t corner_pixel = fb[10 * 640 + 10];
    uint32_t corner_b = corner_pixel & 0xffu;
    uint32_t corner_g = (corner_pixel >> 8) & 0xffu;
    uint32_t corner_r = (corner_pixel >> 16) & 0xffu;
    if (corner_b < 40 || corner_r > 35 || corner_g > 35) {
        fprintf(stderr, "gl-cube selftest: corner clear color unexpected (0x%08x)\n", corner_pixel);
        glContextDestroy(ctx);
        oops_display_close(disp);
        return 1;
    }

    /* Check center pixel (x=320, y=240): cube should have drawn over background */
    uint32_t center_pixel = fb[240 * 640 + 320];
    if (center_pixel == corner_pixel || center_pixel == 0) {
        fprintf(stderr, "gl-cube selftest: center pixel not rendered (got 0x%08x)\n", center_pixel);
        glContextDestroy(ctx);
        oops_display_close(disp);
        return 1;
    }

    /* 8. Cleanup */
    glDeleteTextures(1, &tex_id);
    glContextDestroy(ctx);
    oops_display_close(disp);

    printf("gl-cube selftest: ok (3D cube transformed, textured, and rasterized)\n");
    return 0;
}
