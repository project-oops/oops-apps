#include "save-browser.h"

#include "oops/draw.h"

#define BG 0xFF0D1116u
#define ACCENT OOPS_COLOR_CYAN
#define LABEL OOPS_COLOR_GRAY
#define VALUE OOPS_COLOR_WHITE
#define SELECT 0xFF1E2A33u

#define ROW_H 40
#define LIST_TOP 130
#define LIST_BOTTOM_MARGIN 60

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

int save_browser_visible_rows(unsigned int height) {
    int usable = (int)height - LIST_TOP - LIST_BOTTOM_MARGIN;
    if (usable < ROW_H) {
        return 1;
    }
    return usable / ROW_H;
}

void save_browser_move(save_browser_view_t *view, int count, int visible, int delta) {
    if (!view || count <= 0) {
        if (view) {
            view->selected = 0;
            view->scroll = 0;
        }
        return;
    }
    view->selected += delta;
    if (view->selected < 0) {
        view->selected = 0;
    }
    if (view->selected >= count) {
        view->selected = count - 1;
    }
    /* Keep the selection on screen: scroll only as far as it must. */
    if (view->selected < view->scroll) {
        view->scroll = view->selected;
    }
    if (view->selected >= view->scroll + visible) {
        view->scroll = view->selected - visible + 1;
    }
    if (view->scroll < 0) {
        view->scroll = 0;
    }
}

int save_browser_render(oops_surface_t *surf, const save_entry_t *entries, int count,
                        const save_browser_view_t *view) {
    if (!surf || !view) {
        return 0;
    }

    oops_draw_clear(surf, BG);
    oops_draw_text(surf, 48, 40, "OOPS save-browser", ACCENT, 4);
    oops_draw_rect(surf, 48, 96, (int)surf->width - 96, 3, ACCENT);

    if (count <= 0 || !entries) {
        oops_draw_text(surf, 48, LIST_TOP, "no saves found", LABEL, 3);
        return 0;
    }

    int visible = save_browser_visible_rows(surf->height);
    int drawn = 0;
    char num[11];

    for (int i = 0; i < visible; i++) {
        int idx = view->scroll + i;
        if (idx >= count) {
            break;
        }
        int y = LIST_TOP + i * ROW_H;
        if (idx == view->selected) {
            oops_draw_rect(surf, 40, y - 6, (int)surf->width - 80, ROW_H, SELECT);
        }
        oops_draw_text(surf, 56, y, entries[idx].name, VALUE, 3);
        oops_draw_text(surf, 560, y, entries[idx].user, LABEL, 2);
        char size[16];
        int k = 0;
        const char *p = u32_dec(entries[idx].size_kb, num);
        while (*p) size[k++] = *p++;
        size[k++] = ' ';
        size[k++] = 'K';
        size[k] = '\0';
        oops_draw_text(surf, 860, y, size, LABEL, 2);
        drawn++;
    }

    /* A count, so a long list does not look like a short one scrolled. */
    char footer[48];
    int k = 0;
    const char *p = u32_dec((uint32_t)view->selected + 1u, num);
    while (*p) footer[k++] = *p++;
    footer[k++] = '/';
    p = u32_dec((uint32_t)count, num);
    while (*p) footer[k++] = *p++;
    footer[k] = '\0';
    oops_draw_text(surf, 48, (int)surf->height - 48, footer, LABEL, 2);

    return drawn;
}
