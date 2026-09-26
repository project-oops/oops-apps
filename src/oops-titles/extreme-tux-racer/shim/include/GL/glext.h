/*
 * The two extension typedefs ETR expects a system <GL/glext.h> to carry.
 *
 * `ogl.h` defines `PFNGLLOCKARRAYSEXTPROC` itself for Mac, native Win32 and GLES, and
 * on every other platform expects the GL headers to have them. oops-gl's `<GL/gl.h>` is
 * core GL 1.x and ships no extension header, so this supplies the pair and nothing
 * else.
 *
 * Types only. oops-gl does not implement `GL_EXT_compiled_vertex_array`; ETR looks
 * the functions up through `SDL_GL_GetProcAddress`, which returns NULL here, so ETR
 * takes its non-extension path. Declared functions would link against nothing, and a
 * payload link does not report that.
 */
#ifndef OOPS_ETR_GL_GLEXT_H
#define OOPS_ETR_GL_GLEXT_H

#include <GL/gl.h>

typedef void (*PFNGLLOCKARRAYSEXTPROC)(GLint first, GLsizei count);
typedef void (*PFNGLUNLOCKARRAYSEXTPROC)(void);

#endif
