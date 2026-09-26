/*
 * gl1-probe: the shared check suite.
 *
 * The same suite runs on the host software rasteriser and on the console. A check that
 * passes on one and fails on the other is a hardware-path fault.
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

/* The most checks a run records - what a caller's result array holds. gl1_probe.c
 * refuses to build with more checks than this, so no check goes unrun and uncounted. */
#define GL1_PROBE_MAX_CASES 128

int gl1_probe_case_count(void);
const char *gl1_probe_case_name(int i);
/* Runs every check, writing up to `max` results. Returns how many ran, or -1 if a
 * context could not be made at all. */
int gl1_probe_run(gl1_probe_result_t *out, int max);

/*
 * With `gl1_probe_keep_context` set, `gl1_probe_run` leaves the context and display up,
 * and `gl1_probe_test_card` paints a card on them: flat bands in the primaries and mid
 * grey plus a black-to-white ramp, each also read back through `gl1_probe_saw`. A band
 * right in the log and wrong on the panel is a fault after the framebuffer. The payload
 * uses both; the host self-test uses neither.
 */
extern int gl1_probe_keep_context;
void gl1_probe_test_card(void);

/*
 * Called before each check and again after it, so a run that never finishes still says
 * how far it got. `verdict` is -1 on the way in and the check's result on the way out.
 * Left NULL by the host self-test, which prints its own table.
 */
extern void (*gl1_probe_trace)(const char *name, int verdict);

/*
 * Called only for a check that failed, with the colour at the centre of the probe
 * region as that check left it, so "the draw never landed" and "it landed in the wrong
 * colour" read differently. One pixel, because a read costs a synchronisation (see
 * `scan_frame`). Left NULL by the host self-test, which prints its own table.
 */
extern void (*gl1_probe_saw)(const char *name, uint32_t centre);

#endif /* GL1_PROBE_H */
