#include "../skin.h"
#include "../home.h"
#include "oops/freestd.h"

typedef enum { PS2_VIEW_MAIN = 0, PS2_VIEW_BROWSER = 1 } ps2_view_t;

static ps2_view_t s_view = PS2_VIEW_MAIN;
static int s_main_cursor = 0; /* 0 = Browser, 1 = System Configuration */
static int s_orb_tick = 0;

/* Fast integer trigonometric functions (degrees, 0..359) */
static int fast_sin_deg(int deg) {
    while (deg < 0)
        deg += 360;
    deg = deg % 360;
    if (deg > 180) {
        return -fast_sin_deg(deg - 180);
    }
    if (deg > 90) {
        deg = 180 - deg;
    }
    /* Parabolic approximation: 4 * d * (180 - d) / 405 gives 0..100 */
    return (4 * deg * (180 - deg)) / 405;
}

static int fast_cos_deg(int deg) {
    return fast_sin_deg(deg + 90);
}

static void list_init(struct home_model *m, const struct home_skin *skin) {
    (void)m;
    (void)skin;
    s_view = PS2_VIEW_MAIN;
    s_main_cursor = 0;
    s_orb_tick = 0;
}

static void list_tick(struct home_model *m, const struct home_skin *skin) {
    (void)m;
    (void)skin;
    s_orb_tick++;
}

static int list_move(struct home_model *m, const struct home_skin *skin, int dir) {
    (void)skin;
    if (m == 0)
        return 0;

    if (s_view == PS2_VIEW_MAIN) {
        if (dir == HOME_UP) {
            s_main_cursor = 0;
            return 1;
        } else if (dir == HOME_DOWN) {
            s_main_cursor = 1;
            return 1;
        }
        return 1;
    } else {
        int total = m->title_count;
        if (total <= 0)
            return 1;

        if (dir == HOME_LEFT) {
            if (m->title_cursor > 0) {
                m->title_cursor--;
            }
            return 1;
        } else if (dir == HOME_RIGHT) {
            if (m->title_cursor < total - 1) {
                m->title_cursor++;
            }
            return 1;
        } else if (dir == HOME_UP) {
            if (m->title_cursor >= 5) {
                m->title_cursor -= 5;
            }
            return 1;
        } else if (dir == HOME_DOWN) {
            if (m->title_cursor + 5 < total) {
                m->title_cursor += 5;
            } else if (m->title_cursor < total - 1) {
                m->title_cursor = total - 1;
            }
            return 1;
        }
        return 1;
    }
}

static int list_activate(struct home_model *m, const struct home_skin *skin) {
    (void)skin;
    if (m == 0)
        return 0;

    if (s_view == PS2_VIEW_MAIN) {
        if (s_main_cursor == 0) {
            s_view = PS2_VIEW_BROWSER;
            return 1;
        } else {
            home_open(m, HOME_SCREEN_SETTINGS);
            return 1;
        }
    } else {
        if (m->title_count > 0 && m->title_cursor >= 0 &&
            m->title_cursor < m->title_count) {
            m->selected_title = m->title_cursor;
            home_open(m, HOME_SCREEN_TITLE_OPTIONS);
            return 1;
        }
        return 1;
    }
}

static int list_back(struct home_model *m, const struct home_skin *skin) {
    (void)skin;
    if (m == 0)
        return 0;
    if (m->depth > 1) {
        return 0; /* Let generic engine pop the screen stack */
    }
    if (s_view == PS2_VIEW_BROWSER) {
        s_view = PS2_VIEW_MAIN;
        return 1;
    }
    return 0;
}

