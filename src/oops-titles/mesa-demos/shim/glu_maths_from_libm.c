/*
 * Two maths functions, and a note about why a title is defining them.
 *
 * `oops-sdk`'s GLUT (`src/gl/glut.c`) is backend-agnostic - it names no OpenGL implementation and
 * goes through `oops/gfx.h` - which is what lets this title run upstream demos on Mesa. Its solid
 * shapes (`glutSolidSphere` and friends) call GLU, and oops-sdk's GLU (`src/gl/gl_glu.c`) is
 * *not* backend-agnostic: it reaches into oops-gl for `gl_sin` and `gl_sqrt`, which live in
 * `src/gl/gl_matrix.c`.
 *
 * **And `gl_matrix.c` defines forty OpenGL entry points.** Linking it to obtain two maths helpers
 * would put a second `glMatrixMode`, `glLoadIdentity` and `glTranslatef` in a binary that already
 * has Mesa's, which is not a trade worth making for `sinf` and `sqrtf`.
 *
 * So they are defined here, from the C library the hosted title already links, and they are the
 * whole of the workaround. Both are declared in `oops-sdk/src/gl/gl_internal.h` with exactly
 * these signatures; if that changes, this file stops compiling rather than drifting silently,
 * which is the failure mode to want.
 *
 * **This is filed, not accepted.** `REQ-20260922T0940Z-5c17` asks oops-sdk to take GLU's maths
 * from `oops/math.h` or libm instead of from oops-gl's matrix stack, which is a two-line change
 * on their side and deletes this file. Until then, a title wanting GLUT's solid shapes on Mesa
 * needs these two lines, and it is better to have them here with the reason attached than to
 * discover the collision from a duplicate-symbol error.
 */

#include <math.h>

float gl_sin(float rad);
float gl_cos(float rad);
float gl_sqrt(float val);

float gl_sin(float rad)
{
    return sinf(rad);
}

float gl_cos(float rad)
{
    return cosf(rad);
}

float gl_sqrt(float val)
{
    return sqrtf(val);
}
