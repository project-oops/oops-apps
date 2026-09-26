/*
 * gl2-cube: the shader pair the cube is drawn with.
 *
 * In a header of its own so the source a recorded frame was compiled from is
 * identifiable and stays unchanged. Minimal GLSL 1.10 - one attribute, one uniform, one
 * varying, no built-ins beyond the constructors - the smallest program that needs a
 * parameter export.
 */

#ifndef GL2_CUBE_SHADERS_H
#define GL2_CUBE_SHADERS_H

/* One varying: a GLSL `varying` is a parameter export. */
static const char *const GL2_CUBE_VERTEX_SHADER =
    "uniform mat4 mvp;\n"
    "attribute vec3 pos;\n"
    "attribute vec3 colour;\n"
    "varying vec3 vcolour;\n"
    "void main() {\n"
    "  vcolour = colour;\n"
    "  gl_Position = mvp * vec4(pos, 1.0);\n"
    "}\n";

static const char *const GL2_CUBE_FRAGMENT_SHADER =
    "varying vec3 vcolour;\n"
    "void main() {\n"
    "  gl_FragColor = vec4(vcolour, 1.0);\n"
    "}\n";

#endif /* GL2_CUBE_SHADERS_H */
