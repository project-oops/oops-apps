/*
 * gl-host - run an unmodified GL 1.x program against oops-gl, on a desktop.
 *
 * Preloaded in front of the system GL, it makes a program's GL calls land in oops-gl,
 * dumps the frames oops-gl draws, and reports GL errors and missing entry points.
 * Configured by environment, since the program owns its command line:
 * OOPS_GL_HOST_W/_H (display, default 800x600), OOPS_GL_HOST_DUMP (frame path prefix),
 * OOPS_GL_HOST_EVERY (dump one frame in N), OOPS_GL_HOST_MAX (frames to dump, default
 * 8), OOPS_GL_HOST_REPORT (report path, default stderr).
 *
 *   OOPS_GL_HOST_DUMP=/tmp/nb LD_PRELOAD=./liboopsGL.so ./neverball
 */

#define _GNU_SOURCE

#include <oops/display.h>
#include <GL/gl.h>

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * The display: a plain buffer standing in for the console's display engine. The
 * software rasteriser writes into it and the dump reads it back out.
 * -------------------------------------------------------------------------- */

static uint32_t *s_fb;
static int s_disp_dummy = 1;
static unsigned int s_w = 800;
static unsigned int s_h = 600;

oops_display_t *oops_display_open(oops_display_backend_t backend, unsigned int width,
                                  unsigned int height) {
    (void)backend;
    s_w = width ? width : 800;
    s_h = height ? height : 600;
    free(s_fb);
    s_fb = (uint32_t *)calloc((size_t)s_w * (size_t)s_h, sizeof(uint32_t));
    if (!s_fb)
        return (oops_display_t *)0;
    return (oops_display_t *)&s_disp_dummy;
}

int oops_display_is_ready(const oops_display_t *disp) {
    (void)disp;
    return 1;
}
void oops_display_close(oops_display_t *disp) {
    (void)disp;
}
int oops_display_flip(oops_display_t *disp) {
    (void)disp;
    return 0;
}
uint32_t *oops_display_get_framebuffer(oops_display_t *disp) {
    (void)disp;
    return s_fb;
}
unsigned int oops_display_get_width(const oops_display_t *disp) {
    (void)disp;
    return s_w;
}
unsigned int oops_display_get_height(const oops_display_t *disp) {
    (void)disp;
    return s_h;
}

/* --------------------------------------------------------------------------
 * The context, and what happened while it was current.
 * -------------------------------------------------------------------------- */

static oops_display_t *s_disp;
static void *s_ctx;
static unsigned long s_frames; /* swaps seen */
static unsigned long s_dumped; /* frames written */
static unsigned long s_errors; /* GL errors caught at a swap boundary */
static GLenum s_first_error;   /* and the first of them, which is usually the cause */

static unsigned long env_num(const char *name, unsigned long dflt) {
    const char *v = getenv(name);
    if (!v || !*v)
        return dflt;
    char *end = (char *)0;
    const unsigned long n = strtoul(v, &end, 10);
    return (end && *end == '\0') ? n : dflt;
}

/* A constructor rather than lazy initialisation, since any entry point can be the first
 * call. The loader runs it before the program's main, so before any GL call. */
__attribute__((constructor)) static void gl_host_init(void) {
    const unsigned int w = (unsigned int)env_num("OOPS_GL_HOST_W", 800);
    const unsigned int h = (unsigned int)env_num("OOPS_GL_HOST_H", 600);
    s_disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, w, h);
    if (!s_disp) {
        fprintf(stderr, "gl-host: could not open a %ux%u display\n", w, h);
        return;
    }
    s_ctx = glContextCreate(s_disp);
    if (!s_ctx) {
        fprintf(stderr, "gl-host: could not create a GL context\n");
        return;
    }
    fprintf(stderr, "gl-host: oops-gl is the GL for this process (%ux%u)\n", w, h);

    /* Recording, if asked for, starts here - before the program's main and so before
       any texture it uploads. See `capture_tick`. */
    {
        const char *cap = getenv("OOPS_GL_HOST_CAPTURE");
        if (cap && *cap) {
            oops_gl_capture_begin();
            fprintf(stderr, "gl-host: recording from start, to end at frame %lu\n",
                    env_num("OOPS_GL_HOST_CAPTURE_FRAME", 60));
        }
    }
}

