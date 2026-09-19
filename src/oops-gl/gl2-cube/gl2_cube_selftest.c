/*
 * Host self-test for gl2-cube.
 *
 * **gl2-cube cannot draw yet, and this test does not pretend otherwise.** The GL 2.0 back end
 * is blocked on one hardware measurement - whether this pipeline accepts a third parameter
 * export, which is what a GLSL `varying` compiles to (obSCEne REQ-...-f9d3). Until that
 * answers, `glCreateShader` and the rest have nowhere to go.
 *
 * What *is* finishable today is the front end, and that is what this checks: the two shaders in
 * `gl2_cube_shaders.h` are pushed through oops-gl's real preprocessor, parser and semantic
 * stage and must come out clean. That is worth having on its own - it means the shaders the
 * oracle will eventually be recorded with are known-good GLSL 1.10 now, rather than being
 * debugged at the same moment as the code generator and the hardware path.
 *
 * The test also drives a deliberately *broken* shader through the same path and requires it to
 * be rejected, because a front end that accepts everything would pass the first half of this
 * test while proving nothing.
 */

#include "gl2_cube_shaders.h"

#include "src/gl/glsl_internal.h"

#include <stdio.h>
#include <string.h>

static glsl_ast_t s_ast;
static glsl_sema_t s_sema;
static glsl_pp_t s_pp;

/* Runs a shader through preprocessor, parser and semantic stage exactly as a real
 * glCompileShader will have to. Returns NULL on success or the first diagnostic. */
static const char *compile_check(const char *src) {
    /* The preprocessor first, so a `#version` line is consumed rather than met by the parser as
     * a stray `#`. These two shaders carry none, but the ones a real program hands over will. */
    glsl_pp_init(&s_pp, src, strlen(src));
    glsl_token_t t;
    while (glsl_pp_next(&s_pp, &t)) { /* drain: this stage's job here is to find errors */ }
    if (s_pp.error) return s_pp.error;

    glsl_parser_t p;
    glsl_parser_init(&p, &s_ast, src, strlen(src));
    int32_t unit = glsl_parse_translation_unit(&p);
    if (p.error || unit == GLSL_NO_NODE) return p.error ? p.error : "parse failed";

    glsl_sema_init(&s_sema, &s_ast);
    /* The built-ins a 1.10 shader may assume. They are declared here rather than in the
     * compiler because the built-in table is not written yet; when it is, this goes. */
    glsl_declare(&s_sema, "gl_Position", 11, GLSL_TYPE_VEC4, GL_FALSE);
    glsl_declare(&s_sema, "gl_FragColor", 12, GLSL_TYPE_VEC4, GL_FALSE);
    if (!glsl_check_unit(&s_sema, unit)) {
        return s_sema.error ? s_sema.error : "semantic check failed";
    }
    return NULL;
}

int main(void) {
    int failures = 0;

    const char *vs_err = compile_check(GL2_CUBE_VERTEX_SHADER);
    printf("  vertex-shader    %s%s%s\n", vs_err ? "FAIL: " : "pass",
           vs_err ? vs_err : "", vs_err ? "" : "");
    if (vs_err) failures++;

    const char *fs_err = compile_check(GL2_CUBE_FRAGMENT_SHADER);
    printf("  fragment-shader  %s%s\n", fs_err ? "FAIL: " : "pass", fs_err ? fs_err : "");
    if (fs_err) failures++;

    /* **A front end that accepts everything would pass the two checks above.** This one has to
     * be rejected: `vec3 * mat4` has dimensions that do not meet. */
    const char *bad_err = compile_check(
        "uniform mat4 mvp;\n"
        "attribute vec3 pos;\n"
        "void main() { gl_Position = vec4(pos * mvp, 1.0); }\n");
    printf("  rejects-bad-glsl %s\n", bad_err ? "pass" : "FAIL: accepted a bad shader");
    if (!bad_err) failures++;

    /* And a swizzle past the end of its operand, which is the other shape of mistake a shader
     * author actually makes. */
    const char *swz_err = compile_check(
        "attribute vec2 uv;\n"
        "varying float v;\n"
        "void main() { v = uv.z; }\n");
    printf("  rejects-bad-swizzle %s\n", swz_err ? "pass" : "FAIL: accepted uv.z on a vec2");
    if (!swz_err) failures++;

    printf("gl2-cube selftest: %s (GLSL front end only - no GL 2.0 back end yet)\n",
           failures == 0 ? "ok" : "FAILED");
    return failures == 0 ? 0 : 1;
}
