/*
 * glad, as much of it as mesa-demos uses, answering from the driver.
 *
 * # What glad is, and why almost none of it is needed here
 *
 * glad is a loader: on a desktop it resolves every GL entry point through
 * `glXGetProcAddress` or equivalent, because libGL exports only the GL 1.1 core and
 * everything newer arrives as a function pointer. That problem does not exist in this
 * title. Mesa is linked statically and every `gl*` name is already bound at link time -
 * `common/app.mk`'s undefined-symbol check would have refused the ELF otherwise. So the
 * loading half of glad is a no-op here, and this header does not reimplement it.
 *
 * # The half that is not a no-op, and must not be faked
 *
 * glad also *reports*: `GLAD_GL_VERSION_2_0` and the `GLAD_GL_<vendor>_<extension>`
 * booleans, which demos read to decide whether to take an extension path at all.
 * `fogcoord.c:119` returns early unless `GLAD_GL_EXT_fog_coord`; `engine.c:1331` calls
 * `gladLoadGL()` right after `glutCreateWindow` and trusts what it sets.
 *
 * **Those are answered by asking the driver, never by assertion.** Defining them all to
 * 1 would be one line and would be wrong in the specific way this collection exists to
 * avoid: a demo would take a path for an extension that is not there, and the result
 * would be a blank screen with nothing in the log - which is exactly how a black cube
 * and a black `gears` both cost a session on 2026-09-22, each looking like a driver
 * fault and being neither. A demo that correctly *skips* an absent extension and says
 * so is a better outcome than one that takes the path and draws nothing.
 *
 * So `gladLoadGL` walks the real extension string and parses the real version, and a
 * flag is zero when the driver does not offer the thing. That is a measurement, and it
 * is the only reason this file is allowed to exist under a title whose job is
 * measuring.
 *
 * # Scope
 *
 * Eleven names, because eleven is what the 27 demos that include glad actually
 * reference. A twelfth will fail to compile and name itself, which is the right way for
 * this list to grow - the same discipline `src/oops-deps/libcxx/oops-libcxx.mk` states
 * for its four-of-forty-five source list.
 */

#ifndef OOPS_MESA_DEMOS_GLAD_H
#define OOPS_MESA_DEMOS_GLAD_H

/*
 * Real glad declares the whole of GL itself. Here Mesa's own headers are already on the
 * include path and are the authority on what this build offers, so they are included
 * rather than shadowed. `glext.h` is what carries the extension entry points the demos
 * below call once their flag says yes.
 */
/*
 * **`GL_GLEXT_PROTOTYPES` is not optional and its absence is confusing.** Mesa's
 * `glext.h` puts every `GLAPI ... APIENTRY` declaration behind this macro
 * (`glext.h:96`, `:248`, `:397`) and leaves the enums and typedefs outside it. So
 * without it you get `GL_NUM_EXTENSIONS` defined and `glGetStringi` undeclared - a
 * combination that reads like a header bug and is not one. On a desktop the loader
 * would have supplied the pointer and the question would not arise; here every symbol
 * is linked in already, so the prototype is all that is missing.
 *
 * Note also that `<GL/gl.h>` here is **oops-sdk's**, not Mesa's: a `USE_MESA` title
 * compiles with
 * `-I<oops-sdk>/include` ahead of `-I<oops-mesa>/mesa/include`, and oops-sdk's covers
 * GL 1.x and 2.0 (oops-sdk#D008). `<GL/glext.h>` has no oops-sdk counterpart, so *that*
 * one is Mesa's. The two headers in front of this driver come from different projects
 * and describe different versions of OpenGL, which is worth knowing before wondering
 * why a 3.0 entry point will not compile against a driver that reports 4.6.
 */
#define GL_GLEXT_PROTOTYPES 1

#include <GL/gl.h>
#include <GL/glext.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Load, which here means query. Safe to call more than once and safe to call before a
 * context exists - with no current context `glGetString` returns NULL, every flag stays
 * zero, and the demos take their "not supported" paths rather than faulting. Returns
 * non-zero on success, the way glad does, so `if (!gladLoadGL())` behaves.
 */
int gladLoadGL(void);

/*
 * The flags. `int` and not `bool`, and globals rather than macros, because the demos
 * read them as plain identifiers (`if (!GLAD_GL_EXT_fog_coord)`) and real glad declares
 * them exactly this way. Zero until `gladLoadGL` has run.
 */
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
