/*
 * A loading screen for a title's own start-up work: the title's icon, its name, a
 * progress bar and a status line, drawn on the CPU into a display of its own.
 *
 * A port's first start can take minutes before its renderer opens anything - unpacking
 * a data archive (`tar_unpack.h`), converting assets - and until then the screen is
 * black, which looks the same as a hang. This fills that time, then gets out of the
 * way:
 *
 *     oops_loading_t *ls = oops_loading_open("SuperTuxKart");
 *     oops_tar_unpack_once(archive, dest, marker, oops_loading_tar_progress, ls);
 *     oops_loading_close(ls);        // before the port opens its own display
 *
 * It opens the display itself and closes it again, because the port opens its own
 * afterwards (SDL, oops-gl, or Mesa); it never opens one while another is open. It
 * draws with `oops/draw.h` and nothing else, so it works the same for a freestanding
 * and a hosted title. The icon is the package's `sce_sys/icon0.png`; without one, the
 * name alone is shown.
 *
 * Every call accepts NULL and does nothing, so a title need not branch on whether the
 * display could be opened. It needs the `display`, `draw` and `png` features.
 */
#ifndef OOPS_APPS_LOADING_SCREEN_H
#define OOPS_APPS_LOADING_SCREEN_H

#include <stdint.h>

#include "tar_unpack.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oops_loading oops_loading_t;

/* Opens a display and shows `title` with the icon. NULL when a display is already open
 * or none could be opened; the reason is logged. */
oops_loading_t *oops_loading_open(const char *title);

/*
 * Shows `done` of `total` on the bar and `status` beneath it (NULL keeps the last one).
 * `total` 0 draws no bar, for work whose size is not known. Redraws at most ten times a
 * second, since a flip tiles the whole frame on the CPU and a caller reporting every
 * file would spend its time presenting; the last step (`done == total`) always draws.
 */
void oops_loading_update(oops_loading_t *ls, uint64_t done, uint64_t total,
                         const char *status);

/* An `oops_tar_progress_fn`: pass it with the loading screen as `user`. The bar is the
 * archive's bytes; the status is the file count and the entry just written. */
void oops_loading_tar_progress(const oops_tar_progress_t *progress, void *user);

/* Closes the display, leaving it free for the port to open. */
void oops_loading_close(oops_loading_t *ls);

/*
 * `oops_tar_unpack_once` behind a loading screen titled `title`, for a title's first
 * start. The screen opens only when `marker` is missing - every later start goes
 * straight on - and closes before this returns, whatever the outcome. On a failure the
 * screen shows the error for a few seconds before closing, since the log is not where a
 * player looks. Returns what the unpack returned.
 */
int oops_loading_unpack_once(const char *title, const char *archive, const char *dest,
                             const char *marker);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_APPS_LOADING_SCREEN_H */
