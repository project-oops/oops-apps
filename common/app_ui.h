/*
 * app_ui.h - text layout the utility apps' information pages share.
 */
#ifndef OOPS_APPS_APP_UI_H
#define OOPS_APPS_APP_UI_H

#include "oops/draw.h"
#include "oops/freestd.h"

#ifndef OOPS_APP_VERSION
#define OOPS_APP_VERSION "dev"
#endif

/* One row at scale 3: the label in grey at x=48, the value in white at x=380. Returns
 * the y of the next row. */
static inline int app_ui_row(oops_surface_t *surf, int y, const char *label,
                             const char *value) {
    oops_draw_text(surf, 48, y, label, OOPS_COLOR_GRAY, 3);
    oops_draw_text(surf, 380, y, value, OOPS_COLOR_WHITE, 3);
    return y + 44;
}

/* "v <build>" in grey at scale 2. A negative x right-aligns it 48 pixels from the
 * right edge. */
static inline void app_ui_version(oops_surface_t *surf, int x, int y) {
    char buf[64];
    int n = oops_snprintf(buf, sizeof(buf), "v %s", OOPS_APP_VERSION);
    if (n > (int)sizeof(buf) - 1)
        n = (int)sizeof(buf) - 1;
    if (x < 0)
        x = (int)surf->width - 48 - n * 8 * 2;
    oops_draw_text(surf, x, y, buf, OOPS_COLOR_GRAY, 2);
}

#endif
