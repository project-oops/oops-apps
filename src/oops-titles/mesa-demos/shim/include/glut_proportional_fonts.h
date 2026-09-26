/*
 * The proportional GLUT font names, mapped to the fixed-width face this SDK has.
 *
 * # Why this is in the title and not in the SDK
 *
 * oops-sdk offers `GLUT_BITMAP_8_BY_13` and `GLUT_BITMAP_9_BY_15` and **deliberately
 * withholds** `GLUT_BITMAP_HELVETICA_*` and `GLUT_BITMAP_TIMES_ROMAN_*`. Its reasoning,
 * in `oops-sdk/docs/PORTING.md`: a fixed-width face offered under a proportional name
 * returns the wrong `glutBitmapWidth`, and that breaks the layout of any program that
 * measures a string before drawing it. That is correct and it is the right call for an
 * SDK, where the caller is unknown.
 *
 * Here the caller is known. **Not one of mesa-demos' 56 programs calls
 * `glutBitmapWidth` or `glutBitmapLength`** - checked by grep over the whole set at
 * `mesa-demos-9.0.0`. Every use is an on-screen status line: a frame rate, a key
 * legend, a "not supported" notice. Nothing measures, so nothing can be broken by a
 * width that would be wrong if it were asked for.
 *
 * So the substitution is safe *for this corpus*, and saying that is the whole
 * justification. It lives in the title because a title can only mislead itself; the
 * same mapping in oops-sdk would be a promise to every future port, including the ones
 * that do measure.
 *
 * # What is actually lost
 *
 * Text that upstream expects to be proportional renders fixed-width. Lines will be
 * wider than the demo's author intended and may run off the right edge at small window
 * sizes. That is a cosmetic difference in a status overlay, and it is visible rather
 * than silent - which is the trade this file is making.
 *
 * Nine demos reference these names: `arbocclude`, `arbocclude2`, `engine`,
 * `fbo_firecube`, `fire`, `ipers`, `ray`, `teapot`, `tunnel2` among them. Without this
 * they do not build, and a measurement is lost over a caption.
 */

#ifndef OOPS_MESA_DEMOS_GLUT_PROPORTIONAL_FONTS_H
#define OOPS_MESA_DEMOS_GLUT_PROPORTIONAL_FONTS_H

#include <GL/glut.h>

/*
 * All of them behind `GLUT_BITMAP_9_BY_15`, the larger of the two faces the SDK has.
 * The demos draw these at several nominal sizes - Helvetica 10, 12, 18 and Times Roman
 * 10, 24 - and a console at 1920x1080 reads better with the larger glyph than with an
 * 8x13 standing in for an 18-point face. There is no size information to preserve, so
 * this picks legibility.
 */
#ifndef GLUT_BITMAP_HELVETICA_10
#define GLUT_BITMAP_HELVETICA_10 GLUT_BITMAP_9_BY_15
#endif
#ifndef GLUT_BITMAP_HELVETICA_12
#define GLUT_BITMAP_HELVETICA_12 GLUT_BITMAP_9_BY_15
#endif
#ifndef GLUT_BITMAP_HELVETICA_18
#define GLUT_BITMAP_HELVETICA_18 GLUT_BITMAP_9_BY_15
#endif
#ifndef GLUT_BITMAP_TIMES_ROMAN_10
#define GLUT_BITMAP_TIMES_ROMAN_10 GLUT_BITMAP_9_BY_15
#endif
#ifndef GLUT_BITMAP_TIMES_ROMAN_24
#define GLUT_BITMAP_TIMES_ROMAN_24 GLUT_BITMAP_9_BY_15
#endif

/*
 * The guards matter. If oops-sdk ever grows the real proportional faces - which
 * `REQ-20260922T0915Z-8d3a` leaves open - its definitions win and this file becomes
 * inert instead of overriding something better. Deleting it then is a tidy-up, not a
 * fix.
 */

#endif /* OOPS_MESA_DEMOS_GLUT_PROPORTIONAL_FONTS_H */
