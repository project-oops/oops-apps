/*
 * obj_loader.c - a freestanding Wavefront OBJ loader and procedural mesh generator
 * (see obj_loader.h). No libc dependencies.
 */

#include "obj_loader.h"
#include "oops/freestd.h"
#include "oops/memory.h"

#include "oops/math.h"

#define M_PI_F OOPS_PI
#define M_TWO_PI_F OOPS_TWO_PI
#define M_HALF_PI_F OOPS_HALF_PI

static inline float mesh_sin(float x) {
    return oops_sinf(x);
}

static inline float mesh_cos(float x) {
    return oops_cosf(x);
}

static inline float mesh_sqrt(float v) {
    return oops_sqrtf(v);
}

static inline bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

static inline bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static void skip_spaces(const char **p) {
    while (**p && is_space(**p))
        (*p)++;
}

static void skip_line(const char **p) {
    while (**p && **p != '\n')
        (*p)++;
    if (**p == '\n')
        (*p)++;
}

static float parse_float(const char **p) {
    skip_spaces(p);
    float sign = 1.0f;
    if (**p == '-') {
        sign = -1.0f;
        (*p)++;
    } else if (**p == '+') {
        (*p)++;
    }

    float int_part = 0.0f;
    while (is_digit(**p)) {
        int_part = int_part * 10.0f + (float)(**p - '0');
        (*p)++;
    }

    float frac_part = 0.0f;
    float div = 1.0f;
    if (**p == '.') {
        (*p)++;
        while (is_digit(**p)) {
            frac_part = frac_part * 10.0f + (float)(**p - '0');
            div *= 10.0f;
            (*p)++;
        }
    }

    float res = sign * (int_part + frac_part / div);

    /* Optional scientific exponent (e.g. 1.2e-4) */
    if (**p == 'e' || **p == 'E') {
        (*p)++;
        float exp_sign = 1.0f;
        if (**p == '-') {
            exp_sign = -1.0f;
            (*p)++;
        } else if (**p == '+') {
            (*p)++;
        }
        int exp_val = 0;
        while (is_digit(**p)) {
            exp_val = exp_val * 10 + (**p - '0');
            (*p)++;
        }
        float mult = 1.0f;
        for (int i = 0; i < exp_val; i++) {
            mult *= 10.0f;
        }
        if (exp_sign > 0.0f)
            res *= mult;
        else if (mult > 0.0f)
            res /= mult;
    }

    return res;
}

static int parse_int(const char **p) {
    skip_spaces(p);
    int sign = 1;
    if (**p == '-') {
        sign = -1;
        (*p)++;
    } else if (**p == '+') {
        (*p)++;
    }
    int res = 0;
    while (is_digit(**p)) {
        res = res * 10 + (**p - '0');
        (*p)++;
    }
    return sign * res;
}

/* Parse face index group: "v", "v/vt", "v//vn", or "v/vt/vn" */
static void parse_face_indices(const char **p, int *out_v, int *out_vt, int *out_vn) {
    *out_v = 0;
    *out_vt = 0;
    *out_vn = 0;

    skip_spaces(p);
    if (!is_digit(**p) && **p != '-')
        return;

    *out_v = parse_int(p);

    if (**p == '/') {
        (*p)++;
        if (**p != '/') {
            *out_vt = parse_int(p);
        }
        if (**p == '/') {
            (*p)++;
            *out_vn = parse_int(p);
        }
    }
}