static int list_cursor_rect(const struct home_model *m, const struct home_skin *skin,
                            int *x, int *y, int *w, int *h) {
    (void)skin;
    if (m == 0 || x == 0 || y == 0 || w == 0 || h == 0)
        return 0;

    int sw = 1280;
    int sh = 720;

    if (s_view == PS2_VIEW_MAIN) {
        int sphere_cx = sw * 38 / 100;
        int sphere_cy = sh * 48 / 100;
        int rx = sw * 13 / 100;
        int menu_x = sphere_cx + rx + 45;
        int menu_y = sphere_cy - 20;
        int menu_gap = 38;

        *x = menu_x;
        *y = (s_main_cursor == 0) ? menu_y : (menu_y + menu_gap);
        *w = 260;
        *h = 36;
    } else {
        int total = m->title_count;
        int cursor = m->title_cursor;
        if (cursor < 0)
            cursor = 0;
        if (total > 0 && cursor >= total)
            cursor = total - 1;

        int cursor_row = cursor / 5;
        int start_row = (cursor_row > 1) ? (cursor_row - 1) : 0;
        int r = (cursor / 5) - start_row;
        int c = cursor % 5;
        if (r < 0)
            r = 0;
        if (r > 2)
            r = 2;

        int margin_x = sw * 5 / 100;
        int card_w = 100;
        int card_h = 100;
        int col_gap = (sw - 2 * margin_x - 5 * card_w) / 4;
        if (col_gap < 16)
            col_gap = 16;
        int row_gap = 38;
        int total_grid_w = 5 * card_w + 4 * col_gap;
        int start_x = (sw - total_grid_w) / 2;
        int start_y = 170;

        *x = start_x + c * (card_w + col_gap);
        *y = start_y + r * (card_h + row_gap) - 8;
        *w = card_w;
        *h = card_h;
    }

    if (*x < 0)
        *x = 0;
    if (*y < 0)
        *y = 0;
    if (*x + *w > sw)
        *w = sw - *x;
    if (*y + *h > sh)
        *h = sh - *y;
    return 1;
}

static int list_render_background(oops_surface_t *surf, const struct home_model *m,
                                  const struct home_skin *skin) {
    (void)m;
    (void)skin;
    if (surf == 0 || surf->pixels == 0)
        return 0;
    int sw = (int)surf->width;
    int sh = (int)surf->height;

    if (s_view == PS2_VIEW_MAIN) {
        /* Deep midnight blue space gradient */
        oops_draw_rect_gradient(surf, 0, 0, sw, sh, 0xFF010308u, 0xFF050A14u, 1);
    } else {
        /* Smoky slate-to-charcoal gradient */
        oops_draw_rect_gradient(surf, 0, 0, sw, sh, 0xFF4E535Du, 0xFF1B1D22u, 1);
    }
    return 1;
}

static void draw_single_orb(oops_surface_t *surf, int ox, int oy, int z) {
    /* z ranges from -100 to 100 */
    int k = z + 100;           /* 0..200 */
    int r = 4 + (k * 6 / 200); /* 4..10 px radius */

    if (z >= 0) {
        /* Front orb: brilliant halos and pure white center */
        oops_draw_circle_blend(surf, ox, oy, r + 20, 0x18004488u, 1);
        oops_draw_circle_blend(surf, ox, oy, r + 14, 0x350077CCu, 1);
        oops_draw_circle_blend(surf, ox, oy, r + 8, 0x6500AAEEu, 1);
        oops_draw_circle_blend(surf, ox, oy, r + 3, 0x9955EEFFu, 1);
        oops_draw_circle_blend(surf, ox, oy, r, 0xFFFFFFFFu, 1);
    } else {
        /* Back orb: softer halo, semi-translucent */
        oops_draw_circle_blend(surf, ox, oy, r + 12, 0x12004488u, 1);
        oops_draw_circle_blend(surf, ox, oy, r + 6, 0x250066BBu, 1);
        oops_draw_circle_blend(surf, ox, oy, r + 2, 0x450099DDu, 1);
        oops_draw_circle_blend(surf, ox, oy, r, 0x90BBDDFFu, 1);
    }
}

