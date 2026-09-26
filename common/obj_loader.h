/*
 * oops-apps: Freestanding 3D Wavefront OBJ Loader & Procedural Mesh Generator
 * Zero libc dependencies. Uses oops/freestd.h and oops/memory.h.
 */

#ifndef OOPS_OBJ_LOADER_H
#define OOPS_OBJ_LOADER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float *positions; /* 3 floats per vertex (x, y, z) */
    float *normals;   /* 3 floats per vertex (nx, ny, nz) */
    float *texcoords; /* 2 floats per vertex (u, v) */
    float *colors;    /* 3 floats per vertex (r, g, b) */
    size_t vertex_count;
    size_t triangle_count;
} oops_mesh_t;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Parses a Wavefront OBJ from memory buffer.
 * Automatically triangulates convex faces, computes normals if missing,
 * generates vibrant per-vertex colors, and scales/centers the mesh into [-1.0, 1.0].
 * Returns 0 on success, negative error code on failure.
 */
int oops_mesh_load_obj(const char *data, size_t size, oops_mesh_t *out_mesh);

/*
 * Generates a smooth 3D Torus mesh with normals, colors, and UV coordinates.
 * major_radius: distance from torus center to tube center (e.g. 0.75f)
 * minor_radius: radius of the tube (e.g. 0.35f)
 */
int oops_mesh_create_torus(oops_mesh_t *out_mesh, int num_major, int num_minor,
                           float major_radius, float minor_radius);

/*
 * Generates a smooth 3D UV Sphere with normals, colors, and UV coordinates.
 */
int oops_mesh_create_sphere(oops_mesh_t *out_mesh, int num_lat, int num_lon,
                            float radius);

/*
 * Frees allocated vertex buffers within oops_mesh_t.
 */
void oops_mesh_free(oops_mesh_t *mesh);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_OBJ_LOADER_H */
