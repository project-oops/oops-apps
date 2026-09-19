#include "../skin.h"
#include "../home.h"
#include "oops/freestd.h"

static int blades_render_main(oops_surface_t *surf, const struct home_model *m,
                              const struct home_skin *skin) {
    int drawn = 0;
    const home_theme_t *theme = &skin->theme;
    int sw = (int)surf->width;

    int act_blade = m->category_idx;
    if (act_blade < 0) act_blade = 0;
    if (act_blade >= skin->category_count) act_blade = skin->category_count - 1;

    oops_color_t blade_bg_top;
    oops_color_t blade_bg_bot;
    oops_color_t blade_accent;
    oops_color_t blade_pill_hi;

    switch (act_blade) {
        case 1: /* GAMES */
            blade_bg_top = 0xFF2E7D32u;
            blade_bg_bot = 0xFF143D18u;
            blade_accent = 0xFF5EB827u;
            blade_pill_hi = 0xFFB2FF59u;
            break;
        case 2: /* MEDIA */
            blade_bg_top = 0xFF1565C0u;
            blade_bg_bot = 0xFF0D2D56u;
            blade_accent = 0xFF1E88E5u;
            blade_pill_hi = 0xFF90CAF9u;
            break;
        case 3: /* SYSTEM */
            blade_bg_top = 0xFF5E35B1u;
            blade_bg_bot = 0xFF28154Du;
            blade_accent = 0xFF8E24AAu;
            blade_pill_hi = 0xFFCE93D8u;
            break;
        case 0: /* LIVE */
        default:
            blade_bg_top = 0xFFD84315u;
            blade_bg_bot = 0xFF4E1605u;
            blade_accent = 0xFFFB8C00u;
            blade_pill_hi = 0xFFFFCC80u;
            break;
    }

    int tab_w = 44;
    int top_y = 30;
    int bot_y = 650;
    int blade_h = bot_y - top_y;

    /* ---- 1. Flanking Curved Left Tabs (for b < act_blade) -------------------------------- */
    for (int b = 0; b < act_blade; b++) {
        int tx = 20 + (b * tab_w);
        oops_draw_rect(surf, tx, top_y, tab_w - 4, blade_h, 0xFF8A9EA7u);
        oops_draw_line(surf, tx, top_y, tx, bot_y, 0xFFE0E0E0u);
        oops_draw_line(surf, tx + 1, top_y, tx + 1, bot_y, 0xFFFFFFFFu);
        oops_draw_line(surf, tx + tab_w - 5, top_y, tx + tab_w - 5, bot_y, 0xFF546E7Au);

        const char *txt = skin->categories[b].label;
        int sy = 240;
        while (*txt != '\0') {
            char ch[2];
            ch[0] = *txt++;
            ch[1] = '\0';
            (void)oops_draw_text(surf, tx + 14, sy, ch, 0xFF263238u, 1);
            sy += 15;
        }
        drawn += 4;
    }

    /* ---- 2. Flanking Curved Right Tabs (for b > act_blade) ------------------------------- */
    for (int b = skin->category_count - 1; b > act_blade; b--) {
        int tx = sw - 20 - ((skin->category_count - b) * tab_w);
        oops_draw_rect(surf, tx, top_y, tab_w - 4, blade_h, 0xFF8A9EA7u);
        oops_draw_line(surf, tx, top_y, tx, bot_y, 0xFFE0E0E0u);
        oops_draw_line(surf, tx + tab_w - 5, top_y, tx + tab_w - 5, bot_y, 0xFF546E7Au);

        const char *txt = skin->categories[b].label;
        int sy = 240;
        while (*txt != '\0') {
            char ch[2];
            ch[0] = *txt++;
            ch[1] = '\0';
            (void)oops_draw_text(surf, tx + 14, sy, ch, 0xFF263238u, 1);
            sy += 15;
        }
        drawn += 4;
    }

    /* ---- 3. Active Blade Panel ----------------------------------------------------------- */
    int bx = 20 + (act_blade * tab_w);
    int rx_limit = sw - 20 - ((skin->category_count - 1 - act_blade) * tab_w);
    int bw = rx_limit - bx;

    oops_draw_rect_gradient(surf, bx, top_y, bw, blade_h, blade_bg_top, blade_bg_bot, 1);

    for (int i = 0; i < 4; i++) {
        int ly = top_y + 80 + (i * 120);
        oops_draw_line_blend(surf, bx + 10, ly, bx + bw - 10, ly + 40, 0x14FFFFFFu);
    }

    oops_draw_line(surf, bx, top_y, bx + bw, top_y, 0xFFE0E0E0u);
    oops_draw_line(surf, bx, top_y + 1, bx + bw, top_y + 1, 0xFFFFFFFFu);
    oops_draw_line(surf, bx, top_y, bx, bot_y, 0xFFE0E0E0u);
    oops_draw_line(surf, bx + bw - 1, top_y, bx + bw - 1, bot_y, 0xFF546E7Au);
    drawn += 8;

    /* ---- 4. Top Header & Logo ----------------------------------------------------------- */
    static const char *s_headers[] = {
        "Network", "Select a Game", "Media Library", "System Settings"
    };
    const char *hdr = (act_blade < 4) ? s_headers[act_blade] : "Blades";
    (void)oops_draw_text(surf, bx + 36, top_y + 18, hdr, 0xFFFFFFFFu, 2);

    int logo_cx = bx + bw - 180;
    int logo_cy = top_y + 26;
    oops_draw_circle(surf, logo_cx, logo_cy, 18, 0xFFCCCCCCu, 1);
    oops_draw_circle(surf, logo_cx, logo_cy, 16, 0xFFECEFF1u, 1);
    oops_draw_line(surf, logo_cx - 9, logo_cy - 9, logo_cx + 9, logo_cy + 9, 0xFF5EB827u);
    oops_draw_line(surf, logo_cx - 9, logo_cy + 9, logo_cx + 9, logo_cy - 9, 0xFF5EB827u);
    oops_draw_line(surf, logo_cx - 8, logo_cy - 8, logo_cx + 8, logo_cy + 8, 0xFF7BC043u);
    oops_draw_line(surf, logo_cx - 8, logo_cy + 8, logo_cx + 8, logo_cy - 8, 0xFF7BC043u);
    oops_draw_circle(surf, logo_cx, logo_cy, 4, 0xFFECEFF1u, 1);

    (void)oops_draw_text(surf, logo_cx + 26, top_y + 18, "BLADES", 0xFFFFFFFFu, 2);
    drawn += 6;

    /* ---- 5. Left Column: Vertical List of Items ------------------------------------------ */
    int lx = bx + 30;
    int lw = (bw * 48) / 100;
    int count = home_get_category_item_count(m, skin, act_blade);
    int cur = m->category_cursor[act_blade];

    int start_j = (cur > 2) ? (cur - 2) : 0;
    if (start_j + 5 > count && count >= 5) {
        start_j = count - 5;
    }

    for (int j = 0; j < 5; j++) {
        int idx = start_j + j;
        if (idx >= count) break;

        int iy = top_y + 70 + (j * 70);
        int ih = 62;
        int is_foc = (idx == cur && m->top_nav == HOME_TOP_NAV_NONE);

        const char *name = "";
        const char *sub = "";
        const struct home_title *title_ptr = 0;
        char rating = 'G';
        home_get_category_item_info(m, skin, act_blade, idx, &name, &sub, &title_ptr, &rating);

        if (is_foc) {
            oops_draw_rect(surf, lx, iy, lw, ih, blade_accent);
            oops_draw_rect(surf, lx, iy, lw, 3, blade_pill_hi);
            oops_draw_rect(surf, lx, iy + ih - 3, lw, 3, 0x40000000u);

            if (title_ptr != 0) {
                home_draw_title_icon(surf, title_ptr, lx + 8, iy + 7, 48, 48, theme);
            } else {
                oops_draw_rect(surf, lx + 8, iy + 7, 48, 48, 0xFF143D18u);
            }
            oops_draw_line(surf, lx + 7, iy + 6, lx + 57, iy + 6, 0xFFFFFFFFu);
            oops_draw_line(surf, lx + 7, iy + 56, lx + 57, iy + 56, 0xFFFFFFFFu);
            oops_draw_line(surf, lx + 7, iy + 6, lx + 7, iy + 56, 0xFFFFFFFFu);
            oops_draw_line(surf, lx + 57, iy + 6, lx + 57, iy + 56, 0xFFFFFFFFu);

            char dname[128];
            if (title_ptr && title_ptr->favorite) {
                oops_snprintf(dname, sizeof(dname), "[*] %s", name);
            } else {
                oops_snprintf(dname, sizeof(dname), "%s", name);
            }
            (void)oops_draw_text(surf, lx + 66, iy + 14, dname, 0xFF0A260Cu, 1);
            (void)oops_draw_text(surf, lx + 66, iy + 36, sub, 0xFF1B5E20u, 1);

            int rx_b = lx + lw - 44;
            oops_color_t rbg = (rating == 'M') ? 0xFF1565C0u : 0xFF2E7D32u;
            oops_draw_rect(surf, rx_b, iy + 14, 34, 34, rbg);
            oops_draw_line(surf, rx_b, iy + 14, rx_b + 34, iy + 14, 0xFFFFFFFFu);
            oops_draw_line(surf, rx_b, iy + 48, rx_b + 34, iy + 48, 0xFFFFFFFFu);
            oops_draw_line(surf, rx_b, iy + 14, rx_b, iy + 48, 0xFFFFFFFFu);
            oops_draw_line(surf, rx_b + 34, iy + 14, rx_b + 34, iy + 48, 0xFFFFFFFFu);
            char rstr[2]; rstr[0] = rating; rstr[1] = '\0';
            (void)oops_draw_text(surf, rx_b + 11, iy + 22, rstr, 0xFFFFFFFFu, 2);
        } else {
            oops_draw_rect_blend(surf, lx, iy, lw, ih, 0x40000000u);

            if (title_ptr != 0) {
                home_draw_title_icon(surf, title_ptr, lx + 8, iy + 7, 48, 48, theme);
            } else {
                oops_draw_rect(surf, lx + 8, iy + 7, 48, 48, 0x30000000u);
            }

            (void)oops_draw_text(surf, lx + 66, iy + 14, name, 0xFFFFFFFFu, 1);
            (void)oops_draw_text(surf, lx + 66, iy + 36, sub, 0xFFB0BEC5u, 1);

            int rx_b = lx + lw - 44;
            oops_draw_rect(surf, rx_b, iy + 14, 34, 34, 0x30000000u);
            char rstr[2]; rstr[0] = rating; rstr[1] = '\0';
            (void)oops_draw_text(surf, rx_b + 11, iy + 22, rstr, 0xFFB0BEC5u, 2);
        }
        drawn += 6;
    }

    if (count > 5) {
        int cy = top_y + 70 + (5 * 70) + 4;
        (void)oops_draw_text(surf, lx + (lw / 2) - 8, cy, "v", 0xFFB0BEC5u, 1);
        drawn++;
    }

    /* ---- 6. Right Column: Showcase / Feature Preview Card -------------------------------- */
    int rx_card = lx + lw + 24;
    int rw_card = bw - (rx_card - bx) - 30;
    int ry_card = top_y + 70;
    int rh_card = 420;

    oops_draw_rect_blend(surf, rx_card, ry_card, rw_card, rh_card, 0x38000000u);
    oops_draw_line_blend(surf, rx_card, ry_card, rx_card + rw_card, ry_card, 0x40FFFFFFu);

    const char *cur_name = "";
    const char *cur_sub = "";
    const struct home_title *cur_title = 0;
    char cur_rating = 'G';
    home_get_category_item_info(m, skin, act_blade, cur, &cur_name, &cur_sub, &cur_title, &cur_rating);

    int art_x = rx_card + 14;
    int art_y = ry_card + 14;
    int art_w = rw_card - 28;
    int art_h = 180;

    oops_draw_rect(surf, art_x, art_y, art_w, art_h, 0xFF102812u);
    if (cur_title != 0) {
        home_draw_title_icon(surf, cur_title, art_x + (art_w / 2) - 48, art_y + 24, 96, 96, theme);
    }
    oops_draw_line_blend(surf, art_x, art_y, art_x + art_w, art_y + 60, 0x24FFFFFFu);

    int desc_y = art_y + art_h + 16;
    (void)oops_draw_text(surf, art_x, desc_y, cur_name, 0xFFFFFFFFu, 1);
    desc_y += 20;

    char line1[64];
    oops_snprintf(line1, sizeof(line1), "Publisher: %s", cur_sub);
    (void)oops_draw_text(surf, art_x, desc_y, line1, 0xFFB2FF59u, 1);
    desc_y += 20;

    (void)oops_draw_text(surf, art_x, desc_y,
                         "Experience next-gen gameplay with high fidelity,", 0xFFEFF7E8u, 1);
    desc_y += 16;
    (void)oops_draw_text(surf, art_x, desc_y,
                         "ultra-responsive 60 FPS presentation, and 3D audio.", 0xFFEFF7E8u, 1);
    desc_y += 24;

    (void)oops_draw_text(surf, art_x, desc_y, "(*) Demo Available", 0xFFB0BEC5u, 1);
    desc_y += 18;
    (void)oops_draw_text(surf, art_x, desc_y, "(>) Video Available (4K 60FPS)", 0xFFB0BEC5u, 1);
    drawn += 10;

    /* ---- 7. Bottom Bar: Action Button Legend --------------------------------------------- */
    int leg_y = bot_y + 14;
    int leg_x = bx + 36;

    /* (Y) Yellow Circle */
    oops_draw_circle(surf, leg_x + 8, leg_y + 6, 8, 0xFFFFD700u, 1);
    (void)oops_draw_text(surf, leg_x + 5, leg_y + 2, "Y", 0xFF000000u, 1);
    (void)oops_draw_text(surf, leg_x + 22, leg_y + 2, "Family & Details", 0xFFEFF7E8u, 1);

    /* (X) Blue Circle */
    int leg_x2 = leg_x + 200;
    oops_draw_circle(surf, leg_x2 + 8, leg_y + 6, 8, 0xFF0078D7u, 1);
    (void)oops_draw_text(surf, leg_x2 + 5, leg_y + 2, "X", 0xFFFFFFFFu, 1);
    (void)oops_draw_text(surf, leg_x2 + 22, leg_y + 2, "Search", 0xFFEFF7E8u, 1);

    /* (A) Green Circle & (B) Red Circle on right */
    int leg_xr = bx + bw - 180;
    oops_draw_circle(surf, leg_xr + 8, leg_y + 6, 8, 0xFF107C10u, 1);
    (void)oops_draw_text(surf, leg_xr + 5, leg_y + 2, "A", 0xFFFFFFFFu, 1);
    (void)oops_draw_text(surf, leg_xr + 22, leg_y + 2, "Select", 0xFFEFF7E8u, 1);

    int leg_xr2 = leg_xr + 90;
    oops_draw_circle(surf, leg_xr2 + 8, leg_y + 6, 8, 0xFFE81123u, 1);
    (void)oops_draw_text(surf, leg_xr2 + 5, leg_y + 2, "B", 0xFFFFFFFFu, 1);
    (void)oops_draw_text(surf, leg_xr2 + 22, leg_y + 2, "Back", 0xFFEFF7E8u, 1);
    drawn += 8;

    return drawn;
}