int oops_mesh_load_obj(const char *data, size_t size, oops_mesh_t *out_mesh) {
    if (!data || size == 0 || !out_mesh)
        return -1;
    memset(out_mesh, 0, sizeof(*out_mesh));

    /* Pass 1: Count elements */
    size_t count_v = 0, count_vt = 0, count_vn = 0, count_triangles = 0;
    const char *p = data;
    const char *end = data + size;

    while (p < end) {
        skip_spaces(&p);
        if (*p == '#') {
            skip_line(&p);
            continue;
        }

        if (p[0] == 'v' && is_space(p[1])) {
            count_v++;
            skip_line(&p);
        } else if (p[0] == 'v' && p[1] == 't' && is_space(p[2])) {
            count_vt++;
            skip_line(&p);
        } else if (p[0] == 'v' && p[1] == 'n' && is_space(p[2])) {
            count_vn++;
            skip_line(&p);
        } else if (p[0] == 'f' && is_space(p[1])) {
            p += 2;
            int face_verts = 0;
            while (p < end && *p != '\n' && *p != '\r') {
                skip_spaces(&p);
                if (is_digit(*p) || *p == '-') {
                    face_verts++;
                    while (*p && !is_space(*p) && *p != '\n' && *p != '\r')
                        p++;
                } else {
                    break;
                }
            }
            if (face_verts >= 3) {
                count_triangles += (size_t)(face_verts - 2);
            }
            skip_line(&p);
        } else {
            skip_line(&p);
        }
    }

    if (count_v == 0 || count_triangles == 0)
        return -2;

    /* Temporary arrays for raw indexed data */
    float *raw_v =
        (float *)oops_mem_alloc(count_v * 3 * sizeof(float), 16, OOPS_MEM_WB_ONION);
    float *raw_vt = count_vt ? (float *)oops_mem_alloc(count_vt * 2 * sizeof(float), 16,
                                                       OOPS_MEM_WB_ONION)
                             : NULL;
    float *raw_vn = count_vn ? (float *)oops_mem_alloc(count_vn * 3 * sizeof(float), 16,
                                                       OOPS_MEM_WB_ONION)
                             : NULL;

    if (!raw_v)
        return -3;

    /* Output unrolled vertex arrays (3 vertices per triangle) */
    size_t out_vcount = count_triangles * 3;
    out_mesh->positions =
        (float *)oops_mem_alloc(out_vcount * 3 * sizeof(float), 64, OOPS_MEM_WB_ONION);
    out_mesh->normals =
        (float *)oops_mem_alloc(out_vcount * 3 * sizeof(float), 64, OOPS_MEM_WB_ONION);
    out_mesh->texcoords =
        (float *)oops_mem_alloc(out_vcount * 2 * sizeof(float), 64, OOPS_MEM_WB_ONION);
    out_mesh->colors =
        (float *)oops_mem_alloc(out_vcount * 3 * sizeof(float), 64, OOPS_MEM_WB_ONION);

    if (!out_mesh->positions || !out_mesh->normals || !out_mesh->colors) {
        oops_mesh_free(out_mesh);
        if (raw_v)
            oops_mem_free(raw_v);
        if (raw_vt)
            oops_mem_free(raw_vt);
        if (raw_vn)
            oops_mem_free(raw_vn);
        return -4;
    }

    /* Pass 2: Parse raw vertices */
    p = data;
    size_t cur_v = 0, cur_vt = 0, cur_vn = 0;
    while (p < end) {
        skip_spaces(&p);
        if (p[0] == 'v' && is_space(p[1])) {
            p += 2;
            if (cur_v < count_v) {
                raw_v[cur_v * 3 + 0] = parse_float(&p);
                raw_v[cur_v * 3 + 1] = parse_float(&p);
                raw_v[cur_v * 3 + 2] = parse_float(&p);
                cur_v++;
            }
            skip_line(&p);
        } else if (p[0] == 'v' && p[1] == 't' && is_space(p[2])) {
            p += 3;
            if (raw_vt && cur_vt < count_vt) {
                raw_vt[cur_vt * 2 + 0] = parse_float(&p);
                raw_vt[cur_vt * 2 + 1] = parse_float(&p);
                cur_vt++;
            }
            skip_line(&p);
        } else if (p[0] == 'v' && p[1] == 'n' && is_space(p[2])) {
            p += 3;
            if (raw_vn && cur_vn < count_vn) {
                raw_vn[cur_vn * 3 + 0] = parse_float(&p);
                raw_vn[cur_vn * 3 + 1] = parse_float(&p);
                raw_vn[cur_vn * 3 + 2] = parse_float(&p);
                cur_vn++;
            }
            skip_line(&p);
        } else {
            skip_line(&p);
        }
    }

    /* Pass 3: Parse faces and unroll triangles */
    p = data;
    size_t out_idx = 0;
    int poly_v[32], poly_vt[32], poly_vn[32];

    while (p < end) {
        skip_spaces(&p);
        if (p[0] == 'f' && is_space(p[1])) {
            p += 2;
            int nverts = 0;
            while (p < end && *p != '\n' && *p != '\r' && nverts < 32) {
                skip_spaces(&p);
                if (is_digit(*p) || *p == '-') {
                    parse_face_indices(&p, &poly_v[nverts], &poly_vt[nverts],
                                       &poly_vn[nverts]);
                    nverts++;
                } else {
                    break;
                }
            }
            skip_line(&p);

            /* Fan triangulate convex polygon */
            for (int t = 0; t < nverts - 2; t++) {
                int corner[3] = {0, t + 1, t + 2};
                for (int c = 0; c < 3; c++) {
                    int k = corner[c];
                    int vi = poly_v[k];
                    int vti = poly_vt[k];
                    int vni = poly_vn[k];

                    /* Resolve 1-indexed (or negative relative) indices */
                    if (vi > 0)
                        vi -= 1;
                    else if (vi < 0)
                        vi = (int)count_v + vi;
                    if (vti > 0)
                        vti -= 1;
                    else if (vti < 0 && raw_vt)
                        vti = (int)count_vt + vti;
                    if (vni > 0)
                        vni -= 1;
                    else if (vni < 0 && raw_vn)
                        vni = (int)count_vn + vni;

                    if (vi >= 0 && (size_t)vi < count_v) {
                        out_mesh->positions[out_idx * 3 + 0] = raw_v[vi * 3 + 0];
                        out_mesh->positions[out_idx * 3 + 1] = raw_v[vi * 3 + 1];
                        out_mesh->positions[out_idx * 3 + 2] = raw_v[vi * 3 + 2];
                    }

                    if (raw_vt && vti >= 0 && (size_t)vti < count_vt &&
                        out_mesh->texcoords) {
                        out_mesh->texcoords[out_idx * 2 + 0] = raw_vt[vti * 2 + 0];
                        out_mesh->texcoords[out_idx * 2 + 1] = raw_vt[vti * 2 + 1];
                    }

                    if (raw_vn && vni >= 0 && (size_t)vni < count_vn) {
                        out_mesh->normals[out_idx * 3 + 0] = raw_vn[vni * 3 + 0];
                        out_mesh->normals[out_idx * 3 + 1] = raw_vn[vni * 3 + 1];
                        out_mesh->normals[out_idx * 3 + 2] = raw_vn[vni * 3 + 2];
                    }

                    out_idx++;
                }
            }
        } else {
            skip_line(&p);
        }
    }

    out_mesh->vertex_count = out_idx;
    out_mesh->triangle_count = out_idx / 3;

    oops_mem_free(raw_v);
    if (raw_vt)
        oops_mem_free(raw_vt);
    if (raw_vn)
        oops_mem_free(raw_vn);

    /* An OBJ without normals gets flat per-face normals. */
    if (count_vn == 0) {
        for (size_t i = 0; i < out_mesh->vertex_count; i += 3) {
            float *p0 = &out_mesh->positions[(i + 0) * 3];
            float *p1 = &out_mesh->positions[(i + 1) * 3];
            float *p2 = &out_mesh->positions[(i + 2) * 3];

            float e1x = p1[0] - p0[0], e1y = p1[1] - p0[1], e1z = p1[2] - p0[2];
            float e2x = p2[0] - p0[0], e2y = p2[1] - p0[1], e2z = p2[2] - p0[2];

            float nx = e1y * e2z - e1z * e2y;
            float ny = e1z * e2x - e1x * e2z;
            float nz = e1x * e2y - e1y * e2x;
            float len = mesh_sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 1e-6f) {
                float inv = 1.0f / len;
                nx *= inv;
                ny *= inv;
                nz *= inv;
            } else {
                nx = 0.0f;
                ny = 1.0f;
                nz = 0.0f;
            }

            for (size_t c = 0; c < 3; c++) {
                out_mesh->normals[(i + c) * 3 + 0] = nx;
                out_mesh->normals[(i + c) * 3 + 1] = ny;
                out_mesh->normals[(i + c) * 3 + 2] = nz;
            }
        }
    }

    /* Auto-center and normalize bounds to [-1.0, 1.0] */
    if (out_mesh->vertex_count > 0) {
        float min_x = out_mesh->positions[0], max_x = min_x;
        float min_y = out_mesh->positions[1], max_y = min_y;
        float min_z = out_mesh->positions[2], max_z = min_z;

        for (size_t i = 1; i < out_mesh->vertex_count; i++) {
            float x = out_mesh->positions[i * 3 + 0];
            float y = out_mesh->positions[i * 3 + 1];
            float z = out_mesh->positions[i * 3 + 2];
            if (x < min_x)
                min_x = x;
            if (x > max_x)
                max_x = x;
            if (y < min_y)
                min_y = y;
            if (y > max_y)
                max_y = y;
            if (z < min_z)
                min_z = z;
            if (z > max_z)
                max_z = z;
        }

        float cx = (min_x + max_x) * 0.5f;
        float cy = (min_y + max_y) * 0.5f;
        float cz = (min_z + max_z) * 0.5f;

        float dx = max_x - min_x;
        float dy = max_y - min_y;
        float dz = max_z - min_z;
        float max_extent = dx;
        if (dy > max_extent)
            max_extent = dy;
        if (dz > max_extent)
            max_extent = dz;

        float scale = (max_extent > 1e-6f) ? (2.0f / max_extent) : 1.0f;

        for (size_t i = 0; i < out_mesh->vertex_count; i++) {
            out_mesh->positions[i * 3 + 0] =
                (out_mesh->positions[i * 3 + 0] - cx) * scale;
            out_mesh->positions[i * 3 + 1] =
                (out_mesh->positions[i * 3 + 1] - cy) * scale;
            out_mesh->positions[i * 3 + 2] =
                (out_mesh->positions[i * 3 + 2] - cz) * scale;

            /* Normal-derived Gouraud colours: [-1, 1] maps to [0.15, 0.95]. */
            float nx = out_mesh->normals[i * 3 + 0];
            float ny = out_mesh->normals[i * 3 + 1];
            float nz = out_mesh->normals[i * 3 + 2];
            out_mesh->colors[i * 3 + 0] = 0.55f + 0.40f * nx;
            out_mesh->colors[i * 3 + 1] = 0.55f + 0.40f * ny;
            out_mesh->colors[i * 3 + 2] = 0.55f + 0.40f * nz;
        }
    }

    return 0;
}

