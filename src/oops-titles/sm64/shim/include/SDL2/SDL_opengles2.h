/*
 * <SDL2/SDL_opengles2.h>, answered with oops-gl's own header.
 *
 * Upstream's gfx_opengl.c includes this for its entry points. SDL's copy would
 * redeclare every one of them with Khronos ES types; the declarations here come from
 * the library that defines them. oops-sdk declares its shader, buffer and framebuffer
 * entry points in GL/gl.h and ships no separate glext.h.
 */
#ifndef SM64_SHIM_SDL2_SDL_OPENGLES2_H
#define SM64_SHIM_SDL2_SDL_OPENGLES2_H

#include <GL/gl.h>

#endif /* SM64_SHIM_SDL2_SDL_OPENGLES2_H */
