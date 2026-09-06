#ifndef OOPS_APPS_SAVE_BROWSER_H
#define OOPS_APPS_SAVE_BROWSER_H

#include "oops/draw.h"

/*
 * save-browser: the saves on the machine, listed.
 *
 * The list and its rendering are pure: given entries and where the cursor is, it draws. The
 * mounting and enumeration - the part that reads a real disk - is the console-only half. So
 * the list logic (scrolling, selection, drawing) is testable on a host with a made-up list.
 */

#define SAVE_BROWSER_NAME_MAX 32
#define SAVE_BROWSER_USER_MAX 32

typedef struct save_entry {
    char name[SAVE_BROWSER_NAME_MAX];
    char user[SAVE_BROWSER_USER_MAX];
    uint32_t size_kb;
} save_entry_t;

/* Where the list is: which row is selected, and the first row on screen. */
typedef struct save_browser_view {
    int selected;
    int scroll;
} save_browser_view_t;

/* How many rows fit on a surface of this height. */
int save_browser_visible_rows(unsigned int height);

/* Move the selection by `delta`, clamping to [0, count) and scrolling to keep it on screen. */
void save_browser_move(save_browser_view_t *view, int count, int visible, int delta);

/* Draw the list into `surf`. Returns the number of rows drawn on screen. */
int save_browser_render(oops_surface_t *surf, const save_entry_t *entries, int count,
                        const save_browser_view_t *view);

#endif /* OOPS_APPS_SAVE_BROWSER_H */
