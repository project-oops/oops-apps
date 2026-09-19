#include "../skin.h"
#include "../home.h"
#include "oops/freestd.h"

static void format_badge(const char *cat, char *out, size_t out_len) {
    if (out == 0 || out_len == 0) return;
    if (cat == 0 || cat[0] == '\0') {
        oops_snprintf(out, out_len, "APP");
        return;
    }
    if (cat[0] == 'P' && cat[1] == 'R' && cat[2] == 'O') {
        oops_snprintf(out, out_len, "NATIVE");
    } else if (cat[0] == 'O' && cat[1] == 'R' && cat[2] == 'B') {
        oops_snprintf(out, out_len, "LEGACY");
    } else {
        size_t l = 0;
        while (cat[l] != '\0' && l < out_len - 1) {
            out[l] = cat[l];
            l++;
        }
        out[l] = '\0';
    }
}

static int modern_render_main(oops_surface_t *surf, const struct home_model *m,
                              const struct home_skin *skin) {
    int drawn = 0;
    const home_theme_t *theme = &skin->theme;
    int sw = (int)surf->width;

    /* ---- 1. Top Bar (GAMES / MEDIA tabs, Search, Settings, Profile) --------------------- */
    int top_y = 36;
    int is_media = (m->category_idx == 1);

    /* Tab: GAMES */
    oops_color_t g_col = (!is_media) ? theme->accent : theme->text_dim;
    (void)oops_draw_text(surf, theme->margin_x, top_y, "GAMES", g_col, 2);
    if (!is_media && m->top_nav == HOME_TOP_NAV_TABS) {
        oops_draw_rect(surf, theme->margin_x, top_y + 24, 70, 3, theme->accent);
    }

    /* Tab: MEDIA */
    oops_color_t m_col = is_media ? theme->accent : theme->text_dim;
    (void)oops_draw_text(surf, theme->margin_x + 100, top_y, "MEDIA", m_col, 2);
    if (is_media && m->top_nav == HOME_TOP_NAV_TABS) {
        oops_draw_rect(surf, theme->margin_x + 100, top_y + 24, 70, 3, theme->accent);
    }

    /* Search [TRIANGLE] */
    oops_color_t s_col = (m->top_nav == HOME_TOP_NAV_SEARCH) ? theme->accent : theme->text_dim;
    (void)oops_draw_text(surf, sw - 420, top_y + 2, "[^] SEARCH", s_col, 1);

    /* Settings [GEAR] */
    oops_color_t set_col = (m->top_nav == HOME_TOP_NAV_SETTINGS) ? theme->accent : theme->text_dim;
    (void)oops_draw_text(surf, sw - 300, top_y + 2, "[*] SETTINGS", set_col, 1);

    /* Clock & Profile */
    char clk[32];
    int hr = m->status.hour;
    int mn = m->status.minute;
    if (hr == 0 && mn == 0) { hr = 16; mn = 0; }
    oops_snprintf(clk, sizeof(clk), "%02d:%02d", hr, mn);
    (void)oops_draw_text(surf, sw - 140, top_y + 2, clk, theme->text, 1);
    drawn += 6;

    oops_draw_line_blend(surf, theme->margin_x, top_y + 36, sw - theme->margin_x, top_y + 36, 0x20FFFFFFu);

    /* ---- 2. Horizontal Tile Carousel ----------------------------------------------------- */
    int count = home_get_category_item_count(m, skin, m->category_idx);
    int cur = m->category_cursor[m->category_idx];

    int base_y = 130;
    int step = theme->tile_width + (theme->margin_x / 2);
    int offset_x = (cur > 3) ? ((cur - 3) * step) : 0;

    for (int i = 0; i < count; i++) {
        int tx = theme->margin_x + (i * step) - offset_x;
        if (tx + theme->tile_width < 0) continue;
        if (tx >= sw) break;

        int selected = (i == cur && m->top_nav == HOME_TOP_NAV_NONE);
        int th = selected ? (theme->tile_height + 20) : theme->tile_height;
        int ty = selected ? (base_y - 10) : base_y;

        const char *name = "";
        const char *sub = "";
        const struct home_title *title = 0;
        char badge_code = ' ';
        home_get_category_item_info(m, skin, m->category_idx, i, &name, &sub, &title, &badge_code);

        oops_color_t card_bg = selected ? theme->accent : theme->panel;
        oops_draw_rect(surf, tx, ty, theme->tile_width, th, card_bg);

        int iw = 80;
        int ih = 80;
        if (iw > theme->tile_width - 16) iw = theme->tile_width - 16;
        if (ih > th - 36) ih = th - 36;
        int ix = tx + (theme->tile_width - iw) / 2;
        int iy = ty + 8;
        home_draw_title_icon(surf, title, ix, iy, iw, ih, theme);

        char b_str[16];
        format_badge(title ? title->category : "APP", b_str, sizeof(b_str));
        int bl = 0;
        while (b_str[bl] != '\0') bl++;
        int bx = tx + (theme->tile_width - (bl * 8)) / 2;
        if (bx < tx + 4) bx = tx + 4;
        (void)oops_draw_text(surf, bx, ty + th - 16, b_str,
                             selected ? theme->background : theme->text_dim, 1);
        drawn += 3;

        if (title && title->favorite) {
            (void)oops_draw_text(surf, tx + 6, ty + 6, "[*]", theme->cursor, 1);
            drawn++;
        }

        if (selected) {
            home_draw_cursor(surf, tx, ty, theme->tile_width, th, theme);
        }
    }

    /* ---- 3. Detail Strip Beneath Carousel ----------------------------------------------- */
    if (cur >= 0 && cur < count) {
        const char *cur_name = "";
        const char *cur_sub = "";
        const struct home_title *cur_title = 0;
        char cur_badge = ' ';
        home_get_category_item_info(m, skin, m->category_idx, cur, &cur_name, &cur_sub, &cur_title, &cur_badge);

        int dy = base_y + theme->tile_height + 40;
        char name_buf[128];
        if (cur_title && cur_title->favorite) {
            oops_snprintf(name_buf, sizeof(name_buf), "[*] %s", cur_name);
        } else {
            oops_snprintf(name_buf, sizeof(name_buf), "%s", cur_name);
        }
        (void)oops_draw_text(surf, theme->margin_x, dy, name_buf, theme->text, 2);
        dy += 32;

        char meta_line[128];
        oops_snprintf(meta_line, sizeof(meta_line), "[%s]  %s    VER: %s",
                      cur_title ? (cur_title->category ? cur_title->category : "APP") : "APP",
                      cur_title ? (cur_title->id ? cur_title->id : "-") : "-",
                      cur_title ? (cur_title->version ? cur_title->version : "1.00") : "1.00");
        (void)oops_draw_text(surf, theme->margin_x, dy, meta_line, theme->accent, 1);
        dy += 30;

        (void)oops_draw_text(surf, theme->margin_x, dy, "[X] PLAY", theme->text, 1);
        (void)oops_draw_text(surf, theme->margin_x + 90, dy, "[OPTIONS] OPTIONS", theme->text_dim, 1);
        (void)oops_draw_text(surf, theme->margin_x + 250, dy, "[SQUARE] LIBRARY", theme->text_dim, 1);
        drawn += 5;
    }

    return drawn;
}

