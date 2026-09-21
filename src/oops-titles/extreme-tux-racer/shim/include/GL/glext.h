/*
 * The two extension typedefs ETR expects a system <GL/glext.h> to carry.
 *
 * `ogl.h` defines `PFNGLLOCKARRAYSEXTPROC` itself for Mac, native Win32 and GLES, and on every
 * other platform expects the GL headers to have them. oops-gl's `<GL/gl.h>` is core GL 1.x and
 * ships no extension header, so this supplies the pair and nothing else.
 *
 * **Types only, deliberately.** `GL_EXT_compiled_vertex_array` is not implemented by oops-gl,
 * and `ogl.h` `#undef`s the feature macro immediately above these lines anyway - ETR looks the
 * functions up at run time through `SDL_GL_GetProcAddress`, which this SDK's backend answers
 * with NULL because every entry point is statically linked. So ETR sees the extension as absent
 * and takes its non-extension path, which is the truth.
 *
 * Declaring the *functions* here instead would be the mistake: they would link against nothing
 * and a payload link does not complain.
 */
#ifndef OOPS_ETR_GL_GLEXT_H
#define OOPS_ETR_GL_GLEXT_H

#include <GL/gl.h>

typedef void (*PFNGLLOCKARRAYSEXTPROC)(GLint first, GLsizei count);
typedef void (*PFNGLUNLOCKARRAYSEXTPROC)(void);

#endif
