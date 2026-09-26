/*
 * gl-replay - run a captured GL call stream against the software reference, on a build
 * machine.
 *
 * `oops_gl_capture_frame` writes one of a title's frames to a file on the console. This
 * reads it back and draws it with the reference rasteriser, so the image is what the
 * frame should look like and its difference from a screenshot is the defect. A capture
 * covers call combinations a conformance suite does not enumerate.
 *
 *   make check                 the round trip, in process, as a build gate
 *   ./build/gl-replay_selftest FRAME.oglcap [OUT.ppm]
 *
 * The output is a binary PPM because it needs no library and every image tool reads it.
 */

#include <oops/display.h>
#include <GL/gl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A plain-buffer host display standing in for oops-sdk's hardware one. Full HD, so the
 * row stride is the one a captured frame was drawn against. */
static uint32_t s_host_fb[1920 * 1080];
static int s_host_disp_dummy = 1;
static unsigned int s_host_w = 1920;
static unsigned int s_host_h = 1080;

oops_display_t *oops_display_open(oops_display_backend_t backend, unsigned int width,
                                  unsigned int height) {
    (void)backend;
    s_host_w = width ? width : 1920;
    s_host_h = height ? height : 1080;
    if ((size_t)s_host_w * (size_t)s_host_h >
        sizeof(s_host_fb) / sizeof(s_host_fb[0])) {
        return (oops_display_t *)0;
    }
    memset(s_host_fb, 0, (size_t)s_host_w * (size_t)s_host_h * sizeof(s_host_fb[0]));
    return (oops_display_t *)&s_host_disp_dummy;
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
    return s_host_fb;
}
unsigned int oops_display_get_width(const oops_display_t *disp) {
    (void)disp;
    return s_host_w;
}
unsigned int oops_display_get_height(const oops_display_t *disp) {
    (void)disp;
    return s_host_h;
}

/* Rows top to bottom, which is the opposite of GL's order, so the read is flipped back.
 */
static int write_ppm(const char *path, unsigned w, unsigned h) {
    unsigned char *rgba = (unsigned char *)malloc((size_t)w * h * 4u);
    if (!rgba)
        return 0;
    glReadPixels(0, 0, (GLsizei)w, (GLsizei)h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    FILE *f = fopen(path, "wb");
    if (!f) {
        free(rgba);
        return 0;
    }
    fprintf(f, "P6\n%u %u\n255\n", w, h);
    for (unsigned y = 0; y < h; y++) {
        const unsigned char *row = rgba + (size_t)(h - 1u - y) * w * 4u;
        for (unsigned x = 0; x < w; x++) {
            fputc(row[x * 4u + 0], f);
            fputc(row[x * 4u + 1], f);
            fputc(row[x * 4u + 2], f);
        }
    }
    fclose(f);
    free(rgba);
    return 1;
}

/* The build gate: a small drawing captured in process and replayed into a wiped buffer
 * gives the same pixels. */
static int selftest(void) {
    enum { W = 64, H = 64 };
    oops_display_t *disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, W, H);
    void *gc = glContextCreate(disp);
    if (!gc) {
        printf("gl-replay selftest: FAIL (no GL context)\n");
        return 1;
    }
    glViewport(0, 0, W, H);

    oops_gl_capture_begin();
    glClearColor(0.2f, 0.4f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(1.0f, 0.5f, 0.0f);
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.8f, -0.8f);
    glVertex2f(0.8f, -0.8f);
    glVertex2f(0.0f, 0.8f);
    glEnd();
    oops_gl_capture_end();

    static unsigned char truth[W * H * 4];
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, truth);

    size_t bytes = 0;
    unsigned calls = 0;
    const void *stream = oops_gl_capture_data(&bytes, &calls);
    if (!stream || !calls) {
        printf("gl-replay selftest: FAIL (nothing captured)\n");
        return 1;
    }

    /* Magenta: neither the clear colour nor the triangle, so a replay that drew nothing
     * shows. */
    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    const unsigned ran = oops_gl_capture_replay(stream, bytes);

    static unsigned char again[W * H * 4];
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, again);
    const int same = memcmp(truth, again, sizeof(truth)) == 0;
    printf("  captured %u calls in %zu bytes, replayed %u\n", calls, bytes, ran);
    printf("gl-replay selftest: %s (host software rasteriser)\n",
           (same && ran == calls) ? "pass" : "FAIL");
    glContextDestroy(gc);
    oops_display_close(disp);
    return (same && ran == calls) ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc < 2)
        return selftest();

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "gl-replay: cannot open %s\n", argv[1]);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) {
        fprintf(stderr, "gl-replay: %s is empty\n", argv[1]);
        fclose(f);
        return 1;
    }
    void *buf = malloc((size_t)n);
    if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fprintf(stderr, "gl-replay: cannot read %s\n", argv[1]);
        fclose(f);
        free(buf);
        return 1;
    }
    fclose(f);

    /* The capture carries no window size - it is a call stream, not a frame buffer - so
       the replay is drawn at the size the console used, which is what its viewport
       calls expect. */
    oops_display_t *disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, 1920, 1080);
    void *gc = glContextCreate(disp);
    if (!gc) {
        fprintf(stderr, "gl-replay: no GL context\n");
        free(buf);
        return 1;
    }

    const unsigned ran = oops_gl_capture_replay(buf, (size_t)n);
    if (ran == 0u) {
        fprintf(stderr, "gl-replay: %s is not a capture this version reads\n", argv[1]);
        free(buf);
        return 1;
    }
    printf("gl-replay: %u calls from %s\n", ran, argv[1]);

    const char *out = (argc > 2) ? argv[2] : "replay.ppm";
    if (!write_ppm(out, s_host_w, s_host_h)) {
        fprintf(stderr, "gl-replay: cannot write %s\n", out);
        free(buf);
        return 1;
    }
    printf("gl-replay: wrote %s (%ux%u)\n", out, s_host_w, s_host_h);
    free(buf);
    return 0;
}