int oops_mesh_create_torus(oops_mesh_t *out_mesh, int num_major, int num_minor,
                           float major_radius, float minor_radius) {
    if (!out_mesh || num_major < 3 || num_minor < 3)
        return -1;
    memset(out_mesh, 0, sizeof(*out_mesh));

    size_t num_quads = (size_t)num_major * (size_t)num_minor;
    size_t num_tris = num_quads * 2;
    size_t num_verts = num_tris * 3;

    out_mesh->positions =
        (float *)oops_mem_alloc(num_verts * 3 * sizeof(float), 64, OOPS_MEM_WB_ONION);
    out_mesh->normals =
        (float *)oops_mem_alloc(num_verts * 3 * sizeof(float), 64, OOPS_MEM_WB_ONION);
    out_mesh->texcoords =
        (float *)oops_mem_alloc(num_verts * 2 * sizeof(float), 64, OOPS_MEM_WB_ONION);
    out_mesh->colors =
        (float *)oops_mem_alloc(num_verts * 3 * sizeof(float), 64, OOPS_MEM_WB_ONION);

    if (!out_mesh->positions || !out_mesh->normals || !out_mesh->colors) {
        oops_mesh_free(out_mesh);
        return -2;
    }

    out_mesh->vertex_count = num_verts;
    out_mesh->triangle_count = num_tris;

    size_t v_idx = 0;
    float d_major = M_TWO_PI_F / (float)num_major;
    float d_minor = M_TWO_PI_F / (float)num_minor;

    for (int i = 0; i < num_major; i++) {
        float u0 = (float)i * d_major;
        float u1 = (float)(i + 1) * d_major;

        float cos_u0 = mesh_cos(u0), sin_u0 = mesh_sin(u0);
        float cos_u1 = mesh_cos(u1), sin_u1 = mesh_sin(u1);

        for (int j = 0; j < num_minor; j++) {
            float v0 = (float)j * d_minor;
            float v1 = (float)(j + 1) * d_minor;

            float cos_v0 = mesh_cos(v0), sin_v0 = mesh_sin(v0);
            float cos_v1 = mesh_cos(v1), sin_v1 = mesh_sin(v1);

            /* 4 corner positions and normals of the quad */
            float p00[3] = {(major_radius + minor_radius * cos_v0) * cos_u0,
                            (major_radius + minor_radius * cos_v0) * sin_u0,
                            minor_radius * sin_v0};
            float n00[3] = {cos_v0 * cos_u0, cos_v0 * sin_u0, sin_v0};

            float p10[3] = {(major_radius + minor_radius * cos_v0) * cos_u1,
                            (major_radius + minor_radius * cos_v0) * sin_u1,
                            minor_radius * sin_v0};
            float n10[3] = {cos_v0 * cos_u1, cos_v0 * sin_u1, sin_v0};

            float p11[3] = {(major_radius + minor_radius * cos_v1) * cos_u1,
                            (major_radius + minor_radius * cos_v1) * sin_u1,
                            minor_radius * sin_v1};
            float n11[3] = {cos_v1 * cos_u1, cos_v1 * sin_u1, sin_v1};

            float p01[3] = {(major_radius + minor_radius * cos_v1) * cos_u0,
                            (major_radius + minor_radius * cos_v1) * sin_u0,
                            minor_radius * sin_v1};
            float n01[3] = {cos_v1 * cos_u0, cos_v1 * sin_u0, sin_v1};

            /* Triangle 1: (p00, p10, p11) */
            const float *tri1_p[3] = {p00, p10, p11};
            const float *tri1_n[3] = {n00, n10, n11};
            for (int k = 0; k < 3; k++) {
                out_mesh->positions[v_idx * 3 + 0] = tri1_p[k][0];
                out_mesh->positions[v_idx * 3 + 1] = tri1_p[k][1];
                out_mesh->positions[v_idx * 3 + 2] = tri1_p[k][2];

                out_mesh->normals[v_idx * 3 + 0] = tri1_n[k][0];
                out_mesh->normals[v_idx * 3 + 1] = tri1_n[k][1];
                out_mesh->normals[v_idx * 3 + 2] = tri1_n[k][2];

                out_mesh->colors[v_idx * 3 + 0] = 0.5f + 0.5f * tri1_n[k][0];
                out_mesh->colors[v_idx * 3 + 1] = 0.5f + 0.5f * tri1_n[k][1];
                out_mesh->colors[v_idx * 3 + 2] = 0.5f + 0.5f * tri1_n[k][2];

                if (out_mesh->texcoords) {
                    out_mesh->texcoords[v_idx * 2 + 0] =
                        (k == 0) ? 0.0f : ((k == 1) ? 1.0f : 1.0f);
                    out_mesh->texcoords[v_idx * 2 + 1] =
                        (k == 0) ? 0.0f : ((k == 1) ? 0.0f : 1.0f);
                }
                v_idx++;
            }

            /* Triangle 2: (p00, p11, p01) */
            const float *tri2_p[3] = {p00, p11, p01};
            const float *tri2_n[3] = {n00, n11, n01};
            for (int k = 0; k < 3; k++) {
                out_mesh->positions[v_idx * 3 + 0] = tri2_p[k][0];
                out_mesh->positions[v_idx * 3 + 1] = tri2_p[k][1];
                out_mesh->positions[v_idx * 3 + 2] = tri2_p[k][2];

                out_mesh->normals[v_idx * 3 + 0] = tri2_n[k][0];
                out_mesh->normals[v_idx * 3 + 1] = tri2_n[k][1];
                out_mesh->normals[v_idx * 3 + 2] = tri2_n[k][2];

                out_mesh->colors[v_idx * 3 + 0] = 0.5f + 0.5f * tri2_n[k][0];
                out_mesh->colors[v_idx * 3 + 1] = 0.5f + 0.5f * tri2_n[k][1];
                out_mesh->colors[v_idx * 3 + 2] = 0.5f + 0.5f * tri2_n[k][2];

                if (out_mesh->texcoords) {
                    out_mesh->texcoords[v_idx * 2 + 0] =
                        (k == 0) ? 0.0f : ((k == 1) ? 1.0f : 0.0f);
                    out_mesh->texcoords[v_idx * 2 + 1] =
                        (k == 0) ? 0.0f : ((k == 1) ? 1.0f : 1.0f);
                }
                v_idx++;
            }
        }
    }

    return 0;
}