static int modern_cursor_rect(const struct home_model *m, const struct home_skin *skin,
                              int *x, int *y, int *w, int *h) {
    const home_theme_t *theme = &skin->theme;
    int cur = m->category_cursor[m->category_idx];
    int step = theme->tile_width + (theme->margin_x / 2);
    int offset_x = (cur > 3) ? ((cur - 3) * step) : 0;
    *x = theme->margin_x + (cur * step) - offset_x;
    *y = 130 - 10;
    *w = theme->tile_width;
    *h = theme->tile_height + 20;
    return 1;
}

const home_skin_t g_skin_modern = {
    .id = "modern",
    .name = "MODERN",
    .author = "OOPS Clean-Room",
    .description = "Modern tile carousel over a detail strip",
    .theme = {
        .name = "MODERN",
        .background = 0xFF10141Cu,
        .accent = 0xFF3D8BFDu,
        .text = 0xFFF2F5FAu,
        .text_dim = 0xFF7C8493u,
        .cursor = 0xFF3D8BFDu,
        .panel = 0xFF1B212Cu,
        .column_width = 200,
        .spine_width = 56,
        .tile_width = 132,
        .tile_height = 132,
        .row_height = 28,
        .margin_x = 48,
        .margin_y = 64,
        .text_scale = 2,
        .cursor_style = HOME_CURSOR_BOX
    },
    .categories = {
        { "games", "GAMES", HOME_CAT_GAMES, 0 },
        { "media", "MEDIA", HOME_CAT_MEDIA, 0 }
    },
    .category_count = 2,
    .default_category = 0,
    .primary_axis = HOME_AXIS_NONE,
    .item_axis = HOME_AXIS_HORIZONTAL,
    .wrap_categories = 0,
    .wrap_items = 1,
    .bumper_nav = 0,
    .init = 0,
    .tick = 0,
    .move = 0,
    .activate = 0,
    .item_count = 0,
    .get_cursor_rect = modern_cursor_rect,
    .render_main = modern_render_main,
    .render_background = 0
};
