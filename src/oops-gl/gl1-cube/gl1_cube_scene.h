/*
 * gl1-cube: the scene the payload draws.
 *
 * **One copy, because two copies drifted.** The vertex, colour, texture-coordinate and normal
 * arrays and the procedural texture used to be written out twice - once in `gl1_cube_main.c` and
 * once in `gl1_cube_selftest.c` - and on 2026-09-17 a comparison of the two found the top face's
 * sixth vertex coloured `1.00, 1.00, 0.15` in the payload and `0.15, 1.00, 0.15` in the test. One
 * value in 288, and it meant the host test had been rasterising a cube the console never drew.
 *
 * The payload's values are the ones kept: they are what the pinned hardware oracle measured.
 */

#ifndef GL1_CUBE_SCENE_H
#define GL1_CUBE_SCENE_H

#include "GL/gl.h"
#include <stdint.h>
#include <stdbool.h>

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

/* The procedural checkerboard, written into a caller-owned TEX_DIM * TEX_DIM buffer. */
static void generate_cube_texture(uint32_t *out) {
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
            out[y * TEX_DIM + x] = ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
        }
    }
}

#endif /* GL1_CUBE_SCENE_H */
