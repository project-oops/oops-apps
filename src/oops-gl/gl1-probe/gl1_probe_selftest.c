/*
 * Host self-test for gl1-probe.
 *
 * Runs the whole check suite against the software rasteriser and prints one line per feature.
 * The same suite runs on the console through `gl1_probe_main.c`; comparing the two outputs is
 * the point of the app.
 */

#include "gl1_probe.h"

#include <oops/display.h>
#include <stdio.h>
#include <string.h>

/* A host display is a plain buffer. oops-sdk's real one talks to the AGC backend, which is not
 * here - so these stubs stand in, exactly as gl-cube's self-test does. The software rasteriser
 * writes into this and the checks read it back, which is the whole mechanism. */
/* Sized for the display the probe asks for, which is a full 1920x1080 - the probe works in a
 * small corner of it, but the buffer has to be the whole thing or the row stride is a lie. */
static uint32_t s_host_fb[1920 * 1080];
static int s_host_disp_dummy = 1;
static unsigned int s_host_w = 1920;
static unsigned int s_host_h = 1080;

oops_display_t *
oops_display_open(oops_display_backend_t backend, unsigned int width, unsigned int height) {
    (void)backend;
    s_host_w = width ? width : 1920;
    s_host_h = height ? height : 1080;
    if ((size_t)s_host_w * (size_t)s_host_h > sizeof(s_host_fb) / sizeof(s_host_fb[0])) {
        return (oops_display_t *)0;
    }
    memset(s_host_fb, 0, (size_t)s_host_w * (size_t)s_host_h * sizeof(s_host_fb[0]));
    return (oops_display_t *)&s_host_disp_dummy;
}

/* The real one answers 0 for a display that was returned but never opened, which is the case
 * the probe missed on hardware. Stubbed true here because this one did open. */
int oops_display_is_ready(const oops_display_t *disp) { (void)disp; return 1; }

void oops_display_close(oops_display_t *disp) { (void)disp; }
int oops_display_flip(oops_display_t *disp) { (void)disp; return 0; }
uint32_t *oops_display_get_framebuffer(oops_display_t *disp) { (void)disp; return s_host_fb; }
unsigned int oops_display_get_width(const oops_display_t *disp) { (void)disp; return s_host_w; }
unsigned int oops_display_get_height(const oops_display_t *disp) { (void)disp; return s_host_h; }

int main(void) {
    gl1_probe_result_t results[64];
    const int ran = gl1_probe_run(results, (int)(sizeof(results) / sizeof(results[0])));

    if (ran < 0) {
        printf("gl1-probe selftest: FAIL (no GL context)\n");
        return 1;
    }

    int passed = 0;
    for (int i = 0; i < ran; i++) {
        printf("  %-16s %s\n", results[i].name, results[i].passed ? "pass" : "FAIL");
        if (results[i].passed) passed++;
    }

    printf("gl1-probe selftest: %d/%d passed (host software rasteriser)\n", passed, ran);
    /* **A failing check fails the build.** The probe exists to be believed, so a red line in
     * its output cannot be something the build walks past. */
    return passed == ran ? 0 : 1;
}