static int list_render_main(oops_surface_t *surf, const struct home_model *m,
                            const struct home_skin *skin) {
    if (surf == 0 || surf->pixels == 0 || m == 0 || skin == 0)
        return 0;
    int drawn = 0;
    int sw = (int)surf->width;
    int sh = (int)surf->height;
    const home_theme_t *theme = &skin->theme;

    if (s_view == PS2_VIEW_MAIN) {
        /* ---- View 1: Seven Orbs Main Menu
         * --------------------------------------------- */
        int sphere_cx = sw * 38 / 100;
        int sphere_cy = sh * 48 / 100;
        int rx = sw * 13 / 100;
        int ry = sh * 14 / 100;

        /* Calculate positions and depths for all 7 orbs */
        typedef struct {
            int x;
            int y;
            int z;
        } orb_pos_t;

        orb_pos_t orbs[7];
        int tilt_cos = fast_cos_deg(-22);
        int tilt_sin = fast_sin_deg(-22);

        for (int i = 0; i < 7; i++) {
            int angle = (i * 360 / 7 + s_orb_tick * 2) % 360;
            int raw_x = (rx * fast_cos_deg(angle)) / 100;
            int raw_y = (ry * fast_sin_deg(angle)) / 100;

            /* Rotate by tilt angle */
            int px = (raw_x * tilt_cos - raw_y * tilt_sin) / 100;
            int py = (raw_x * tilt_sin + raw_y * tilt_cos) / 100;
            int pz = fast_sin_deg(angle);

            orbs[i].x = sphere_cx + px;
            orbs[i].y = sphere_cy + py;
            orbs[i].z = pz;
        }

        /* 1. Draw back orbs (Z < 0) */
        for (int i = 0; i < 7; i++) {
            if (orbs[i].z < 0) {
                draw_single_orb(surf, orbs[i].x, orbs[i].y, orbs[i].z);
                drawn += 4;
            }
        }

        /* 2. Draw central celestial energy core & halo */
        oops_draw_circle_blend(surf, sphere_cx, sphere_cy, rx * 85 / 100, 0x0A001833u,
                               1);
        oops_draw_circle_blend(surf, sphere_cx, sphere_cy, rx * 65 / 100, 0x14002555u,
                               1);
        oops_draw_circle_blend(surf, sphere_cx, sphere_cy, rx * 45 / 100, 0x24004488u,
                               1);
        oops_draw_circle_blend(surf, sphere_cx, sphere_cy, rx * 30 / 100, 0x380077CCu,
                               1);
        oops_draw_circle_blend(surf, sphere_cx, sphere_cy, rx * 18 / 100, 0x5500AADDu,
                               1);
        oops_draw_circle_blend(surf, sphere_cx, sphere_cy, rx * 10 / 100, 0x7533DDFFu,
                               1);
        oops_draw_circle_blend(surf, sphere_cx, sphere_cy, rx * 5 / 100, 0xA5FFFFFFu,
                               1);
        drawn += 7;

        /* Swirling orbital light strands connecting the orbs */
        for (int i = 0; i < 7; i++) {
            int next = (i + 1) % 7;
            oops_draw_line_blend(surf, orbs[i].x, orbs[i].y, orbs[next].x, orbs[next].y,
                                 0x220077BBu);
            drawn++;
        }

        /* 3. Draw front orbs (Z >= 0) */
        for (int i = 0; i < 7; i++) {
            if (orbs[i].z >= 0) {
                draw_single_orb(surf, orbs[i].x, orbs[i].y, orbs[i].z);
                drawn += 5;
            }
        }

        /* 4. Right-side Main Menu Options */
        int menu_x = sphere_cx + rx + (sw > 1600 ? 70 : 45);
        int menu_y = sphere_cy - (sh > 900 ? 30 : 20);
        int menu_gap = (sh > 900 ? 55 : 38);
        int text_scale = (sw > 1600) ? 2 : ((sw > 1200) ? 2 : 1);

        /* Option 0: Browser */
        {
            const char *lbl = "Browser";
            int tw = oops_draw_text_width(lbl, text_scale);
            if (s_main_cursor == 0) {
                oops_draw_rect_blend(surf, menu_x - 12, menu_y - 6, tw + 24,
                                     (text_scale * 8) + 12, 0x25006699u);
                oops_draw_rect_blend(surf, menu_x - 6, menu_y - 3, tw + 12,
                                     (text_scale * 8) + 6, 0x3800B4D8u);
                (void)oops_draw_text(surf, menu_x, menu_y, lbl, 0xFF00E5FFu,
                                     text_scale);
            } else {
                (void)oops_draw_text(surf, menu_x, menu_y, lbl, 0xFFD0D6E0u,
                                     text_scale);
            }
            drawn += 3;
        }

        /* Option 1: System Configuration */
        {
            const char *lbl = "System Configuration";
            int tw = oops_draw_text_width(lbl, text_scale);
            int opt_y = menu_y + menu_gap;
            if (s_main_cursor == 1) {
                oops_draw_rect_blend(surf, menu_x - 12, opt_y - 6, tw + 24,
                                     (text_scale * 8) + 12, 0x25006699u);
                oops_draw_rect_blend(surf, menu_x - 6, opt_y - 3, tw + 12,
                                     (text_scale * 8) + 6, 0x3800B4D8u);
                (void)oops_draw_text(surf, menu_x, opt_y, lbl, 0xFF00E5FFu, text_scale);
            } else {
                (void)oops_draw_text(surf, menu_x, opt_y, lbl, 0xFFD0D6E0u, text_scale);
            }
            drawn += 3;
        }

        /* 5. Bottom Navigation Legend */
        int by = sh - (sh > 900 ? 60 : 42);
        int lx = sw / 2 - (sw > 1600 ? 160 : 120);
        (void)oops_draw_text(surf, lx, by, "[SELECT] Enter    [OPTIONS] Options",
                             0xFFD0D6E0u, text_scale > 1 ? 2 : 1);
        drawn++;

    } else {
        /* ---- View 2: Memory Card Browser
         * ---------------------------------------------- */
        int margin_x = sw * 5 / 100;
        int margin_y = sh * 6 / 100;

        /* 1. Top-Left Storage Information */
        (void)oops_draw_text(surf, margin_x + 2, margin_y + 2, "Storage / 1",
                             0xFF000000u, 2);
        (void)oops_draw_text(surf, margin_x, margin_y, "Storage / 1", 0xFFFFFFFFu, 2);
        drawn += 2;

        char free_buf[64];
        int free_gb = (m->storage.free_gb > 0) ? m->storage.free_gb : 482;
        oops_snprintf(free_buf, sizeof(free_buf), "%d GBytes Free", free_gb);
        int sub_y = margin_y + (sw > 1600 ? 30 : 22);
        (void)oops_draw_text(surf, margin_x + 2, sub_y + 2, free_buf, 0xFF000000u, 1);
        (void)oops_draw_text(surf, margin_x, sub_y, free_buf, 0xFFD8DCE4u, 1);
        drawn += 2;

        /* 2. Top-Right Gold/Yellow Active Title Header with Drop Shadow */
        if (m->title_count > 0 && m->title_cursor >= 0 &&
            m->title_cursor < m->title_count) {
            const home_title_t *t = &m->titles[m->title_cursor];
            if (t->name && t->name[0] != '\0') {
                char line1[64];
                char line2[64];
                line1[0] = '\0';
                line2[0] = '\0';
                size_t nlen = obs_strlen(t->name);
                if (nlen > 18) {
                    int split_idx = -1;
                    for (int k = 0; k < (int)nlen && k < 24; k++) {
                        if (t->name[k] == ' ')
                            split_idx = k;
                    }
                    if (split_idx > 4) {
                        int p = 0;
                        for (p = 0; p < split_idx && p < (int)sizeof(line1) - 1; p++)
                            line1[p] = t->name[p];
                        line1[p] = '\0';
                        int q = 0;
                        for (p = split_idx + 1;
                             p < (int)nlen && q < (int)sizeof(line2) - 1; p++, q++)
                            line2[q] = t->name[p];
                        line2[q] = '\0';
                    } else {
                        oops_snprintf(line1, sizeof(line1), "%s", t->name);
                    }
                } else {
                    oops_snprintf(line1, sizeof(line1), "%s", t->name);
                }

                int right_x_base = sw - margin_x;
                int t_y = margin_y;
                int w1 = oops_draw_text_width(line1, 2);
                (void)oops_draw_text(surf, right_x_base - w1 + 2, t_y + 2, line1,
                                     0xFF000000u, 2);
                (void)oops_draw_text(surf, right_x_base - w1, t_y, line1, 0xFFFFDE59u,
                                     2);
                drawn += 2;

                if (line2[0] != '\0') {
                    int w2 = oops_draw_text_width(line2, 2);
                    (void)oops_draw_text(surf, right_x_base - w2 + 2, t_y + 26 + 2,
                                         line2, 0xFF000000u, 2);
                    (void)oops_draw_text(surf, right_x_base - w2, t_y + 26, line2,
                                         0xFFFFDE59u, 2);
                    drawn += 2;
                }
            }
        }

        /* 3. 5-Column 3D Floating Icon Matrix */
        int total = m->title_count;
        int cursor = m->title_cursor;
        int cursor_row = (total > 0 && cursor >= 0) ? (cursor / 5) : 0;
        int start_row = (cursor_row > 1) ? (cursor_row - 1) : 0;

        int card_w = (sw > 1600) ? 140 : 100;
        int card_h = card_w;
        int col_gap = (sw - 2 * margin_x - 5 * card_w) / 4;
        if (col_gap < 16)
            col_gap = 16;
        int row_gap = (sh > 900) ? 56 : 38;
        int total_grid_w = 5 * card_w + 4 * col_gap;
        int start_x = (sw - total_grid_w) / 2;
        int start_y = (sh > 900) ? 270 : 170;

        if (total == 0) {
            (void)oops_draw_text(surf, sw / 2 - 80, sh / 2, "NO TITLES FOUND",
                                 0xFF808894u, 2);
            drawn++;
        }

        for (int i = 0; i < total; i++) {
            int r = (i / 5) - start_row;
            int c = i % 5;
            if (r < 0 || r >= 3)
                continue;

            int cx = start_x + c * (card_w + col_gap);
            int cy = start_y + r * (card_h + row_gap);
            int selected = (i == cursor);

            /* Selected card floats slightly higher in 3D */
            if (selected) {
                cy -= (sw > 1600 ? 12 : 8);
            }

            /* Floor Drop Shadow */
            int shadow_base_y =
                start_y + r * (card_h + row_gap) + card_h + (sw > 1600 ? 14 : 10);
            int shadow_w = card_w * 7 / 10;
            int shadow_h = (sw > 1600 ? 14 : 10);
            int shadow_x = cx + (card_w - shadow_w) / 2;

            if (selected) {
                oops_draw_rect_blend(surf, shadow_x - 4, shadow_base_y, shadow_w + 8,
                                     shadow_h, 0x40000000u);
                oops_draw_rect_blend(surf, shadow_x, shadow_base_y + 2, shadow_w,
                                     shadow_h - 4, 0x55000000u);
            } else {
                oops_draw_rect_blend(surf, shadow_x, shadow_base_y, shadow_w, shadow_h,
                                     0x55000000u);
                oops_draw_rect_blend(surf, shadow_x + 4, shadow_base_y + 2,
                                     shadow_w - 8, shadow_h - 4, 0x44000000u);
            }
            drawn += 2;

            /* Radiant White Spotlight Flare behind active item */
            if (selected) {
                int spot_cx = cx + card_w / 2;
                int spot_cy = cy + card_h / 2;
                oops_draw_circle_blend(surf, spot_cx, spot_cy, card_w * 90 / 100,
                                       0x15FFFFFFu, 1);
                oops_draw_circle_blend(surf, spot_cx, spot_cy, card_w * 72 / 100,
                                       0x28FFFFFFu, 1);
                oops_draw_circle_blend(surf, spot_cx, spot_cy, card_w * 54 / 100,
                                       0x45FFFFFFu, 1);
                oops_draw_circle_blend(surf, spot_cx, spot_cy, card_w * 38 / 100,
                                       0x70FFFFFFu, 1);
                oops_draw_circle_blend(surf, spot_cx, spot_cy, card_w * 24 / 100,
                                       0xAAFFFFFFu, 1);
                oops_draw_circle_blend(surf, spot_cx, spot_cy, card_w * 14 / 100,
                                       0xDDFFFFFFu, 1);
                drawn += 6;
            }

            /* Floating Card Bevel / Container */
            oops_draw_rect_blend(surf, cx + 4, cy + 4, card_w - 8, card_h - 8,
                                 0x50181A20u);
            if (selected) {
                oops_draw_rect(surf, cx + 2, cy + 2, card_w - 4, card_h - 4,
                               0xFFFFFFFFu);
                oops_draw_rect(surf, cx + 3, cy + 3, card_w - 6, card_h - 6,
                               0x88A0E0FFu);
            } else {
                oops_draw_rect_blend(surf, cx + 3, cy + 3, card_w - 6, card_h - 6,
                                     0x30FFFFFFu);
            }
            drawn += 2;

            /* Title Icon */
            const home_title_t *title = &m->titles[i];
            home_draw_title_icon(surf, title, cx + 8, cy + 8, card_w - 16, card_h - 16,
                                 theme);
            drawn++;

            /* Badges */
            if (title->favorite) {
                (void)oops_draw_text(surf, cx + 10, cy + 10, "[*]", 0xFFFFDE59u, 1);
                drawn++;
            }
            if (m->switcher.has_running_title && m->switcher.running_title_index == i) {
                (void)oops_draw_text(surf, cx + 10, cy + card_h - 20, "[RUN]",
                                     0xFF00FF88u, 1);
                drawn++;
            }
        }

        /* 4. Bottom Navigation Legend */
        int by = sh - (sh > 900 ? 54 : 38);
        (void)oops_draw_text(surf, margin_x, by,
                             "[SELECT] Enter    [BACK] Back    [OPTIONS] Options",
                             0xFFD0D6E0u, sw > 1600 ? 2 : 1);
        drawn++;
    }

    return drawn;
}

