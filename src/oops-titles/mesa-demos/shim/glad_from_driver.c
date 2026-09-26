/*
 * glad's reporting half, answered from the driver. See `shim/include/glad/glad.h`.
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
 * Whole-word search of a space-separated list: `strstr` alone would find
 * `GL_EXT_texture` inside `GL_EXT_texture3D`.
 */
static int has_word(const char *list, const char *word) {
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
 * Returns 1 if the extension is present in `glGetString(GL_EXTENSIONS)`, which answers
 * in a compatibility profile, or in the indexed list, which a core profile needs.
 */
static int have_extension(const char *name) {
    const GLubyte *all = glGetString(GL_EXTENSIONS);
    if (all != NULL && has_word((const char *)all, name)) {
        return 1;
    }

    /* Guarded: a `USE_MESA` title's `<GL/gl.h>` is oops-sdk's GL 2.0 header
     * (oops-sdk#D008), which may not declare `GL_NUM_EXTENSIONS`. */
#ifdef GL_NUM_EXTENSIONS
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);

    /* A context without the indexed query leaves an error; clear it for the demo. */
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

int gladLoadGL(void) {
    const GLubyte *version = glGetString(GL_VERSION);
    if (version == NULL) {
        /* No current context: every flag stays zero and demos take their "not
         * supported" paths. */
        return 0;
    }

    /* The version string begins with `major.minor` per the GL specification; vendor
     * text follows. */
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

    GLAD_GL_ARB_seamless_cube_map = have_extension("GL_ARB_seamless_cube_map");
    GLAD_GL_ARB_texture_cube_map = have_extension("GL_ARB_texture_cube_map");
    GLAD_GL_ARB_window_pos = have_extension("GL_ARB_window_pos");
    GLAD_GL_EXT_compiled_vertex_array = have_extension("GL_EXT_compiled_vertex_array");
    GLAD_GL_EXT_fog_coord = have_extension("GL_EXT_fog_coord");
    GLAD_GL_EXT_framebuffer_object = have_extension("GL_EXT_framebuffer_object");
    GLAD_GL_EXT_polygon_offset = have_extension("GL_EXT_polygon_offset");
    GLAD_GL_MESA_window_pos = have_extension("GL_MESA_window_pos");
    GLAD_GL_SGIS_generate_mipmap = have_extension("GL_SGIS_generate_mipmap");

    return 1;
}
