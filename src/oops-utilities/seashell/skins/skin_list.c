#include "../skin.h"
#include "../home.h"
#include "oops/freestd.h"

static int list_render_main(oops_surface_t *surf, const struct home_model *m,
                            const struct home_skin *skin) {
    int drawn = 0;
    const home_theme_t *theme = &skin->theme;
    int sw = (int)surf->width;

    int total_titles = home_get_category_item_count(m, skin, m->category_idx);
    int cursor = m->category_cursor[m->category_idx];

    int y = 50;
    (void)oops_draw_text(surf, theme->margin_x, y, "SYSTEM BROWSER", theme->text, 2);
    char count_buf[64];
    oops_snprintf(count_buf, sizeof(count_buf), "%d TITLES MOUNTED", total_titles);
    (void)oops_draw_text(surf, sw - theme->margin_x - 180, y + 4, count_buf, theme->accent, 1);
    drawn += 2;

    int col_w = (sw - (theme->margin_x * 2) - 24) / 2;
    int row_h = 76;
    int row_gap = 14;
    int start_y = 110;

    int cursor_row = cursor / 2;
    int start_row = (cursor_row > 2) ? (cursor_row - 2) : 0;

    for (int i = 0; i < total_titles; i++) {
        int r = (i / 2) - start_row;
        int c = i % 2;
        if (r < 0 || r >= 5) continue;

        int cx = theme->margin_x + (c * (col_w + 24));
        int cy = start_y + (r * (row_h + row_gap));
        int selected = (i == cursor && m->top_nav == HOME_TOP_NAV_NONE);

        const char *name = "";
        const char *sub = "";
        const struct home_title *title = 0;
        char badge_code = ' ';
        home_get_category_item_info(m, skin, m->category_idx, i, &name, &sub, &title, &badge_code);

        oops_draw_rect(surf, cx, cy, col_w, row_h, theme->panel);
        oops_draw_rect(surf, cx + 8, cy + 8, 60, 60, theme->background);
        home_draw_title_icon(surf, title, cx + 14, cy + 14, 48, 48, theme);

        oops_color_t name_col = selected ? theme->accent : theme->text;
        char list_name[128];
        if (title && title->favorite) {
            oops_snprintf(list_name, sizeof(list_name), "[*] %s", name);
        } else {
            oops_snprintf(list_name, sizeof(list_name), "%s", name);
        }
        (void)oops_draw_text(surf, cx + 78, cy + 14, list_name, name_col, 1);

        char sub_buf[64];
        oops_snprintf(sub_buf, sizeof(sub_buf), "[%s] %s  v%s",
                      title ? (title->category ? title->category : "APP") : "APP",
                      title ? (title->id ? title->id : "-") : "-",
                      title ? (title->version ? title->version : "1.00") : "1.00");
        (void)oops_draw_text(surf, cx + 78, cy + 40, sub_buf, theme->text_dim, 1);

        if (selected) {
            home_draw_cursor(surf, cx, cy, col_w, row_h, theme);
        }
        drawn += 4;
    }

    int by = (int)surf->height - 46;
    (void)oops_draw_text(surf, theme->margin_x, by,
                         "[X] ENTER / PLAY    [O] BACK    [OPTIONS] DATA MANAGEMENT    [SQUARE] LIBRARY",
                         theme->text_dim, 1);
    drawn++;
    return drawn;
}

static int list_cursor_rect(const struct home_model *m, const struct home_skin *skin,
                            int *x, int *y, int *w, int *h) {
    const home_theme_t *theme = &skin->theme;
    int sw = 1920;
    int col_w = (sw - (theme->margin_x * 2) - 24) / 2;
    int row_h = 76;
    int row_gap = 14;
    int start_y = 110;

    int cursor = m->category_cursor[m->category_idx];
    int cursor_row = cursor / 2;
    int start_row = (cursor_row > 2) ? (cursor_row - 2) : 0;
    int r = (cursor / 2) - start_row;
    int c = cursor % 2;

    *x = theme->margin_x + (c * (col_w + 24));
    *y = start_y + (r * (row_h + row_gap));
    *w = col_w;
    *h = row_h;
    return 1;
}

const home_skin_t g_skin_list = {
    .id = "list",
    .name = "MEMCARD",
    .author = "OOPS Clean-Room",
    .description = "Framed column card list",
    .theme = {
        .name = "MEMCARD",
        .background = 0xFF20242Cu,
        .accent = 0xFFC8CDD6u,
        .text = 0xFFEDEFF3u,
        .text_dim = 0xFF767C88u,
        .cursor = 0xFFC8CDD6u,
        .panel = 0xFF2B3038u,
        .column_width = 180,
        .spine_width = 48,
        .tile_width = 110,
        .tile_height = 110,
        .row_height = 24,
        .margin_x = 40,
        .margin_y = 40,
        .text_scale = 2,
        .cursor_style = HOME_CURSOR_BOX
    },
    .categories = {
        { "card", "MEMORY CARD", HOME_CAT_GAMES, 0 }
    },
    .category_count = 1,
    .default_category = 0,
    .primary_axis = HOME_AXIS_NONE,
    .item_axis = HOME_AXIS_VERTICAL,
    .wrap_categories = 0,
    .wrap_items = 1,
    .bumper_nav = 0,
    .init = 0,
    .tick = 0,
    .move = 0,
    .activate = 0,
    .item_count = 0,
    .get_cursor_rect = list_cursor_rect,
    .render_main = list_render_main,
    .render_background = 0
};

