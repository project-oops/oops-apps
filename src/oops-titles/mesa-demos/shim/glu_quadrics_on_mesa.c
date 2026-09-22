/*
 * GLU's quadrics, drawn rather than refused.
 *
 * # Why this replaced a set of reporting no-ops
 *
 * These began as no-ops that logged their own absence, on the reasoning that a shape drawn wrong
 * is worse than a shape not drawn. That was right as far as it went and wrong about the cost:
 * `cubemap` ran on hardware on 2026-09-22 and drew nothing, because the object it reflection-maps
 * is a `glutSolidSphere` - and oops-sdk builds `glutSolidSphere` on GLU quadrics
 * (`src/gl/glut.c:461`). The demo could not test cube mapping because the thing to map onto did
 * not exist.
 *
 * Twelve more demos call GLU quadrics directly, and every `glutSolid*`/`glutWire*` shape goes the
 * same way. So the no-ops were not costing one demo, they were costing most of the set's
 * geometry.
 *
 * **And unlike the pixel paths, this is not a judgement call.** A quadric is a parametric surface
 * with a closed form. `gluSphere` is a latitude/longitude tessellation and the specification says
 * exactly which; there is no filter to choose and nothing to approximate. Writing it is
 * transcription, not interpretation, which is what makes it safe here where a mipmap filter would
 * not have been.
 *
 * It still deletes when `REQ-20260922T0940Z-5c17` lands and oops-sdk's own GLU can be linked.
 *
 * # Normals are the point, not a detail
 *
 * `cubemap` uses `GL_REFLECTION_MAP_ARB` texgen, which derives its texture coordinate from the
 * **normal** and the eye vector. A sphere drawn with correct positions and absent normals would
 * be geometrically right and reflect nothing - which would have looked like a cube-map failure
 * and been a quadric one. For a unit sphere the normal is the unit position, which is the one
 * piece of luck in this file.
 */

#include <stdio.h> /* snprintf, for the one thing still unimplemented */

#include "oops/system.h"

#include <GL/gl.h>
#include <GL/glu.h>

#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 * The quadric object. Real GLU carries a callback and an error code too; nothing in this demo set
 * sets either, so they are absent rather than stored and ignored.
 */
struct GLUquadric {
    GLenum draw_style;  /* GLU_FILL, GLU_LINE, GLU_POINT, GLU_SILHOUETTE */
    GLenum normals;     /* GLU_SMOOTH, GLU_FLAT, GLU_NONE */
    GLenum orientation; /* GLU_OUTSIDE, GLU_INSIDE */
    GLboolean texture;
};

GLUquadric *gluNewQuadric(void)
{
    GLUquadric *q = (GLUquadric *)malloc(sizeof(*q));
    if (q == NULL) {
        return NULL;
    }
    /* The specification's defaults. */
    q->draw_style = GLU_FILL;
    q->normals = GLU_SMOOTH;
    q->orientation = GLU_OUTSIDE;
    q->texture = GL_FALSE;
    return q;
}

void gluDeleteQuadric(GLUquadric *q)
{
    free(q);
}

void gluQuadricDrawStyle(GLUquadric *q, GLenum draw)
{
    if (q != NULL) {
        q->draw_style = draw;
    }
}

void gluQuadricNormals(GLUquadric *q, GLenum normal)
{
    if (q != NULL) {
        q->normals = normal;
    }
}

void gluQuadricOrientation(GLUquadric *q, GLenum orientation)
{
    if (q != NULL) {
        q->orientation = orientation;
    }
}

void gluQuadricTexture(GLUquadric *q, GLboolean texture)
{
    if (q != NULL) {
        q->texture = texture;
    }
}

/* `GLU_FILL` draws surfaces; `GLU_LINE` and `GLU_SILHOUETTE` draw wireframe; `GLU_POINT` points.
 * Mapping them onto a polygon mode keeps one set of vertex loops for all four. */
static GLenum quadric_begin_mode(const GLUquadric *q)
{
    switch (q->draw_style) {
        case GLU_POINT: return GL_POINTS;
        case GLU_LINE:
        case GLU_SILHOUETTE: return GL_LINE_STRIP;
        default: return GL_QUAD_STRIP;
    }
}

/* Inside-facing quadrics flip the normal; the winding is left alone, which is what GLU does. */
static void quadric_normal(const GLUquadric *q, double x, double y, double z)
{
    if (q->normals == GLU_NONE) {
        return;
    }
    const double s = (q->orientation == GLU_INSIDE) ? -1.0 : 1.0;
    glNormal3d(x * s, y * s, z * s);
}

/*
 * A latitude/longitude sphere, as the specification defines it: `stacks` bands from +z to -z,
 * `slices` divisions around. The normal at a point on a unit sphere is the point itself, so
 * position and normal come from one pair of trig calls.
 */
