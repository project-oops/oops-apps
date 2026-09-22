/*
 * The GLU functions that are pure maths, implemented here because oops-sdk's cannot be linked.
 *
 * # Why these are here and `glu_absent_on_mesa.c` is next to them
 *
 * oops-sdk has a perfectly good GLU. A `USE_MESA` title cannot use it: `src/gl/gl_glu.c` reaches
 * into oops-gl for `gl_sin`, `gl_pixel_transfer_rgbaf`, `gl_list_alloc` and more, and those live
 * in files that between them define forty `gl*` entry points - linking them beside Mesa would put
 * a second `glMatrixMode` in the binary. Filed as `REQ-20260922T0940Z-5c17`.
 *
 * That leaves each GLU function in one of two groups, and the split is worth stating because it
 * is not obvious from the names:
 *
 * - **Pure maths and strings**, which need nothing from oops-gl and are reimplemented here from
 *   their specifications: `gluPerspective`, `gluLookAt`, `gluOrtho2D`, `gluErrorString`. They are
 *   short, they are fully defined by the GL specification, and writing them is less work than
 *   explaining their absence. `gluBuild2DMipmaps` is here too - it touches pixels, but it is a
 *   box filter over an array and needs nothing from a GL implementation but `glTexImage2D`.
 * - **Anything needing oops-gl's own state or geometry** - the quadrics, the tessellator - which
 *   stay in `glu_absent_on_mesa.c` as reporting no-ops, because a shape drawn wrong is worse
 *   than a shape not drawn.
 *
 * All of it deletes when `-5c17` lands.
 *
 * # These are specifications, not guesses
 *
 * `gluPerspective` and `gluLookAt` are defined in the OpenGL specification as exact matrices, and
 * both are written here as the specification gives them and then multiplied into the current
 * matrix with `glMultMatrixd` - which is what a real GLU does. Nothing here is fitted to what a
 * demo happened to expect.
 */

#include <GL/gl.h>
#include <GL/glu.h>

#include <math.h>
#include <stdlib.h> /* malloc/free, for the mipmap chain at the bottom */

/*
 * The specification's perspective matrix. `f = cot(fovy/2)`, and the result is column-major for
 * `glMultMatrixd`, which reads 16 doubles in column order.
 */
void gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
    const GLdouble radians = fovy * 3.14159265358979323846 / 360.0; /* fovy/2, in radians */
    const GLdouble sine = sin(radians);

    /* Degenerate arguments produce a singular matrix. The specification does not define the
     * result; doing nothing leaves the caller's matrix untouched, which is recoverable, where
     * dividing by zero is not. */
    if (sine == 0.0 || aspect == 0.0 || zFar == zNear) {
        return;
    }

    const GLdouble cotangent = cos(radians) / sine;
    const GLdouble depth = zFar - zNear;

    const GLdouble m[16] = {
        cotangent / aspect, 0.0,       0.0,                              0.0,
        0.0,                cotangent, 0.0,                              0.0,
        0.0,                0.0,       -(zFar + zNear) / depth,         -1.0,
        0.0,                0.0,       -2.0 * zNear * zFar / depth,      0.0,
    };

    glMultMatrixd(m);
}

static void glu_normalise(GLdouble v[3])
{
    const GLdouble r = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (r == 0.0) {
        return;
    }
    v[0] /= r;
    v[1] /= r;
    v[2] /= r;
}

static void glu_cross(const GLdouble a[3], const GLdouble b[3], GLdouble out[3])
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

/*
 * The specification's viewing transform: forward = centre - eye, side = forward x up,
 * up' = side x forward, then a rotation built from those three and a translation by -eye.
 */
void gluLookAt(GLdouble eyeX, GLdouble eyeY, GLdouble eyeZ,
               GLdouble centerX, GLdouble centerY, GLdouble centerZ,
               GLdouble upX, GLdouble upY, GLdouble upZ)
{
    GLdouble forward[3] = { centerX - eyeX, centerY - eyeY, centerZ - eyeZ };
    GLdouble up[3] = { upX, upY, upZ };
    GLdouble side[3];

    glu_normalise(forward);
    glu_cross(forward, up, side);
    glu_normalise(side);

    /* Recompute up from the orthogonalised axes, so a caller's up vector need not be
     * perpendicular to the view direction - which is the whole convenience of this function. */
    glu_cross(side, forward, up);

    const GLdouble m[16] = {
        side[0],     up[0],     -forward[0], 0.0,
        side[1],     up[1],     -forward[1], 0.0,
        side[2],     up[2],     -forward[2], 0.0,
        0.0,         0.0,        0.0,        1.0,
    };

    glMultMatrixd(m);
    glTranslated(-eyeX, -eyeY, -eyeZ);
}

