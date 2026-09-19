/*
 * gl2-cube: the shader pair the GL 2.0 oracle will be recorded with.
 *
 * Kept in a header of its own because **these two strings are the thing the oracle is about**.
 * gl1-cube's oracle pins a command stream produced by fixed-function state; gl2-cube's will pin
 * one produced by compiling these. When the record is made, the source that produced it has to
 * be identifiable at a glance and unchanged afterwards - a shader edited between the recording
 * and the next run turns the record into a description of something that no longer exists.
 *
 * Deliberately **minimal GLSL 1.10**: one attribute, one uniform, one varying, no built-in
 * functions beyond the constructors. The point is to be the smallest program that needs a
 * parameter export, because that is the one thing the hardware has not yet confirmed
 * (obSCEne REQ-...-f9d3). A shader with more in it would fail for more reasons.
 */

#ifndef GL2_CUBE_SHADERS_H
#define GL2_CUBE_SHADERS_H

/* One varying, which is the whole question: a GLSL `varying` *is* a parameter export, and
 * oops-gl's pipeline exports exactly two parameters today. */
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
