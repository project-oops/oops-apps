/*
 * The controls card: the shared controller drawing (`common/assets/controls`) beside a
 * title's own button table, shown on the title's display before the game starts.
 *
 * Nothing on a console port's screen says what the buttons do, and a pad has no labels
 * under the player's fingers. A title opts in by carrying `controls.txt` beside its
 * Makefile; `common/controls.mk` then builds this in, embeds the table and the drawing
 * in the payload, and puts `oops_controls_entry` in front of the title's own entry
 * point, so no shim changes. The card stays up until Cross or Options is pressed, or
 * for `OOPS_CONTROLS_SECONDS`.
 *
 * `controls.txt`, one line each:
 *
 *     # a comment
 *     R2 = Accelerate
 *     Left stick = Steer
 *     > A note under the table, such as where the game's own help is.
 *
 * The button names are this pad's (Cross, Circle, Square, Triangle, L1, R2, Options,
 * Create, Left stick, D-pad). The table is written by hand, so it can drift from a game
 * that lets the player rebind; a game with its own help screen shows the live bindings
 * there too (Neverball, SuperTuxKart).
 *
 * Drawn with `oops/draw.h` on a display of its own, which is closed before the title
 * opens its renderer - the loading screen's pattern (`loading_screen.h`).
 */
#ifndef OOPS_APPS_CONTROLS_CARD_H
#define OOPS_APPS_CONTROLS_CARD_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Shows the card for `title` from `table` (the text of a controls.txt, `table_len`
 * bytes) and `png` (the controller drawing), and returns once it is dismissed. Does
 * nothing when a display is already open or none can be opened.
 */
void oops_controls_card_show(const char *title, const char *table, size_t table_len,
                             const void *png, size_t png_len);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_APPS_CONTROLS_CARD_H */
