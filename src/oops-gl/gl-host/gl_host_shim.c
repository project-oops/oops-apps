/*
 * gl-host - run an unmodified GL 1.x program against oops-gl, on a desktop.
 *
 * **The point is the next port, not this one.** Every question asked of oops-gl so far has cost
 * a console run and a photograph of a television: build, push, launch, look, guess again. That
 * is affordable once and not affordable for a shelf of titles. A program linked against GL calls
 * the same entry points wherever it runs, so putting this library in front of the system's one
 * makes those calls land in oops-gl instead - on a machine with a debugger, a filesystem and no
 * queue for the hardware.
 *
 * What it gives, for any title, before a line of porting work is done:
 *
 *   - the frame oops-gl draws from that title's own calls, beside the frame the title's normal
 *     driver draws, which is the comparison that has been missing
 *   - every error oops-gl raised while drawing it, which is the list of what that title needs
 *     and this GL has not got
 *
 * Driven by the environment rather than by arguments, because the program being measured owns
 * its own command line:
 *
 *   OOPS_GL_HOST_W, OOPS_GL_HOST_H   the display to open (default 800x600)
 *   OOPS_GL_HOST_DUMP                path prefix for frames; "shot" writes shot-0001.ppm
 *   OOPS_GL_HOST_EVERY              dump one frame in N (default 1)
 *   OOPS_GL_HOST_MAX                stop dumping after this many frames (default 8)
 *   OOPS_GL_HOST_REPORT             where to write the readiness report (default stderr)
 *
 * Used as:
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
 * The display.
 *
 * oops-sdk's real one talks to the console's display engine, which is not here - so this stands
 * in, exactly as gl1-probe's and gl-cube's self-tests do. The software rasteriser writes into
 * this buffer and the dump reads it back out, which is the whole mechanism.
 * -------------------------------------------------------------------------- */

static uint32_t *s_fb;
static int s_disp_dummy = 1;
static unsigned int s_w = 800;
static unsigned int s_h = 600;

oops_display_t *
oops_display_open(oops_display_backend_t backend, unsigned int width, unsigned int height) {
    (void)backend;
    s_w = width ? width : 800;
    s_h = height ? height : 600;
    free(s_fb);
    s_fb = (uint32_t *)calloc((size_t)s_w * (size_t)s_h, sizeof(uint32_t));
    if (!s_fb) return (oops_display_t *)0;
    return (oops_display_t *)&s_disp_dummy;
}

int oops_display_is_ready(const oops_display_t *disp) { (void)disp; return 1; }
void oops_display_close(oops_display_t *disp) { (void)disp; }
int oops_display_flip(oops_display_t *disp) { (void)disp; return 0; }
uint32_t *oops_display_get_framebuffer(oops_display_t *disp) { (void)disp; return s_fb; }
unsigned int oops_display_get_width(const oops_display_t *disp) { (void)disp; return s_w; }
unsigned int oops_display_get_height(const oops_display_t *disp) { (void)disp; return s_h; }

/* --------------------------------------------------------------------------
 * The context, and what happened while it was current.
 * -------------------------------------------------------------------------- */

static oops_display_t *s_disp;
static void *s_ctx;
static unsigned long s_frames;      /* swaps seen */
static unsigned long s_dumped;      /* frames written */
static unsigned long s_errors;      /* GL errors caught at a swap boundary */
static GLenum s_first_error;        /* and the first of them, which is usually the cause */

static unsigned long env_num(const char *name, unsigned long dflt) {
    const char *v = getenv(name);
    if (!v || !*v) return dflt;
    char *end = (char *)0;
    const unsigned long n = strtoul(v, &end, 10);
    return (end && *end == '\0') ? n : dflt;
}

/* **A constructor, not lazy initialisation on the first GL call.** There are 667 entry points
 * and any of them can be the first; a check at the top of each would mean touching all of them,
 * and a check in only some would mean the one that got missed draws into no context at all and
 * silently does nothing. The loader runs this before the program's own main, which is earlier
 * than any GL call can be. */
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
}

/* One frame, as a binary PPM - the format costs fifteen lines to write and every image tool
 * reads it, which is the right trade for a debugging artifact nobody keeps. */
