#ifndef OOPS_APPS_PAD_VIZ_H
#define OOPS_APPS_PAD_VIZ_H

#include "oops/draw.h"
#include "oops/input.h"

/*
 * pad-viz: a live controller diagram.
 *
 * Draws the DualSense/DualShock state as a stylised pad - buttons that light when pressed,
 * sticks whose thumbs move, analog trigger bars, the touch-pad with its contacts, and a tilt
 * box driven by the accelerometer. It is the on-screen counterpart to obSCEne's input probe:
 * a person looks and judges, there is no verdict.
 *
 * Render is a pure function of the state, so the whole diagram draws into a plain buffer and
 * is checked on a host - the seam that makes it testable off the console.
 */

/* What the visualiser draws, gathered so render stays a pure function of it. */
typedef struct padviz_state {
    oops_pad_state_t pad;  /* the controller sample to draw (the newest of a batched read) */
    int sample_count;      /* how many samples the last batched read returned - shows the
                            * low-latency path is delivering more than one record a frame */
} padviz_state_t;

/* Draw the controller diagram into `surf`. Returns 0, or -1 on a bad argument. */
int padviz_render(oops_surface_t *surf, const padviz_state_t *state);

#endif /* OOPS_APPS_PAD_VIZ_H */
