/*
 * gl2-probe: the shared check suite for OpenGL 2.0.
 *
 * The same shape as gl1-probe, for the same reason: one suite that runs against the software
 * reference and against the console, so a check that passes on one and fails on the other is a
 * hardware-path bug and nothing else in this repository can see one.
 *
 * **Only the reference half exists today.** oops-gl runs GLSL on the CPU - the entry points,
 * the linker and the interpreter are all there - and the console draw path refuses a draw with
 * a program bound rather than running the fixed-function instruments in its place. So this
 * suite has one runner and no payload, and the day there is a console back end it gains a
 * `gl2_probe_main.c` beside gl1-probe's and the same checks run on hardware unchanged. The
 * hooks below are declared now for that reason: the payload's reporting is the part gl1-probe
 * had to learn the hard way, and there is no sense learning it twice.
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
 * that check left it.
 *
 * A bare `FAIL` says a check disagreed with the specification and nothing about how. One pixel,
 * and only on a failure, because reading one costs a synchronisation - which on console hardware
 * is about half a second. It is not the whole of any check's evidence; it is the one value cheap
 * enough to take from every check without being asked for.
 */
extern void (*gl2_probe_saw)(const char *name, uint32_t centre);

#endif /* GL2_PROBE_H */
