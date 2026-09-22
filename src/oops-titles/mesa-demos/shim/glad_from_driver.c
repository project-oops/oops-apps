/*
 * glad's reporting half, answered from the driver. See `shim/include/glad/glad.h` for why the
 * loading half is absent and why these flags are queried rather than asserted.
 */

#include "glad/glad.h"

#include <string.h>

int GLAD_GL_VERSION_1_1;
int GLAD_GL_VERSION_2_0;

int GLAD_GL_ARB_seamless_cube_map;
int GLAD_GL_ARB_texture_cube_map;
int GLAD_GL_ARB_window_pos;
int GLAD_GL_EXT_compiled_vertex_array;
int GLAD_GL_EXT_fog_coord;
int GLAD_GL_EXT_framebuffer_object;
int GLAD_GL_EXT_polygon_offset;
int GLAD_GL_MESA_window_pos;
int GLAD_GL_SGIS_generate_mipmap;

/*
 * Whole-word search of a space-separated list.
 *
 * `strstr` alone is wrong here and the bug it causes is quiet: `GL_EXT_texture` matches inside
 * `GL_EXT_texture3D`, so a driver offering only the latter would be reported as offering both.
 * The boundary checks are what make this a measurement rather than a guess.
 */
static int has_word(const char *list, const char *word)
{
    if (list == NULL || word == NULL) {
        return 0;
    }

    const size_t n = strlen(word);
    const char *p = list;

    for (;;) {
        p = strstr(p, word);
        if (p == NULL) {
            return 0;
        }

        const int left_ok = (p == list) || (p[-1] == ' ');
        const int right_ok = (p[n] == '\0') || (p[n] == ' ');
        if (left_ok && right_ok) {
            return 1;
        }
        p += n;
    }
}

/*
 * Ask for the extension list both ways.
 *
 * `glGetString(GL_EXTENSIONS)` is the old spelling and it works on this driver, which reports a
 * *compatibility* profile. It is deprecated in core profiles and returns NULL there, so the
 * indexed form is tried as well - the same pair real glad uses, and cheap insurance against the
 * day this context is created differently.
 *
 * Returns 1 if the extension is present by either route.
 */
static int have_extension(const char *name)
{
    const GLubyte *all = glGetString(GL_EXTENSIONS);
    if (all != NULL && has_word((const char *)all, name)) {
        return 1;
    }

    /*
     * **The indexed form is guarded, and the reason is worth knowing before porting anything
     * else to this renderer.** A `USE_MESA` title compiles with `-I<oops-sdk>/include` *ahead* of
     * `-I<oops-mesa>/mesa/include`, so `<GL/gl.h>` is oops-sdk's - which covers GL 1.x and 2.0
     * (oops-sdk#D008) and declares neither `GL_NUM_EXTENSIONS` nor `glGetStringi`. The driver
     * underneath is Mesa and reports 4.6; the headers in front of it are 2.0. They disagree, and
     * the compiler follows the headers.
     *
     * That costs nothing here: this context is a compatibility profile, so the flat string above
     * always answers. A title that genuinely needs a GL 3.0+ entry point has to include Mesa's
     * header explicitly rather than assume `<GL/gl.h>` is Mesa's.
     */
#ifdef GL_NUM_EXTENSIONS
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);

    /* A context that does not support the indexed query leaves an error behind rather than a
     * count; clear it so the demo does not later find someone else's. */
    if (count <= 0) {
        (void)glGetError();
        return 0;
    }

    for (GLint i = 0; i < count; i++) {
        const GLubyte *one = glGetStringi(GL_EXTENSIONS, (GLuint)i);
        if (one != NULL && strcmp((const char *)one, name) == 0) {
            return 1;
        }
    }
#endif
    return 0;
}

int gladLoadGL(void)
{
    const GLubyte *version = glGetString(GL_VERSION);
    if (version == NULL) {
        /* No current context. Every flag stays zero and the demos take their "not supported"
         * paths, which is the honest answer rather than a fault. */
        return 0;
    }

    /*
     * The version string begins with `major.minor`, per the GL specification, whatever vendor
     * text follows it. This driver answers `4.6 (Compatibility Profile) Mesa 26.2.2 ...`.
     * Parsed by hand rather than with `sscanf` so the digits are the only thing trusted.
     */
    int major = 0;
    int minor = 0;
    const char *v = (const char *)version;

    while (*v >= '0' && *v <= '9') {
        major = major * 10 + (*v - '0');
        v++;
    }
    if (*v == '.') {
        v++;
        while (*v >= '0' && *v <= '9') {
            minor = minor * 10 + (*v - '0');
            v++;
        }
    }

    GLAD_GL_VERSION_1_1 = (major > 1) || (major == 1 && minor >= 1);
    GLAD_GL_VERSION_2_0 = (major > 2) || (major == 2 && minor >= 0);

    GLAD_GL_ARB_seamless_cube_map      = have_extension("GL_ARB_seamless_cube_map");
    GLAD_GL_ARB_texture_cube_map       = have_extension("GL_ARB_texture_cube_map");
    GLAD_GL_ARB_window_pos             = have_extension("GL_ARB_window_pos");
    GLAD_GL_EXT_compiled_vertex_array  = have_extension("GL_EXT_compiled_vertex_array");
    GLAD_GL_EXT_fog_coord              = have_extension("GL_EXT_fog_coord");
    GLAD_GL_EXT_framebuffer_object     = have_extension("GL_EXT_framebuffer_object");
    GLAD_GL_EXT_polygon_offset         = have_extension("GL_EXT_polygon_offset");
    GLAD_GL_MESA_window_pos            = have_extension("GL_MESA_window_pos");
    GLAD_GL_SGIS_generate_mipmap       = have_extension("GL_SGIS_generate_mipmap");

    return 1;
}