static int blades_cursor_rect(const struct home_model *m, const struct home_skin *skin,
                              int *x, int *y, int *w, int *h) {
    int sw = 1920;
    int act_blade = m->category_idx;
    int tab_w = 44;
    int bx = 20 + (act_blade * tab_w);
    int rx_limit = sw - 20 - ((skin->category_count - 1 - act_blade) * tab_w);
    int bw = rx_limit - bx;
    int lx = bx + 30;
    int lw = (bw * 48) / 100;

    int cur = m->category_cursor[act_blade];
    int count = home_get_category_item_count(m, skin, act_blade);
    int start_j = (cur > 2) ? (cur - 2) : 0;
    if (start_j + 5 > count && count >= 5) {
        start_j = count - 5;
    }
    int j = cur - start_j;
    if (j < 0) j = 0;
    if (j > 4) j = 4;

    *x = lx;
    *y = 30 + 70 + (j * 70);
    *w = lw;
    *h = 62;
    return 1;
}

const home_skin_t g_skin_blades = {
    .id = "blades",
    .name = "BLADES",
    .author = "OOPS Clean-Room",
    .description = "Curved tab dashboard with flanking blades",
    .theme = {
        .name = "BLADES",
        .background = 0xFF0E2A12u,
        .accent = 0xFF7BC043u,
        .text = 0xFFEFF7E8u,
        .text_dim = 0xFF5E7A50u,
        .cursor = 0xFF7BC043u,
        .panel = 0xFF163A1Cu,
        .column_width = 200,
        .spine_width = 56,
        .tile_width = 120,
        .tile_height = 120,
        .row_height = 26,
        .margin_x = 40,
        .margin_y = 44,
        .text_scale = 2,
        .cursor_style = HOME_CURSOR_BAR
    },
    .categories = {
        { "live", "network", HOME_CAT_CUSTOM, 0 },
        { "games", "games", HOME_CAT_GAMES, 0 },
        { "media", "media", HOME_CAT_MEDIA, 0 },
        { "system", "system", HOME_CAT_SETTINGS, 0 }
    },
    .category_count = 4,
    .default_category = 1, /* games */
    .primary_axis = HOME_AXIS_HORIZONTAL,
    .item_axis = HOME_AXIS_VERTICAL,
    .wrap_categories = 1,
    .wrap_items = 1,
    .bumper_nav = 1,
    .init = 0,
    .tick = 0,
    .move = 0,
    .activate = 0,
    .item_count = 0,
    .get_cursor_rect = blades_cursor_rect,
    .render_main = blades_render_main,
    .render_background = 0
};

