/*
 * gl2-probe: the shared check suite for OpenGL 2.0.
 *
 * The same shape as gl1-probe, for the same reason: one suite that runs against the software
 * reference and against the console, so a check that passes on one and fails on the other is a
 * hardware-path bug and nothing else in this repository can see one.
 *
 * **Both halves run.** oops-gl runs GLSL on the CPU - the entry points, the linker and the
 * interpreter - and compiles the fragment stage to gfx1030 for the console, so the same checks
 * measure a software rasteriser and a real pipeline. `gl2_probe_selftest.c` is the host's
 * runner and `gl2_probe_main.c` the console's; neither owns a check.
 *
 * The hooks below were declared before there was a payload to use them, because the payload's
 * reporting is the part gl1-probe had to learn the hard way - a hang leaves behind only what
 * was printed before it - and there was no sense learning it twice.
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

/* The most checks a run records - what a caller's result array holds. **gl2_probe.c refuses to
 * build with more checks than this**, because gl1-probe once grew past its callers' array and
 * the last check in the table was run by neither and counted by neither: the totals read 64/64
 * with 65 checks in the file. */
#define GL2_PROBE_MAX_CASES 128

int gl2_probe_case_count(void);
const char *gl2_probe_case_name(int i);
/* Runs every check, writing up to `max` results. Returns how many ran, or -1 if a context could
 * not be made at all. */
int gl2_probe_run(gl2_probe_result_t *out, int max);

/*
 * **Called before each check and again after it**, so a run that never finishes still says how
 * far it got.
 *
 * gl1-probe's first console run hung after nine frames and reported nothing at all, because
 * every result was written to the caller's array and printed only once the run returned - which
 * it never did. A hang is exactly the case a hardware run exists to find, and it was the one
 * case the reporting could not describe.
 *
 * `verdict` is -1 on the way in and the check's own result on the way out. Left NULL by the host
 * runner, which prints its own table and cannot hang the machine.
 */
extern void (*gl2_probe_trace)(const char *name, int verdict);

/*
 * **Called only for a check that failed**, with the colour at the centre of the probe region as
 * that check left it and the first GL error it raised.
 *
 * A bare `FAIL` says a check disagreed with the specification and nothing about how. One pixel,
 * and only on a failure, because reading one costs a synchronisation - which on console hardware
 * is about half a second. It is not the whole of any check's evidence; it is the one value cheap
 * enough to take from every check without being asked for.
 *
 * **The error is what tells a refused draw from a wrong one.** gl2-probe's first hardware run
 * had thirteen checks leave the reset colour at the centre, which reads identically whether the
 * draw was refused before it started or ran and missed - and the run loop already clears errors
 * between checks, so the first one this check raised was there to be reported and was not.
 * `GL_NO_ERROR` with the reset colour means the draw happened; `GL_INVALID_OPERATION` means it
 * never did.
 *
 * **And `drawn` is how many of the region's pixels are not the reset colour**, which separates
 * the two ways a centre pixel can read as background. Zero means nothing reached the
 * framebuffer. A large number means the draw landed and the *sample point* is what missed it -
 * which is a live possibility here, because `attrib_rect` builds its quad as two triangles
 * sharing the diagonal from one corner to the other, and a centred rect puts that seam straight
 * through the pixel at the middle of the region.
 *
 * **`left` and `right` flank the centre on the same row**, which says what shape the missing
 * part is. The quad's seam runs corner to corner, so those two pixels fall on opposite sides of
 * it: one drawn and one not means a triangle is missing, and both the same means the gap is
 * something else and the diagonal was a red herring.
 */
extern void (*gl2_probe_saw)(const char *name, uint32_t centre, unsigned int err, int drawn,
                             uint32_t left, uint32_t right);

#endif /* GL2_PROBE_H */
