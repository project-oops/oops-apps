/*
 * The DualSense tilt backend's own controls, for the screen that offers it.
 *
 * `share/tilt.h` has no enable or disable: a Wiimote is attached or not. Here the pad
 * is always present and tilt is a preference, so these entry points live in this
 * header and only this port's code calls them.
 */
#ifndef OOPS_NEVERBALL_TILT_H
#define OOPS_NEVERBALL_TILT_H

/* Whether a pad with a usable IMU answered at start-up. Zero means the option is not
   offered. */
int nb_tilt_available(void);

/* Whether motion tilt drives the floor. Off unless the player turns it on. */
int nb_tilt_enabled(void);

/* Turn it on or off. Turning it on re-centres, so the current pad attitude becomes
   level, and the choice is written to `/app0/oops-tilt`.

   A separate state file rather than a `tilt=` line in `/app0/oops-input`, so the config
   (gain, deadzone, axes) is never rewritten. `tilt_init` reads the state file first and
   falls back to the config's `tilt=` key. */
void nb_tilt_set_enabled(int on);

/* Re-centre without changing the mode: the current pad attitude becomes level. */
void nb_tilt_recentre(void);

#endif