static void dump_frame(unsigned long n) {
    const char *prefix = getenv("OOPS_GL_HOST_DUMP");
    if (!prefix || !s_fb) return;

    char path[512];
    snprintf(path, sizeof(path), "%s-%04lu.ppm", prefix, n);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%u %u\n255\n", s_w, s_h);

    /* Read through GL rather than off the buffer directly: glReadPixels is what a caller would
     * use, it applies the same origin convention every other reader here assumes, and going
     * through it means this reports the pixels oops-gl says it drew rather than the ones this
     * file guessed the layout of. PPM is top-down and GL's origin is bottom-left, so the rows
     * come back in the order this has to reverse. */
    unsigned char *rgba = (unsigned char *)malloc((size_t)s_w * (size_t)s_h * 4u);
    if (!rgba) { fclose(f); return; }
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
 * A GL program does not tell its driver when a frame ends - it tells its *windowing* library,
 * and SDL is what every title here uses. Interposing the swap is therefore how this finds out,
 * and forwarding it afterwards is what keeps the program running normally: it still has its own
 * real context from the system driver, still presents to its own window, and still behaves like
 * a program rather than like a program being measured.
 * -------------------------------------------------------------------------- */

void SDL_GL_SwapWindow(void *window) {
    s_frames++;

    /* Asked at the boundary rather than per call: glGetError clears as it reports, so a caller
     * checking its own errors would hide them from this and this would hide them from the
     * caller. At a swap the program has had its chance and whatever is left is unexamined - and
     * an unexamined error is exactly the kind a port never notices it is relying on. */
    for (;;) {
        const GLenum e = glGetError();
        if (e == GL_NO_ERROR) break;
        if (!s_errors) s_first_error = e;
        s_errors++;
        if (s_errors > 100000u) break;
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
    if (real_swap) real_swap(window);
}

/* --------------------------------------------------------------------------
 * How a title finds the GL it is going to use.
 *
 * **Half a GL is worse than none.** A GL 1.x program links the entry points that existed in
 * 1.1 and asks for everything newer by name, because that is how extensions have always been
 * reached - Neverball takes `glBindBuffer` and the rest of the buffer-object family this way
 * (share/glext.c). Those requests go to SDL, which answers out of the system driver, so without
 * this a title's buffer calls configure the *system's* GL while its draw calls arrive here. The
 * two then disagree about everything: this GL is told to draw from an array whose buffer it
 * never saw bound, reads the offset as a client pointer, and follows it into nothing.
 *
 * That cost an afternoon and looked exactly like a bug in the array path, which is worth saying
 * plainly - the first crash this harness produced was the harness's fault, and a harness that
 * manufactures faults is worse than no harness at all.
 *
 * The names are counted on the way past, because what a title asks for *is* the list of what it
 * needs. A name this GL cannot answer is a porting task, discovered before anyone has built
 * anything for the console.
 * -------------------------------------------------------------------------- */

#define GL_HOST_MAX_MISSING 64
static unsigned long s_proc_ours;
static unsigned long s_proc_missing;
static const char *s_missing[GL_HOST_MAX_MISSING];
static char s_missing_store[GL_HOST_MAX_MISSING][64];

void *SDL_GL_GetProcAddress(const char *name) {
    if (name) {
        /* Ours first. The loader searches the executable, then the preloaded libraries, then
         * everything else - and the program under test does not define GL entry points, so a
         * name this library exports resolves here and not in the system driver. */
        void *p = dlsym(RTLD_DEFAULT, name);
        if (p) {
            s_proc_ours++;
            return p;
        }
        if (s_proc_missing < GL_HOST_MAX_MISSING) {
            size_t n = strlen(name);
            if (n > sizeof(s_missing_store[0]) - 1u) n = sizeof(s_missing_store[0]) - 1u;
            memcpy(s_missing_store[s_proc_missing], name, n);
            s_missing_store[s_proc_missing][n] = '\0';
            s_missing[s_proc_missing] = s_missing_store[s_proc_missing];
        }
        s_proc_missing++;
    }
    /* Not ours: let SDL answer, so the program still gets whatever it was going to get and
     * behaves as it normally would rather than crashing inside the measurement. */
    static void *(*real_gpa)(const char *);
    if (!real_gpa) {
        *(void **)(&real_gpa) = dlsym(RTLD_NEXT, "SDL_GL_GetProcAddress");
    }
    return real_gpa ? real_gpa(name) : (void *)0;
}

/* --------------------------------------------------------------------------
 * The report.
 *
 * What a title asked of this GL and did not get. For a port that has not started yet this is
 * the answer to "what does it need", which is otherwise discovered one hardware run at a time.
 * -------------------------------------------------------------------------- */

__attribute__((destructor)) static void gl_host_report(void) {
    const char *path = getenv("OOPS_GL_HOST_REPORT");
    FILE *f = stderr;
    if (path && *path) {
        FILE *o = fopen(path, "w");
        if (o) f = o;
    }
    fprintf(f, "gl-host: %lu frames, %lu dumped, %lu GL errors", s_frames, s_dumped, s_errors);
    if (s_errors) fprintf(f, ", first 0x%04x", (unsigned)s_first_error);
    fprintf(f, "\ngl-host: %lu entry points served, %lu not ours\n", s_proc_ours, s_proc_missing);
    /* **The porting task list.** Every name here is something the title looked for and this GL
     * did not have, which is the question a new port opens with and has until now been answered
     * by building it and finding out. */
    const unsigned long shown =
        (s_proc_missing < GL_HOST_MAX_MISSING) ? s_proc_missing : GL_HOST_MAX_MISSING;
    for (unsigned long i = 0; i < shown; i++) {
        if (s_missing[i]) fprintf(f, "gl-host:   absent: %s\n", s_missing[i]);
    }
    if (s_proc_missing > shown) {
        fprintf(f, "gl-host:   ... and %lu more\n", s_proc_missing - shown);
    }
    if (f != stderr) fclose(f);
}
