/*
 * gl1-probe: the shared check suite.
 *
 * The same suite runs on the host software rasteriser and on the console. That is the whole
 * design: a check that passes on one and fails on the other is a hardware-path bug, and
 * nothing else in this repository can see one.
 */

#ifndef GL1_PROBE_H
#define GL1_PROBE_H

#include <stdint.h>

typedef struct {
    const char *name;
    int (*fn)(void);
} gl1_probe_case_t;

typedef struct {
    const char *name;
    int passed;
} gl1_probe_result_t;

/* The most checks a run records - what a caller's result array holds. **gl1_probe.c refuses to
 * build with more checks than this.** Both callers held 64 until the suite grew to 65 on
 * 2026-09-19, and the 65th - the last in the table - was run by neither and counted by neither:
 * the totals read 64/64. */
#define GL1_PROBE_MAX_CASES 128

int gl1_probe_case_count(void);
const char *gl1_probe_case_name(int i);
/* Runs every check, writing up to `max` results. Returns how many ran, or -1 if a context
 * could not be made at all. */
int gl1_probe_run(gl1_probe_result_t *out, int max);

/*
 * **Called before each check and again after it**, so a run that never finishes still says how
 * far it got.
 *
 * The first console run of this suite, on 2026-09-20, hung after nine frames and reported
 * nothing at all - because every result was written to the caller's array and printed only once
 * `gl1_probe_run` returned, which it never did. A hang is exactly the case a hardware run exists
 * to find, and it was the one case the reporting could not describe.
 *
 * `verdict` is -1 on the way in and the check's own result on the way out. Left NULL by the host
 * self-test, which prints its own table and cannot hang the machine.
 */
extern void (*gl1_probe_trace)(const char *name, int verdict);

/*
 * **Called only for a check that failed**, with the colour at the centre of the probe region as
 * that check left it.
 *
 * A bare `FAIL` says a check disagreed with the specification and nothing about how. The first
 * full hardware run, on 2026-09-20, produced seven of them and not one value between them - so
 * the failures could be grouped by what they call but not told apart by what they saw, and the
 * difference between "the draw never landed" and "it landed in the wrong colour" is most of the
 * diagnosis. This is the same rule the obSCEne bus runs on: a result is its rows, not its
 * verdict.
 *
 * One pixel, and only on a failure, because it costs a synchronisation to read - which on this
 * hardware is half a second (see `scan_frame`). It is not the whole of any check's evidence; it
 * is the one value cheap enough to take from every check without being asked for.
 *
 * Left NULL by the host self-test, which prints its own table.
 */
extern void (*gl1_probe_saw)(const char *name, uint32_t centre);

#endif /* GL1_PROBE_H */
