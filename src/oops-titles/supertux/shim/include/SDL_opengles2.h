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

/*
 * oops-gl grew real vertex-array objects, so `GL/gl.h` declares these three now and
 * `video/gl.hpp`'s inline no-ops stopped compiling beside them - a C++ inline
 * definition and an `extern "C"` declaration of the same name cannot share a
 * translation unit. Renaming them across the include is what "keep them apart" means
 * here: the declarations still exist, under names nothing refers to, and upstream's
 * stubs keep the names.
 *
 * This preserves what SuperTux had - the stubs were no-ops and it rendered correctly
 * without them. Taking oops-gl's real vertex arrays instead means dropping the stubs
 * from `video/gl.hpp` in a patch, which changes what the renderer binds and wants a
 * hardware run behind it, so it belongs to whoever owns this port rather than to the
 * change that made it possible.
 */
#define glGenVertexArrays stx_shim_unused_glGenVertexArrays
#define glDeleteVertexArrays stx_shim_unused_glDeleteVertexArrays
#define glBindVertexArray stx_shim_unused_glBindVertexArray

/* `GL/gl.h` alone: oops-sdk declares its shader, framebuffer and buffer entry points
 * there and ships no separate `glext.h`. `tools/glcheck.c` reads the same file. */
#include <GL/gl.h>

#undef glGenVertexArrays
#undef glDeleteVertexArrays
#undef glBindVertexArray

#endif /* STX_SHIM_SDL_OPENGLES2_H */
