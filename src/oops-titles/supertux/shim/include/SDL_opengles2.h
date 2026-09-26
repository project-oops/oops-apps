/*
 * `<SDL_opengles2.h>`, answered with oops-gl's own header.
 *
 * Upstream's `video/gl.hpp` includes only this when `USE_OPENGLES2` is set. SDL's copy
 * would redeclare every oops-gl entry point with Khronos ES types; the declarations
 * here come from the library that defines them.
 *
 * The vertex-array-object calls must not be declared through here: `video/gl.hpp`
 * defines `glGenVertexArrays`, `glDeleteVertexArrays` and `glBindVertexArray` as empty
 * inline functions after this include. `tools/glcheck.c` fails if oops-gl declares
 * them.
 */
#ifndef STX_SHIM_SDL_OPENGLES2_H
#define STX_SHIM_SDL_OPENGLES2_H

/* `GL/gl.h` alone: oops-sdk declares its shader, framebuffer and buffer entry points
 * there and ships no separate `glext.h`. `tools/glcheck.c` reads the same file. */
#include <GL/gl.h>

#endif /* STX_SHIM_SDL_OPENGLES2_H */
