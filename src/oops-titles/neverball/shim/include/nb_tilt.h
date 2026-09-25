/*
 * The DualSense tilt backend's own controls, for the screen that offers it.
 *
 * `share/tilt.h` is upstream's interface and says nothing about enabling or disabling - on a
 * Wiimote the sensor is either attached or it is not. Here it is a preference: the pad is always
 * there, and tilting it is one of two ways to play. So the extra three entry points live in this
 * header rather than in upstream's, and only this port's own code calls them.
 */
#ifndef OOPS_NEVERBALL_TILT_H
#define OOPS_NEVERBALL_TILT_H

/* Whether a pad with a usable IMU answered at start-up. Zero means the option should not be
   offered at all, rather than offered and silently doing nothing. */
int nb_tilt_available(void);

/* Whether motion tilt is currently driving the floor. **Off unless the player turns it on** -
   an accelerometer that hijacks the sticks on first launch is a port that looks broken. */
int nb_tilt_enabled(void);

/* Turn it on or off. Re-centres on the way on, so whatever way the pad is being held becomes
   level, and writes the choice to `/app0/oops-tilt` so it survives the launch.

   **That file, not a `tilt=` line in `/app0/oops-input`.** The config file also holds the gain,
   the deadzone and the axis knobs, so rewriting it to change one value would mean parsing and
   re-emitting the rest, with a truncated file as the price of getting it wrong. `tilt_init`
   reads the state file first and falls back to the config's `tilt=` key, so a preset still
   works and the remembered choice only ever overrides it. */
void nb_tilt_set_enabled(int on);

/* Re-centre without changing the mode: the current pad attitude becomes level. Worth a button
   because a player shifts position on a sofa and the floor should not drift with them. */
void nb_tilt_recentre(void);

#endif
