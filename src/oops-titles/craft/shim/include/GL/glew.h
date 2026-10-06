/*
 * GLEW's names, for a target where oops-gl is linked into the payload and nothing is
 * loaded at run time.
 *
 * This is `<GL/gl.h>` plus the names Craft touches, so a GL function oops-gl lacks is a
 * symbol the link names rather than a null pointer called at run time.
 * `glewExperimental` is defined in `glfw_shim.c` so Craft's assignment compiles.
 */
#ifndef OOPS_CRAFT_GLEW_H
#define OOPS_CRAFT_GLEW_H

#include <GL/gl.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GLEW_OK 0

typedef unsigned char GLboolean_glew;
extern int glewExperimental;

/* Always GLEW_OK: there is nothing to load and nothing that can fail. */
int glewInit(void);
/* GLEW's error string, for Craft's `glewInit` failure branch. */
const char *glewGetErrorString(int error);

/* `glUniform1f`, which also logs once what the block program's `fog_distance` received
 * and what GL reads back after it (`glfw_shim.c`). */
void craft_glUniform1f(GLint location, GLfloat v0);
#define glUniform1f craft_glUniform1f

#ifdef __cplusplus
}
#endif

#endif /* OOPS_CRAFT_GLEW_H */