/* One frame, as a binary PPM: no library needed, and every image tool reads it. */
static void dump_frame(unsigned long n) {
    const char *prefix = getenv("OOPS_GL_HOST_DUMP");
    if (!prefix || !s_fb)
        return;

    char path[512];
    snprintf(path, sizeof(path), "%s-%04lu.ppm", prefix, n);
    FILE *f = fopen(path, "wb");
    if (!f)
        return;
    fprintf(f, "P6\n%u %u\n255\n", s_w, s_h);

    /* Read through glReadPixels rather than off the buffer, so this reports the pixels
     * oops-gl says it drew. PPM is top-down and GL's origin is bottom-left, so the rows
     * are reversed. */
    unsigned char *rgba = (unsigned char *)malloc((size_t)s_w * (size_t)s_h * 4u);
    if (!rgba) {
        fclose(f);
        return;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, (GLsizei)s_w, (GLsizei)s_h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

    for (unsigned int y = 0; y < s_h; y++) {
        const unsigned char *row = rgba + (size_t)(s_h - 1u - y) * (size_t)s_w * 4u;
        for (unsigned int x = 0; x < s_w; x++) {
            fputc(row[x * 4u + 0u], f);
            fputc(row[x * 4u + 1u], f);
            fputc(row[x * 4u + 2u], f);
        }
    }
    free(rgba);
    fclose(f);
    fprintf(stderr, "gl-host: wrote %s\n", path);
}

/* --------------------------------------------------------------------------
 * The frame boundary.
 *
 * A GL program tells its windowing library, SDL here, when a frame ends, so the swap is
 * interposed and then forwarded: the program keeps its real context and window.
 * -------------------------------------------------------------------------- */

/* Writes one frame's call stream to a file, for replay on hardware. The replay can be
 * cut at any call, so the call where the replayed frame stops matching the host's is
 * the one that breaks it. OOPS_GL_HOST_CAPTURE names the file and
 * OOPS_GL_HOST_CAPTURE_FRAME the frame to record, counted in swaps.
 */
static void capture_tick(unsigned long frame) {
    const char *path = getenv("OOPS_GL_HOST_CAPTURE");
    if (!path || !*path)
        return;
    const unsigned long want = env_num("OOPS_GL_HOST_CAPTURE_FRAME", 60);

    /* Recording starts in the constructor, not a frame before the one wanted: a title
       uploads its textures while it loads, and the upload has to be in the stream with
       the draw that uses it. */
    if (frame != want)
        return;

    oops_gl_capture_end();
    size_t bytes = 0;
    unsigned calls = 0;
    const void *data = oops_gl_capture_data(&bytes, &calls);
    if (!data) {
        fprintf(stderr, "gl-host: capture overflowed, nothing written\n");
        return;
    }
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "gl-host: cannot open %s\n", path);
        return;
    }
    const size_t wrote = fwrite(data, 1, bytes, f);
    fclose(f);
    fprintf(stderr, "gl-host: captured frame %lu - %u calls, %zu bytes -> %s\n", frame,
            calls, wrote, path);
}

