/*
 * `<SDL_opengles2.h>`, answered with oops-gl's own header.
 *
 * Upstream's `video/gl.hpp` includes this and nothing else when `USE_OPENGLES2` is set. SDL's
 * copy would bring the Khronos ES 2.0 prototypes, which are a second declaration of every entry
 * point oops-gl already declares - with ES types where oops-gl has desktop ones, and with no
 * guarantee the two agree. One set of declarations, the one the library that defines them ships,
 * is the only arrangement a mismatch cannot hide in.
 *
 * **What must stay out of here: the vertex-array-object calls.** `video/gl.hpp` defines
 * `glGenVertexArrays`, `glDeleteVertexArrays` and `glBindVertexArray` as empty inline functions
 * straight after this include, because ES 2.0 has none. oops-gl does not declare them today, so
 * nothing collides; `tools/glcheck.c` fails the day it does.
 */
#ifndef STX_SHIM_SDL_OPENGLES2_H
#define STX_SHIM_SDL_OPENGLES2_H

/* `GL/gl.h` alone: oops-sdk declares its shader, framebuffer and buffer entry points there and
 * ships no separate `glext.h`. `tools/glcheck.c` reads the same file. */
#include <GL/gl.h>

#endif /* STX_SHIM_SDL_OPENGLES2_H */
