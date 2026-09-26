/*
 * Pad rumble as a game wants it: a hit, not a level.
 *
 * oops_input_set_rumble is a state - the motors run at what they were last told until
 * told otherwise. This file turns a hit into on, decay and off, shared by every port.
 *
 * A decay thread does the turning off, so no title needs a frame-loop patch and a
 * missed frame cannot leave the pad running.
 *
 * oops_haptics_bump is safe to call from anywhere, including a physics step at a
 * different rate from the frame: it only raises a target the thread is lowering.
 */
#ifndef OOPS_APPS_HAPTICS_H
#define OOPS_APPS_HAPTICS_H

/* Start the decay thread. Safe to call twice; the second call does nothing. Returns 0
   on success, non-zero if no thread could be started - in which case every call below
   is a no-op rather than a pad left running. */
int oops_haptics_init(void);

/* Stop the thread and silence the motors. A title that ends without this leaves the pad
   buzzing. */
void oops_haptics_quit(void);

/*
 * One hit. `strength` is 0..1 and is not a duration - the decay is fixed, so a light
 * tap and a hard one differ in how hard the motors start rather than how long they run.
 *
 * Raises the current level rather than replacing it: a run of small bounces should not
 * cut off the big one that started them.
 */
void oops_haptics_bump(float strength);

/* Silence now, without stopping the thread: for a pause, a menu, or a run's end. */
void oops_haptics_silence(void);

/*
 * Whether rumble is wanted at all. Read from `/app0/oops-input` (`rumble=on|off`,
 * default on) by `oops_haptics_init`, and settable at runtime for a title that offers
 * it in its own options.
 */
int oops_haptics_enabled(void);
void oops_haptics_set_enabled(int on);

#endif