const home_skin_t g_skin_list = {
    .id = "list",
    .name = "MEMCARD",
    .author = "OOPS Clean-Room",
    .description = "Seven Orbs & Memory Card Browser",
    .theme = {.name = "MEMCARD",
              .background = 0xFF020409u,
              .accent = 0xFF00E5FFu,
              .text = 0xFFEDEFF3u,
              .text_dim = 0xFF767C88u,
              .cursor = 0xFF00E5FFu,
              .panel = 0xFF1C1E24u,
              .column_width = 180,
              .spine_width = 48,
              .tile_width = 110,
              .tile_height = 110,
              .row_height = 24,
              .margin_x = 40,
              .margin_y = 40,
              .text_scale = 2,
              .cursor_style = HOME_CURSOR_NONE},
    .categories = {{"card", "BROWSER", HOME_CAT_GAMES, 0},
                   {"settings", "SYSTEM CONFIGURATION", HOME_CAT_SETTINGS, 0}},
    .category_count = 2,
    .default_category = 0,
    .primary_axis = HOME_AXIS_NONE,
    .item_axis = HOME_AXIS_NONE,
    .wrap_categories = 0,
    .wrap_items = 0,
    .bumper_nav = 0,
    .init = list_init,
    .tick = list_tick,
    .move = list_move,
    .activate = list_activate,
    .back = list_back,
    .item_count = 0,
    .get_cursor_rect = list_cursor_rect,
    .render_main = list_render_main,
    .render_background = list_render_background};