void gluSphere(GLUquadric *q, GLdouble radius, GLint slices, GLint stacks)
{
    if (q == NULL || slices < 2 || stacks < 2) {
        return;
    }

    const GLenum mode = quadric_begin_mode(q);

    for (GLint i = 0; i < stacks; i++) {
        /* Polar angle from +z, so rho0 is the top of this band and rho1 the bottom. */
        const double rho0 = M_PI * (double)i / (double)stacks;
        const double rho1 = M_PI * (double)(i + 1) / (double)stacks;
        const double z0 = cos(rho0), r0 = sin(rho0);
        const double z1 = cos(rho1), r1 = sin(rho1);

        glBegin(mode);
        for (GLint j = 0; j <= slices; j++) {
            const double theta = 2.0 * M_PI * (double)j / (double)slices;
            const double ct = cos(theta), st = sin(theta);

            if (q->texture) {
                glTexCoord2d((double)j / (double)slices, 1.0 - (double)i / (double)stacks);
            }
            quadric_normal(q, r0 * ct, r0 * st, z0);
            glVertex3d(radius * r0 * ct, radius * r0 * st, radius * z0);

            if (q->texture) {
                glTexCoord2d((double)j / (double)slices, 1.0 - (double)(i + 1) / (double)stacks);
            }
            quadric_normal(q, r1 * ct, r1 * st, z1);
            glVertex3d(radius * r1 * ct, radius * r1 * st, radius * z1);
        }
        glEnd();
    }
}

/*
 * A cone or tube along +z from z=0 to z=height, `base` radius at the bottom and `top` at the top.
 *
 * The normal is **not** radial unless the sides are parallel: for a cone it tilts by the slope,
 * and getting that wrong gives a lit cone that shades like a cylinder. `nz` below is that slope
 * term, and it is why this is not simply `(ct, st, 0)`.
 */
void gluCylinder(GLUquadric *q, GLdouble base, GLdouble top, GLdouble height, GLint slices,
                 GLint stacks)
{
    if (q == NULL || slices < 2 || stacks < 1 || height == 0.0) {
        return;
    }

    const GLenum mode = quadric_begin_mode(q);
    const double slope = (base - top) / height;
    /* Normalise (1, slope) so the radial and z parts of the normal are a unit vector together. */
    const double norm = sqrt(1.0 + slope * slope);
    const double nr = 1.0 / norm;
    const double nz = slope / norm;

    for (GLint i = 0; i < stacks; i++) {
        const double t0 = (double)i / (double)stacks;
        const double t1 = (double)(i + 1) / (double)stacks;
        const double z0 = height * t0, z1 = height * t1;
        const double r0 = base + (top - base) * t0;
        const double r1 = base + (top - base) * t1;

        glBegin(mode);
        for (GLint j = 0; j <= slices; j++) {
            const double theta = 2.0 * M_PI * (double)j / (double)slices;
            const double ct = cos(theta), st = sin(theta);

            if (q->texture) {
                glTexCoord2d((double)j / (double)slices, t0);
            }
            quadric_normal(q, nr * ct, nr * st, nz);
            glVertex3d(r0 * ct, r0 * st, z0);

            if (q->texture) {
                glTexCoord2d((double)j / (double)slices, t1);
            }
            quadric_normal(q, nr * ct, nr * st, nz);
            glVertex3d(r1 * ct, r1 * st, z1);
        }
        glEnd();
    }
}

/* A flat annulus in z=0, from `inner` to `outer`. The normal is +z, or -z facing inward. */
void gluDisk(GLUquadric *q, GLdouble inner, GLdouble outer, GLint slices, GLint loops)
{
    if (q == NULL || slices < 2 || loops < 1) {
        return;
    }

    const GLenum mode = quadric_begin_mode(q);

    for (GLint i = 0; i < loops; i++) {
        const double r0 = inner + (outer - inner) * (double)i / (double)loops;
        const double r1 = inner + (outer - inner) * (double)(i + 1) / (double)loops;

        glBegin(mode);
        for (GLint j = 0; j <= slices; j++) {
            const double theta = 2.0 * M_PI * (double)j / (double)slices;
            const double ct = cos(theta), st = sin(theta);

            quadric_normal(q, 0.0, 0.0, 1.0);

            /* GLU's disk texture coordinates map the bounding square to [0,1], not the radius. */
            if (q->texture) {
                glTexCoord2d(0.5 + 0.5 * r0 * ct / outer, 0.5 + 0.5 * r0 * st / outer);
            }
            glVertex3d(r0 * ct, r0 * st, 0.0);

            if (q->texture) {
                glTexCoord2d(0.5 + 0.5 * r1 * ct / outer, 0.5 + 0.5 * r1 * st / outer);
            }
            glVertex3d(r1 * ct, r1 * st, 0.0);
        }
        glEnd();
    }
}

/*
 * Still absent, and still said out loud.
 *
 * `gluPartialDisk` is the one quadric nothing in this demo set calls, so it has no test and is
 * not written on spec alone - the rest of this file is checked by demos that draw. It reports
 * rather than drawing a full disk, because a partial disk silently drawn whole is exactly the
 * quiet wrongness this title exists to avoid.
 */
void gluPartialDisk(GLUquadric *q, GLdouble inner, GLdouble outer, GLint slices, GLint loops,
                    GLdouble start, GLdouble sweep)
{
    static int said;

    (void)q; (void)inner; (void)outer; (void)slices; (void)loops; (void)start; (void)sweep;

    if (!said) {
        said = 1;
        oops_klog("MESA-DEMOS",
                  "gluPartialDisk is not implemented in this title's shim - drawing nothing");
    }
}