int oops_mesh_create_sphere(oops_mesh_t *out_mesh, int num_lat, int num_lon,
                            float radius) {
    if (!out_mesh || num_lat < 3 || num_lon < 3)
        return -1;
    memset(out_mesh, 0, sizeof(*out_mesh));

    size_t num_quads = (size_t)num_lat * (size_t)num_lon;
    size_t num_tris = num_quads * 2;
    size_t num_verts = num_tris * 3;

    out_mesh->positions =
        (float *)oops_mem_alloc(num_verts * 3 * sizeof(float), 64, OOPS_MEM_WB_ONION);
    out_mesh->normals =
        (float *)oops_mem_alloc(num_verts * 3 * sizeof(float), 64, OOPS_MEM_WB_ONION);
    out_mesh->texcoords =
        (float *)oops_mem_alloc(num_verts * 2 * sizeof(float), 64, OOPS_MEM_WB_ONION);
    out_mesh->colors =
        (float *)oops_mem_alloc(num_verts * 3 * sizeof(float), 64, OOPS_MEM_WB_ONION);

    if (!out_mesh->positions || !out_mesh->normals || !out_mesh->colors) {
        oops_mesh_free(out_mesh);
        return -2;
    }

    out_mesh->vertex_count = num_verts;
    out_mesh->triangle_count = num_tris;

    size_t v_idx = 0;
    float d_lat = M_PI_F / (float)num_lat;
    float d_lon = M_TWO_PI_F / (float)num_lon;

    for (int i = 0; i < num_lat; i++) {
        float lat0 = -M_HALF_PI_F + (float)i * d_lat;
        float lat1 = -M_HALF_PI_F + (float)(i + 1) * d_lat;

        float sin_lat0 = mesh_sin(lat0), cos_lat0 = mesh_cos(lat0);
        float sin_lat1 = mesh_sin(lat1), cos_lat1 = mesh_cos(lat1);

        for (int j = 0; j < num_lon; j++) {
            float lon0 = (float)j * d_lon;
            float lon1 = (float)(j + 1) * d_lon;

            float sin_lon0 = mesh_sin(lon0), cos_lon0 = mesh_cos(lon0);
            float sin_lon1 = mesh_sin(lon1), cos_lon1 = mesh_cos(lon1);

            float p00[3] = {radius * cos_lat0 * cos_lon0, radius * cos_lat0 * sin_lon0,
                            radius * sin_lat0};
            float n00[3] = {cos_lat0 * cos_lon0, cos_lat0 * sin_lon0, sin_lat0};

            float p10[3] = {radius * cos_lat0 * cos_lon1, radius * cos_lat0 * sin_lon1,
                            radius * sin_lat0};
            float n10[3] = {cos_lat0 * cos_lon1, cos_lat0 * sin_lon1, sin_lat0};

            float p11[3] = {radius * cos_lat1 * cos_lon1, radius * cos_lat1 * sin_lon1,
                            radius * sin_lat1};
            float n11[3] = {cos_lat1 * cos_lon1, cos_lat1 * sin_lon1, sin_lat1};

            float p01[3] = {radius * cos_lat1 * cos_lon0, radius * cos_lat1 * sin_lon0,
                            radius * sin_lat1};
            float n01[3] = {cos_lat1 * cos_lon0, cos_lat1 * sin_lon0, sin_lat1};

            /* Triangle 1 */
            const float *tri1_p[3] = {p00, p10, p11};
            const float *tri1_n[3] = {n00, n10, n11};
            for (int k = 0; k < 3; k++) {
                out_mesh->positions[v_idx * 3 + 0] = tri1_p[k][0];
                out_mesh->positions[v_idx * 3 + 1] = tri1_p[k][1];
                out_mesh->positions[v_idx * 3 + 2] = tri1_p[k][2];

                out_mesh->normals[v_idx * 3 + 0] = tri1_n[k][0];
                out_mesh->normals[v_idx * 3 + 1] = tri1_n[k][1];
                out_mesh->normals[v_idx * 3 + 2] = tri1_n[k][2];

                out_mesh->colors[v_idx * 3 + 0] = 0.5f + 0.5f * tri1_n[k][0];
                out_mesh->colors[v_idx * 3 + 1] = 0.5f + 0.5f * tri1_n[k][1];
                out_mesh->colors[v_idx * 3 + 2] = 0.5f + 0.5f * tri1_n[k][2];
                v_idx++;
            }

            /* Triangle 2 */
            const float *tri2_p[3] = {p00, p11, p01};
            const float *tri2_n[3] = {n00, n11, n01};
            for (int k = 0; k < 3; k++) {
                out_mesh->positions[v_idx * 3 + 0] = tri2_p[k][0];
                out_mesh->positions[v_idx * 3 + 1] = tri2_p[k][1];
                out_mesh->positions[v_idx * 3 + 2] = tri2_p[k][2];

                out_mesh->normals[v_idx * 3 + 0] = tri2_n[k][0];
                out_mesh->normals[v_idx * 3 + 1] = tri2_n[k][1];
                out_mesh->normals[v_idx * 3 + 2] = tri2_n[k][2];

                out_mesh->colors[v_idx * 3 + 0] = 0.5f + 0.5f * tri2_n[k][0];
                out_mesh->colors[v_idx * 3 + 1] = 0.5f + 0.5f * tri2_n[k][1];
                out_mesh->colors[v_idx * 3 + 2] = 0.5f + 0.5f * tri2_n[k][2];
                v_idx++;
            }
        }
    }

    return 0;
}

void oops_mesh_free(oops_mesh_t *mesh) {
    if (!mesh)
        return;
    if (mesh->positions)
        oops_mem_free(mesh->positions);
    if (mesh->normals)
        oops_mem_free(mesh->normals);
    if (mesh->texcoords)
        oops_mem_free(mesh->texcoords);
    if (mesh->colors)
        oops_mem_free(mesh->colors);
    memset(mesh, 0, sizeof(*mesh));
}
