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

int gl1_probe_case_count(void);
const char *gl1_probe_case_name(int i);
/* Runs every check, writing up to `max` results. Returns how many ran, or -1 if a context
 * could not be made at all. */
int gl1_probe_run(gl1_probe_result_t *out, int max);

#endif /* GL1_PROBE_H */
