/*
 * Pad rumble as a game wants it: a hit, not a level.
 *
 * `oops_input_set_rumble` is a *state* - the motors run at what they were last told until they
 * are told otherwise. Every game that wants a bump therefore has to turn it on, wait, and turn
 * it off, and a port that forgets the last half leaves the pad buzzing on the results screen.
 * That waiting is the whole of this file, and it is the same in every port, so it is here and
 * not in one.
 *
 * A decay thread does the turning off. The alternative - decaying from the game's own frame loop
 * - costs a patch in each title and does nothing on a frame the game misses, which is precisely
 * when a long rumble is most noticeable.
 *
 * **It is safe to call `oops_haptics_bump` from anywhere, including a physics step running at a
 * different rate from the frame.** It only ever raises a target the thread is lowering.
 */
#ifndef OOPS_APPS_HAPTICS_H
#define OOPS_APPS_HAPTICS_H

/* Start the decay thread. Safe to call twice; the second call does nothing. Returns 0 on
   success, non-zero if no thread could be started - in which case every call below is a no-op
   rather than a pad left running. */
int oops_haptics_init(void);

/* Stop the thread and silence the motors. A title that ends without this leaves the pad buzzing,
   which is the failure this whole file exists to prevent. */
void oops_haptics_quit(void);

/*
 * One hit. `strength` is 0..1 and is *not* a duration - the decay is fixed, so a light tap and a
 * hard one differ in how hard the motors start rather than how long they run.
 *
 * Raises the current level rather than replacing it: a run of small bounces should not cut off
 * the big one that started them.
 */
void oops_haptics_bump(float strength);

/* Silence now, without stopping the thread - for a pause, a menu, or the end of a run. */
void oops_haptics_silence(void);

/*
 * Whether rumble is wanted at all. **Off is a real preference** - somebody plays late at night
 * with the pad on a desk, and a game with no way to stop it buzzing is a game they turn off.
 * Read from `/app0/oops-input` (`rumble=on|off`, default on) by `oops_haptics_init`, and
 * settable at runtime for a title that offers it in its own options.
 */
int  oops_haptics_enabled(void);
void oops_haptics_set_enabled(int on);

#endif
