/*
 * glad, as much of it as mesa-demos uses, answering from the driver.
 *
 * Mesa is linked statically, so every `gl*` name is bound at link time and glad's
 * loading half is absent. Its reporting half - `GLAD_GL_VERSION_*` and the extension
 * flags demos test before taking a path (`fogcoord.c:119`) - is queried from the
 * driver, never asserted, so a demo skips an absent extension instead of drawing
 * nothing. The list holds the names the demos reference; a missing one fails to
 * compile by name.
 */

#ifndef OOPS_MESA_DEMOS_GLAD_H
#define OOPS_MESA_DEMOS_GLAD_H

/*
 * The GL headers on the include path are included rather than shadowed. `<GL/gl.h>` is
 * oops-sdk's GL 2.0 header (oops-sdk#D008) and `<GL/glext.h>` is Mesa's. Mesa's
 * `glext.h` declares its functions only under `GL_GLEXT_PROTOTYPES` (`glext.h:96`).
 */
#define GL_GLEXT_PROTOTYPES 1

#include <GL/gl.h>
#include <GL/glext.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Queries the driver and sets the flags. Safe to call more than once; with no current
 * context every flag stays zero. Returns non-zero on success, as glad does.
 */
int gladLoadGL(void);

/* The flags, `int` globals as real glad declares them. Zero until `gladLoadGL` runs. */
extern int GLAD_GL_VERSION_1_1;
extern int GLAD_GL_VERSION_2_0;

extern int GLAD_GL_ARB_seamless_cube_map;
extern int GLAD_GL_ARB_texture_cube_map;
extern int GLAD_GL_ARB_window_pos;
extern int GLAD_GL_EXT_compiled_vertex_array;
extern int GLAD_GL_EXT_fog_coord;
extern int GLAD_GL_EXT_framebuffer_object;
extern int GLAD_GL_EXT_polygon_offset;
extern int GLAD_GL_MESA_window_pos;
extern int GLAD_GL_SGIS_generate_mipmap;

#ifdef __cplusplus
}
#endif

#endif /* OOPS_MESA_DEMOS_GLAD_H */