/* An orthographic projection with the near and far planes at -1 and 1, which is all this is. */
void gluOrtho2D(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top)
{
    glOrtho(left, right, bottom, top, -1.0, 1.0);
}

/*
 * The error names. GLU's own strings are longer sentences; these are the enum names, which is
 * what a log line wants and what every demo here does with the result. `GL_NO_ERROR` is included
 * because a demo that prints unconditionally should not print "unknown".
 */
const GLubyte *gluErrorString(GLenum error)
{
    switch (error) {
        case GL_NO_ERROR:                      return (const GLubyte *)"GL_NO_ERROR";
        case GL_INVALID_ENUM:                  return (const GLubyte *)"GL_INVALID_ENUM";
        case GL_INVALID_VALUE:                 return (const GLubyte *)"GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:             return (const GLubyte *)"GL_INVALID_OPERATION";
        case GL_STACK_OVERFLOW:                return (const GLubyte *)"GL_STACK_OVERFLOW";
        case GL_STACK_UNDERFLOW:               return (const GLubyte *)"GL_STACK_UNDERFLOW";
        case GL_OUT_OF_MEMORY:                 return (const GLubyte *)"GL_OUT_OF_MEMORY";
        default:                               break;
    }
    return (const GLubyte *)"unrecognised GL error";
}

/*
 * `gluBuild2DMipmaps`, as a real box filter.
 *
 * # Why not just call `glGenerateMipmap`
 *
 * Mesa has it and it would be four lines. It would also put the GPU's filter in the picture
 * instead of GLU's, and this title exists to measure pictures - so when a mipmapped demo looks
 * wrong, the answer has to be "the driver is wrong", not "the shim filtered differently". A box
 * filter is what GLU specifies and it is forty lines, so it is what this does.
 *
 * # What it handles, and what it refuses
 *
 * `GL_UNSIGNED_BYTE` with 1 to 4 components, which is every texture in this demo set (all of them
 * arrive from `readtex.c` as 8-bit RGB or RGBA). Anything else returns `GLU_INVALID_ENUM` without
 * uploading, because a half-supported format that silently uploads level 0 and no chain is the
 * kind of quiet wrongness this whole title is written against.
 *
 * Non-power-of-two input is **not** rescaled first, as real GLU would. Every image here is a
 * power of two; a non-power-of-two one halves by integer division and the result is a valid chain
 * of slightly different dimensions. That is a deliberate simplification and it is recorded here
 * rather than discovered later.
 */
GLint gluBuild2DMipmaps(GLenum target, GLint internalFormat, GLsizei width, GLsizei height,
                        GLenum format, GLenum type, const void *data)
{
    if (type != GL_UNSIGNED_BYTE || width <= 0 || height <= 0 || data == NULL) {
        return GLU_INVALID_ENUM;
    }

    int components;
    switch (format) {
        case GL_RGBA:            components = 4; break;
        case GL_RGB:             components = 3; break;
        case GL_LUMINANCE_ALPHA: components = 2; break;
        case GL_LUMINANCE:
        case GL_ALPHA:           components = 1; break;
        default:                 return GLU_INVALID_ENUM;
    }

    /* Level 0 is the caller's image, untouched. */
    glTexImage2D(target, 0, internalFormat, width, height, 0, format, type, data);

    /* Two buffers, ping-ponged: the source of each halving and its destination. The first source
     * is the caller's data, which is never written to. */
    const unsigned char *src = (const unsigned char *)data;
    unsigned char *owned_src = NULL;
    GLsizei w = width;
    GLsizei h = height;
    GLint level = 0;

    while (w > 1 || h > 1) {
        const GLsizei nw = (w > 1) ? w / 2 : 1;
        const GLsizei nh = (h > 1) ? h / 2 : 1;

        unsigned char *dst = (unsigned char *)malloc((size_t)nw * (size_t)nh * (size_t)components);
        if (dst == NULL) {
            free(owned_src);
            return GLU_OUT_OF_MEMORY;
        }

        /* The 2x2 box. When a dimension is already 1 the two samples along it coincide, which is
         * what makes a 1xN or Nx1 chain come out right without a special case. */
        for (GLsizei y = 0; y < nh; y++) {
            const GLsizei y0 = (h > 1) ? (y * 2) : 0;
            const GLsizei y1 = (h > 1) ? (y0 + 1) : 0;

            for (GLsizei x = 0; x < nw; x++) {
                const GLsizei x0 = (w > 1) ? (x * 2) : 0;
                const GLsizei x1 = (w > 1) ? (x0 + 1) : 0;

                for (int c = 0; c < components; c++) {
                    const unsigned a = src[((size_t)y0 * (size_t)w + (size_t)x0) * (size_t)components + (size_t)c];
                    const unsigned b = src[((size_t)y0 * (size_t)w + (size_t)x1) * (size_t)components + (size_t)c];
                    const unsigned d = src[((size_t)y1 * (size_t)w + (size_t)x0) * (size_t)components + (size_t)c];
                    const unsigned e = src[((size_t)y1 * (size_t)w + (size_t)x1) * (size_t)components + (size_t)c];

                    /* +2 rounds to nearest rather than truncating, which over ten levels is the
                     * difference between a chain that stays neutral and one that darkens. */
                    dst[((size_t)y * (size_t)nw + (size_t)x) * (size_t)components + (size_t)c] =
                        (unsigned char)((a + b + d + e + 2u) / 4u);
                }
            }
        }

        level++;
        glTexImage2D(target, level, internalFormat, nw, nh, 0, format, type, dst);

        free(owned_src);
        owned_src = dst;
        src = dst;
        w = nw;
        h = nh;
    }

    free(owned_src);
    return 0;
}

