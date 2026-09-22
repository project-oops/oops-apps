/*
 * GLU's quadrics, absent on the Mesa path, and saying so out loud when one is called.
 *
 * # Why this file exists
 *
 * `oops-sdk`'s GLUT is backend-agnostic - `src/gl/glut.c` names no OpenGL implementation and goes
 * entirely through `oops/gfx.h` - which is the whole reason this title can run upstream's demos
 * on upstream Mesa. Its solid shapes (`glutSolidSphere`, `glutSolidCylinder` and friends) are
 * built on GLU quadrics, so linking GLUT leaves those six names undefined.
 *
 * **oops-sdk's GLU cannot supply them here.** `src/gl/gl_glu.c` is not backend-agnostic: it
 * reaches into oops-gl for `gl_sin`, `gl_sqrt`, `gl_pixel_transfer_rgbaf`, `gl_list_alloc` and
 * more, and those live in files that between them define forty OpenGL entry points. Linking them
 * beside Mesa would put a second `glMatrixMode` and `glLoadIdentity` in the binary. Each one
 * pulled in two more when this was tried, which is the shape of a dependency that wants fixing at
 * its own end rather than chasing from here.
 *
 * # Why a reporting no-op rather than a stub that draws something
 *
 * A stub that drew a cube where a sphere was asked for would make a demo *look* like it ran. This
 * title's entire job is measuring what works, so a silent substitution is the one thing it must
 * not do. A demo that calls one of these prints a line naming it and draws nothing, and the
 * absence is then visible in the log rather than inferred from a picture.
 *
 * It says it once per symbol. A GLUT demo calls these from inside its display callback, sixty
 * times a second, and a per-frame line would bury the run.
 *
 * # When this file goes
 *
 * `REQ-20260922T0940Z-5c17` asks oops-sdk to take GLU's maths and pixel helpers from
 * `oops/math.h` and its own utilities rather than from oops-gl's internals. That is their call
 * and their change; when it lands, `gl_glu.c` links beside Mesa and this file and
 * `glu_maths_from_libm.c` both delete.
 *
 * Until then: `gears`, the default demo, calls none of these. The six names are here so the
 * import manifest can be generated at all - it refuses to write a manifest with an undefined
 * symbol in it, which is the correct default and is what surfaced this whole question.
 */

#include <stdio.h> /* snprintf - from the Mesa sysroot, this being a hosted title */

#include "oops/system.h"

#include <GL/gl.h>
#include <GL/glu.h>

static void say_once(const char *name, int *said)
{
    if (*said) {
        return;
    }
    *said = 1;

    char msg[160];
    (void)snprintf(msg, sizeof msg,
                   "%s is not available on the Mesa path (oops-sdk GLU needs oops-gl internals; "
                   "REQ-5c17) - drawing nothing", name);
    oops_klog("MESA-DEMOS", msg);
}

/*
 * One static object, handed out to every caller. It is never dereferenced by anything here, and
 * returning NULL would be worse: upstream demos check the result and some exit on it, which would
 * end a run before the part being measured.
 */
static struct GLUquadric {
    int unused;
} g_quadric;

GLUquadric *gluNewQuadric(void)
{
    static int said;
    say_once("gluNewQuadric", &said);
    return &g_quadric;
}

void gluDeleteQuadric(GLUquadric *q)
{
    (void)q;
}

void gluQuadricDrawStyle(GLUquadric *q, GLenum draw)
{
    (void)q;
    (void)draw;
}

void gluQuadricNormals(GLUquadric *q, GLenum normal)
{
    (void)q;
    (void)normal;
}

void gluSphere(GLUquadric *q, GLdouble radius, GLint slices, GLint stacks)
{
    static int said;
    (void)q;
    (void)radius;
    (void)slices;
    (void)stacks;
    say_once("gluSphere", &said);
}

void gluCylinder(GLUquadric *q, GLdouble base, GLdouble top, GLdouble height, GLint slices,
                 GLint stacks)
{
    static int said;
    (void)q;
    (void)base;
    (void)top;
    (void)height;
    (void)slices;
    (void)stacks;
    say_once("gluCylinder", &said);
}
