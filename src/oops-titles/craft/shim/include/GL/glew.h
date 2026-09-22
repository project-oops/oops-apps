/*
 * GLEW, for a target that does not need it.
 *
 * GLEW exists to find OpenGL entry points at run time, because on a desktop the driver's GL is
 * whatever happened to be installed and the headers are whatever the SDK shipped. Neither is
 * true here: oops-gl **is** the GL, it is linked into the payload, and every function Craft
 * calls is a symbol the linker resolves or an error at build time.
 *
 * So this is not a port of GLEW and not a loader. It is `<GL/gl.h>` plus the two names Craft
 * touches, and its value is that the failure mode moves: a GL function that oops-gl does not
 * have becomes an **undefined symbol at link**, naming the function, instead of a null pointer
 * that GLEW dutifully loads as zero and the game calls in its first frame.
 *
 * `glewExperimental` is a variable Craft assigns to before `glewInit`. It is defined in the
 * shim's translation unit so that the assignment compiles and goes nowhere.
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
/* GLEW's error string, for the branch Craft writes around `glewInit` and never takes. */
const char *glewGetErrorString(int error);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_CRAFT_GLEW_H */
