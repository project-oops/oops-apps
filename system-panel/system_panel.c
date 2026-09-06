#include "system_panel.h"

#include "oops/draw.h"

/*
 * No stdio here on purpose: this file compiles freestanding for the target as well as hosted
 * for the self-test, so it formats its own numbers rather than reaching for snprintf, which
 * the target has no libc to provide.
 */

/* Write `value` as decimal into `out` (at least 11 bytes), returning it for chaining. */
static const char *u32_dec(uint32_t value, char *out) {
    char rev[11];
    int n = 0;
    do {
        rev[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u && n < 10);
    int i = 0;
    while (n > 0) {
        out[i++] = rev[--n];
    }
    out[i] = '\0';
    return out;
}

/* The panel's colours, kept together so a theme is one edit. */
#define PANEL_BG 0xFF0D1116u
#define PANEL_ACCENT OOPS_COLOR_CYAN
#define PANEL_LABEL OOPS_COLOR_GRAY
#define PANEL_VALUE OOPS_COLOR_WHITE

/* One label/value line, at a fixed left margin. Returns the next y. */
static int row(oops_surface_t *surf, int y, const char *label, const char *value) {
    const int label_x = 48;
    const int value_x = 360;
    const int scale = 3;
    oops_draw_text(surf, label_x, y, label, PANEL_LABEL, scale);
    oops_draw_text(surf, value_x, y, value, PANEL_VALUE, scale);
    return y + 44;
}

int system_panel_render(oops_surface_t *surf, const oops_system_info_t *info) {
    if (!surf || !info) {
        return 0;
    }

    oops_draw_clear(surf, PANEL_BG);
    oops_draw_text(surf, 48, 40, "OOPS system panel", PANEL_ACCENT, 4);
    oops_draw_rect(surf, 48, 96, 900, 3, PANEL_ACCENT);

    char num[11];
    int y = 140;
    int rows = 0;

    const char *generation =
        info->generation == 5 ? "Prospero"
        : info->generation == 4 ? "Orbis"
                                : "unknown";
    y = row(surf, y, "generation", generation);
    rows++;

    y = row(surf, y, "firmware", info->firmware_str[0] ? info->firmware_str : "-");
    rows++;

    y = row(surf, y, "memory MB", u32_dec((uint32_t)info->total_ram_mb, num));
    rows++;

    y = row(surf, y, "user", info->user_name[0] ? info->user_name : "-");
    rows++;

    oops_draw_text(surf, 48, y + 24, "press circle to exit", PANEL_LABEL, 2);

    return rows;
}
