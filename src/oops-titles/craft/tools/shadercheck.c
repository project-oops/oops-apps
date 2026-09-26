/*
 * Every shader Craft ships, compiled for the console on a build machine.
 *
 * oops-gl's GL 2.0 back end compiles a fragment shader to gfx1030 instructions or
 * refuses it, and a refused shader fails its draw with `GL_INVALID_OPERATION`. This
 * reads `upstream/shaders/` directly and prints each pair's cost against the four
 * limits that can refuse one, on success too: varying floats (16, four exported
 * parameters), uniform floats (one draw's block), texture sets (2, in the same block)
 * and pixel-stage registers (136, the frame's stage table allocation).
 */
#include "oops/display.h"
#include "oops/memory.h"
#include "src/gl/gl_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The limits, named so the report can say how close a shader is. */
#define MAX_VARYING_FLOATS 16
#define MAX_UNIFORM_FLOATS OOPS_GL_GL2_UNIFORM_FLOATS
#define MAX_TEX_SETS 2
#define MAX_VGPRS 136

/* ---------------------------------------------------------------------------
 * A host display and allocator, standing in for the SDK's direct-memory ones.
 * ------------------------------------------------------------------------- */
static uint32_t *s_fb;
static int s_dummy = 1;
static unsigned int s_w = 64, s_h = 64;

oops_display_t *oops_display_open(oops_display_backend_t b, unsigned int w,
                                  unsigned int h) {
    (void)b;
    s_w = w ? w : 64;
    s_h = h ? h : 64;
    if (!s_fb)
        s_fb = (uint32_t *)calloc((size_t)s_w * (size_t)s_h, sizeof(uint32_t));
    return (oops_display_t *)&s_dummy;
}
void oops_display_close(oops_display_t *d) {
    (void)d;
}
int oops_display_flip(oops_display_t *d) {
    (void)d;
    return 0;
}
uint32_t *oops_display_get_framebuffer(oops_display_t *d) {
    (void)d;
    return s_fb;
}
unsigned int oops_display_get_width(const oops_display_t *d) {
    (void)d;
    return s_w;
}
unsigned int oops_display_get_height(const oops_display_t *d) {
    (void)d;
    return s_h;
}
int oops_display_is_gpu_accelerated(const oops_display_t *d) {
    (void)d;
    return 0;
}
int oops_display_is_ready(const oops_display_t *d) {
    return d != (const oops_display_t *)0;
}
void *oops_mem_alloc(size_t n, size_t a, oops_mem_type_t t) {
    (void)a;
    (void)t;
    return malloc(n);
}
void oops_mem_free(void *p) {
    free(p);
}

/* ------------------------------------------------------------------------- */

static void *g_ctx;
static int g_failures;

static char *slurp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return (char *)0;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) {
        fclose(f);
        return (char *)0;
    }
    char *b = (char *)malloc((size_t)n + 1u);
    if (!b) {
        fclose(f);
        return (char *)0;
    }
    const size_t got = fread(b, 1, (size_t)n, f);
    b[got] = '\0';
    fclose(f);
    return b;
}

static GLuint compile_one(GLenum type, const char *src, const char *what,
                          const char *name) {
    GLuint sh = glCreateShader(type);
    const GLchar *strings[1];
    strings[0] = src;
    glShaderSource(sh, 1, strings, (const GLint *)0);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512] = {0};
        glGetShaderInfoLog(sh, (GLsizei)sizeof(log), (GLsizei *)0, log);
        printf("  %-8s FAIL  %s does not compile: %s\n", name, what, log);
        g_failures++;
        return 0u;
    }
    return sh;
}

/* Each `<name>_vertex.glsl` / `<name>_fragment.glsl` pair, through the whole front end
 * and then through the console back end. */
static void check_pair(const char *dir, const char *name) {
    char vpath[512], fpath[512];
    snprintf(vpath, sizeof(vpath), "%s/%s_vertex.glsl", dir, name);
    snprintf(fpath, sizeof(fpath), "%s/%s_fragment.glsl", dir, name);

    char *vsrc = slurp(vpath);
    char *fsrc = slurp(fpath);
    if (!vsrc || !fsrc) {
        printf("  %-8s FAIL  cannot read %s\n", name, vsrc ? fpath : vpath);
        g_failures++;
        free(vsrc);
        free(fsrc);
        return;
    }

    const GLuint vs = compile_one(GL_VERTEX_SHADER, vsrc, "the vertex shader", name);
    const GLuint fs =
        compile_one(GL_FRAGMENT_SHADER, fsrc, "the fragment shader", name);
    free(vsrc);
    free(fsrc);
    if (!vs || !fs)
        return;

    const GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512] = {0};
        glGetProgramInfoLog(prog, (GLsizei)sizeof(log), (GLsizei *)0, log);
        printf("  %-8s FAIL  does not link: %s\n", name, log);
        g_failures++;
        return;
    }

    const gl_program_object_t *p = gl_find_program((gl_context_t *)g_ctx, prog);
    if (!p) {
        printf("  %-8s FAIL  linked program has vanished\n", name);
        g_failures++;
        return;
    }

    static uint32_t words[OOPS_GL_PS_GL2_WORDS];
    uint32_t count = 0u, vgprs = 0u;
    char log[512] = {0};
    /* Null for the user-SGPR count and input-enable mask, which configure a draw and
       are not limits this tool reports. `gl_program_compile_fragment` guards every
       output write (glsl_ps.c:213-218 and :765-771). */
    if (!gl_program_compile_fragment(p, words, OOPS_GL_PS_GL2_WORDS, &count, &vgprs,
                                     (uint32_t *)0, (uint32_t *)0, log, sizeof(log))) {
        printf("  %-8s FAIL  no console code: %s\n", name, log);
        g_failures++;
        return;
    }

    printf("  %-8s pass  %3u words  %3u/%d regs  %2d/%d varying  %2d/%d uniform  %d/%d "
           "tex\n",
           name, count, vgprs, MAX_VGPRS, p->varying_floats, MAX_VARYING_FLOATS,
           p->value_floats, MAX_UNIFORM_FLOATS, p->hw_tex_sets, MAX_TEX_SETS);
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "upstream/shaders";

    oops_display_t *disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, 64, 64);
    g_ctx = glContextCreate(disp);
    if (!g_ctx) {
        printf("craft shadercheck: FAILED (no GL context; nothing was measured)\n");
        return 1;
    }
    glContextMakeCurrent(g_ctx);
    /* A context exposes only its version's entry points and defaults to 1.1. */
    glContextSetVersion(2, 0);

    static const char *const NAMES[] = {"block", "line", "sky", "text"};
    printf("craft shadercheck: %s\n", dir);
    for (size_t i = 0; i < sizeof(NAMES) / sizeof(NAMES[0]); i++) {
        check_pair(dir, NAMES[i]);
    }

    const int n = (int)(sizeof(NAMES) / sizeof(NAMES[0]));
    printf("craft shadercheck: %d/%d shader pairs compile for the console\n",
           n - g_failures, n);
    return g_failures == 0 ? 0 : 1;
}
