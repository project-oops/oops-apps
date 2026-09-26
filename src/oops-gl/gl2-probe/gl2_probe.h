/*
 * gl2-probe: the shared check suite for OpenGL 2.0.
 *
 * One suite runs against the software reference and against the console, so a check
 * that passes on one and fails on the other is a hardware-path fault. oops-gl runs GLSL
 * on the CPU on the host and compiles the fragment stage to gfx1030 on the console.
 * `gl2_probe_selftest.c` is the host's runner and `gl2_probe_main.c` the console's;
 * neither owns a check.
 */

#ifndef GL2_PROBE_H
#define GL2_PROBE_H

#include <stdint.h>

typedef struct {
    const char *name;
    int (*fn)(void);
} gl2_probe_case_t;

typedef struct {
    const char *name;
    int passed;
} gl2_probe_result_t;

/* The most checks a run records - what a caller's result array holds. gl2_probe.c
 * refuses to build with more checks than this, so no check goes unrun and uncounted. */
#define GL2_PROBE_MAX_CASES 128

int gl2_probe_case_count(void);
const char *gl2_probe_case_name(int i);
/* Runs every check, writing up to `max` results. Returns how many ran, or -1 if a
 * context could not be made at all. */
int gl2_probe_run(gl2_probe_result_t *out, int max);

/*
 * Called before each check and again after it, so a run that never finishes still says
 * how far it got. `verdict` is -1 on the way in and the check's result on the way out.
 * Left NULL by the host runner, which prints its own table.
 */
extern void (*gl2_probe_trace)(const char *name, int verdict);

/*
 * Called only for a check that failed. `centre` is the colour at the centre of the
 * probe region; one pixel, because a read costs a synchronisation. `err` is the first
 * GL error the check raised, which tells a refused draw from one that ran and missed.
 * `drawn` counts the region's pixels that are not the reset colour: zero means nothing
 * reached the framebuffer. `left` and `right` flank the centre on opposite sides of the
 * diagonal seam `attrib_rect` splits its quad along, so one drawn and one not means a
 * missing triangle.
 */
extern void (*gl2_probe_saw)(const char *name, uint32_t centre, unsigned int err,
                             int drawn, uint32_t left, uint32_t right);

#endif /* GL2_PROBE_H */
