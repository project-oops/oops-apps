/*
 * gl2-probe: the host runner.
 *
 * Runs the check suite against oops-gl's software reference and prints a row per check.
 *
 * **This is one half of a pair, and the other half is `gl2_probe_main.c`.** gl1-probe's value is
 * that the same suite runs here and on a console, so a check that passes in one place and fails
 * in the other is a hardware-path bug - and that is what this suite is for too. The console path
 * for a GL 2.0 program was measured through 2026-09-21: obSCEne's `REQ-...-4e77` ran shaders
 * this compiler generated and they retired with their interpolated parameters intact,
 * `REQ-...-6c0d` did the same for a uniform block, and `REQ-...-2a45` for a sample under
 * whole-quad mode. gl2-cube then put one on hardware through oops-gl's own draw path rather
 * than a fixture.
 *
 * This runner prints a table and returns; the payload's cannot assume it will get to return,
 * which is why the two report differently and why the suite itself owns neither.
 *
 * It prints every row rather than only the failures, because a suite that prints nothing when
 * it passes gives no way to tell "all green" from "did not run".
 */

#include "gl2_probe.h"

#include <oops/display.h>
#include <oops/memory.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define FB_W 1920
#define FB_H 1080

/* The display the checks draw into. Full size, because the probe region is a corner of a real
 * display rather than a display of its own - see gl2_probe.c. */
static uint32_t *s_host_fb;
static int s_host_disp_dummy = 1;
static unsigned int s_host_w = FB_W;
static unsigned int s_host_h = FB_H;

oops_display_t *
oops_display_open(oops_display_backend_t backend, unsigned int width, unsigned int height) {
    (void)backend;
    s_host_w = width ? width : FB_W;
    s_host_h = height ? height : FB_H;
    if (!s_host_fb) {
        s_host_fb = (uint32_t *)calloc((size_t)s_host_w * (size_t)s_host_h, sizeof(uint32_t));
        if (!s_host_fb) return (oops_display_t *)0;
    }
    return (oops_display_t *)&s_host_disp_dummy;
}
void oops_display_close(oops_display_t *disp) { (void)disp; }
int oops_display_flip(oops_display_t *disp) { (void)disp; return 0; }
uint32_t *oops_display_get_framebuffer(oops_display_t *disp) { (void)disp; return s_host_fb; }
unsigned int oops_display_get_width(const oops_display_t *d) { (void)d; return s_host_w; }
unsigned int oops_display_get_height(const oops_display_t *d) { (void)d; return s_host_h; }
int oops_display_is_gpu_accelerated(const oops_display_t *d) { (void)d; return 0; }
int oops_display_is_ready(const oops_display_t *d) { return d != (const oops_display_t *)0; }

/* The SDK's allocator is direct memory and has nothing to talk to on a build machine. */
void *oops_mem_alloc(size_t size, size_t alignment, oops_mem_type_t type) {
    (void)alignment;
    (void)type;
    return malloc(size);
}
void oops_mem_free(void *p) { free(p); }

int main(void) {
    gl2_probe_result_t results[GL2_PROBE_MAX_CASES];
    const int n = gl2_probe_run(results, GL2_PROBE_MAX_CASES);
    if (n < 0) {
        printf("gl2-probe selftest: FAILED (no context; nothing was measured)\n");
        return 1;
    }

    int passed = 0;
    for (int i = 0; i < n; i++) {
        printf("  %-24s %s\n", results[i].name, results[i].passed ? "pass" : "FAIL");
        if (results[i].passed) passed++;
    }
    printf("gl2-probe selftest: %d/%d passed (host software reference; the console path runs - "
           "obSCEne REQ-...-4e77 - and samples since REQ-...-2a45)\n",
           passed, n);
    free(s_host_fb);
    s_host_fb = (uint32_t *)0;
    return passed == n ? 0 : 1;
}
