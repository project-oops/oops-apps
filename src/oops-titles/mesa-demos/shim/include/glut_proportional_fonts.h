/*
 * The proportional GLUT font names, mapped to the fixed-width face this SDK has.
 *
 * oops-sdk withholds the proportional names because a fixed-width face under them
 * returns the wrong `glutBitmapWidth` (`oops-sdk/docs/PORTING.md`). No mesa-demos
 * program calls `glutBitmapWidth` or `glutBitmapLength`, so the mapping is safe for
 * this corpus and lives in this title, not the SDK. Proportional text renders
 * fixed-width.
 */

#ifndef OOPS_MESA_DEMOS_GLUT_PROPORTIONAL_FONTS_H
#define OOPS_MESA_DEMOS_GLUT_PROPORTIONAL_FONTS_H

#include <GL/glut.h>

/* All map to `GLUT_BITMAP_9_BY_15`, the larger face, for legibility at 1920x1080.
 * Each is guarded, so a real definition from oops-sdk wins. */
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

#endif /* OOPS_MESA_DEMOS_GLUT_PROPORTIONAL_FONTS_H */