void SDL_GL_SwapWindow(void *window) {
    s_frames++;
    capture_tick(s_frames);

    /* Asked at the swap rather than per call: glGetError clears as it reports, so a
     * per-call check would hide errors from the program. What is left at a swap is
     * what the program never examined. */
    for (;;) {
        const GLenum e = glGetError();
        if (e == GL_NO_ERROR)
            break;
        if (!s_errors)
            s_first_error = e;
        s_errors++;
        if (s_errors > 100000u)
            break;
    }

    const unsigned long every = env_num("OOPS_GL_HOST_EVERY", 1);
    const unsigned long max = env_num("OOPS_GL_HOST_MAX", 8);
    if (s_dumped < max && every && (s_frames % every) == 0u) {
        dump_frame(s_frames);
        s_dumped++;
    }

    static void (*real_swap)(void *);
    if (!real_swap) {
        *(void **)(&real_swap) = dlsym(RTLD_NEXT, "SDL_GL_SwapWindow");
    }
    if (real_swap)
        real_swap(window);
}

/* --------------------------------------------------------------------------
 * How a title finds the GL it is going to use.
 *
 * A GL 1.x program links the 1.1 entry points and asks for everything newer by name
 * through SDL (Neverball's share/glext.c takes the buffer-object family this way).
 * Interposing the lookup keeps those calls in oops-gl too, so buffer binds and draws
 * reach the same GL. Names this GL cannot answer are recorded for the report.
 * -------------------------------------------------------------------------- */

#define GL_HOST_MAX_MISSING 64
static unsigned long s_proc_ours;
static unsigned long s_proc_missing;
static const char *s_missing[GL_HOST_MAX_MISSING];
static char s_missing_store[GL_HOST_MAX_MISSING][64];

void *SDL_GL_GetProcAddress(const char *name) {
    if (name) {
        /* Ours first: the loader searches the executable, then the preloaded
         * libraries, so a name this library exports resolves here and not in the
         * system driver. */
        void *p = dlsym(RTLD_DEFAULT, name);
        if (p) {
            s_proc_ours++;
            return p;
        }
        if (s_proc_missing < GL_HOST_MAX_MISSING) {
            size_t n = strlen(name);
            if (n > sizeof(s_missing_store[0]) - 1u)
                n = sizeof(s_missing_store[0]) - 1u;
            memcpy(s_missing_store[s_proc_missing], name, n);
            s_missing_store[s_proc_missing][n] = '\0';
            s_missing[s_proc_missing] = s_missing_store[s_proc_missing];
        }
        s_proc_missing++;
    }
    /* Not ours: SDL answers, so the program still gets what it would otherwise get. */
    static void *(*real_gpa)(const char *);
    if (!real_gpa) {
        *(void **)(&real_gpa) = dlsym(RTLD_NEXT, "SDL_GL_GetProcAddress");
    }
    return real_gpa ? real_gpa(name) : (void *)0;
}

/* --------------------------------------------------------------------------
 * The report.
 *
 * What a title asked of this GL and did not get: the porting task list for that title.
 * -------------------------------------------------------------------------- */

__attribute__((destructor)) static void gl_host_report(void) {
    const char *path = getenv("OOPS_GL_HOST_REPORT");
    FILE *f = stderr;
    if (path && *path) {
        FILE *o = fopen(path, "w");
        if (o)
            f = o;
    }
    fprintf(f, "gl-host: %lu frames, %lu dumped, %lu GL errors", s_frames, s_dumped,
            s_errors);
    if (s_errors)
        fprintf(f, ", first 0x%04x", (unsigned)s_first_error);
    fprintf(f, "\ngl-host: %lu entry points served, %lu not ours\n", s_proc_ours,
            s_proc_missing);
    /* Every name the title looked up that this GL does not have. */
    const unsigned long shown =
        (s_proc_missing < GL_HOST_MAX_MISSING) ? s_proc_missing : GL_HOST_MAX_MISSING;
    for (unsigned long i = 0; i < shown; i++) {
        if (s_missing[i])
            fprintf(f, "gl-host:   absent: %s\n", s_missing[i]);
    }
    if (s_proc_missing > shown) {
        fprintf(f, "gl-host:   ... and %lu more\n", s_proc_missing - shown);
    }
    if (f != stderr)
        fclose(f);
}
