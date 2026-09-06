#ifndef OOPS_APPS_SHELL_SKIN_H
#define OOPS_APPS_SHELL_SKIN_H

#include <stddef.h>
#include <stdint.h>

/*
 * shell-skin: restyle the home screen by injecting a userscript into it.
 *
 * The system UI (SceShellUI) is a WebKit process, so the *content* of a modification is CSS
 * and JavaScript - a userscript, exactly as a browser extension would apply. That content is
 * pure text this composes and is fully testable on a host. Getting it *into* the running shell
 * is the console-only, and currently unproven, half - see the README and the obSCEne probe it
 * waits on.
 *
 * What each recipe is worth is deliberately uneven, and honestly so:
 *   - BACKGROUND overlays a full-screen layer of its own and needs to know nothing about the
 *     shell's markup, so it is robust.
 *   - HIDE_STORE targets an element by selector, and the selector is a *hypothesis* until the
 *     real DOM is inspected on hardware. It is marked as such rather than shipped as fact.
 */

enum {
    SHELL_SKIN_BACKGROUND = 1u << 0, /* a full-screen background layer - robust */
    SHELL_SKIN_HIDE_STORE = 1u << 1, /* hide the store tile - selector is a hypothesis */
    SHELL_SKIN_BANNER     = 1u << 2, /* a small "modified by oops" corner mark - robust */
};

/*
 * Compose the userscript for `recipes` into `out` (freestanding, no stdio). Returns the length
 * written, or 0 if the buffer was too small - never a partial script, because a half-written
 * `<script>` is worse than none. `background` is a 0xRRGGBB colour for the BACKGROUND recipe.
 */
size_t shell_skin_compose(char *out, size_t max, unsigned int recipes, uint32_t background);

#endif /* OOPS_APPS_SHELL_SKIN_H */