/*
 * `gluGetString`. The version this shim implements is 1.3's subset, and it says so rather than
 * claiming a full 1.3 - a demo printing it is the only consumer here, and an honest string in a
 * log is worth more than a matching one.
 */
const GLubyte *gluGetString(GLenum name)
{
    switch (name) {
        case GLU_VERSION:    return (const GLubyte *)"1.3 (oops-mesa title shim: matrices, "
                                                     "mipmaps, error strings)";
        case GLU_EXTENSIONS: return (const GLubyte *)"";
        default:             break;
    }
    return NULL;
}

/*
 * `gluScaleImage`, as a box filter over the source rectangle covering each destination texel.
 *
 * Real GLU reads through the `GL_UNPACK_*` pixel-store state; this reads the buffer tightly
 * packed, which is what every caller in this demo set hands it. A caller that had set a row
 * length would get a wrong picture silently, so the restriction is stated here and the only
 * formats accepted are the ones that cannot carry that surprise.
 */
GLint gluScaleImage(GLenum format, GLsizei wIn, GLsizei hIn, GLenum typeIn, const void *dataIn,
                    GLsizei wOut, GLsizei hOut, GLenum typeOut, void *dataOut)
{
    if (typeIn != GL_UNSIGNED_BYTE || typeOut != GL_UNSIGNED_BYTE ||
        wIn <= 0 || hIn <= 0 || wOut <= 0 || hOut <= 0 ||
        dataIn == NULL || dataOut == NULL) {
        return GLU_INVALID_ENUM;
    }

    int components;
    switch (format) {
        case GL_RGBA:            components = 4; break;
        case GL_RGB:             components = 3; break;
        case GL_LUMINANCE_ALPHA: components = 2; break;
        case GL_LUMINANCE:
        case GL_ALPHA:           components = 1; break;
        default:                 return GLU_INVALID_ENUM;
    }

    const unsigned char *src = (const unsigned char *)dataIn;
    unsigned char *dst = (unsigned char *)dataOut;

    for (GLsizei y = 0; y < hOut; y++) {
        /* The source rows this destination row covers. Computed as a half-open range so that
         * scaling up (where the range is one row) and scaling down (where it is many) are the
         * same loop rather than two cases. */
        GLsizei sy0 = (GLsizei)(((long long)y * hIn) / hOut);
        GLsizei sy1 = (GLsizei)(((long long)(y + 1) * hIn) / hOut);
        if (sy1 <= sy0) sy1 = sy0 + 1;
        if (sy1 > hIn) sy1 = hIn;

        for (GLsizei x = 0; x < wOut; x++) {
            GLsizei sx0 = (GLsizei)(((long long)x * wIn) / wOut);
            GLsizei sx1 = (GLsizei)(((long long)(x + 1) * wIn) / wOut);
            if (sx1 <= sx0) sx1 = sx0 + 1;
            if (sx1 > wIn) sx1 = wIn;

            for (int c = 0; c < components; c++) {
                unsigned long total = 0;
                unsigned long n = 0;

                for (GLsizei sy = sy0; sy < sy1; sy++) {
                    for (GLsizei sx = sx0; sx < sx1; sx++) {
                        total += src[((size_t)sy * (size_t)wIn + (size_t)sx) * (size_t)components
                                     + (size_t)c];
                        n++;
                    }
                }

                dst[((size_t)y * (size_t)wOut + (size_t)x) * (size_t)components + (size_t)c] =
                    (unsigned char)(n ? ((total + n / 2) / n) : 0);
            }
        }
    }

    return 0;
}
