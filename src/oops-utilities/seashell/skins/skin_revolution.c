#include "../skin.h"
#include "../home.h"
#include "oops/freestd.h"

/*
 * Revolution skin - Clean-room channel grid dashboard featuring:
 * - 4x3 grid of 12 rounded title channels populated from installed titles
 * - Channel card rendering with icon, title name, and status badges
 * - Soft patterned channel background on empty channels
 * - Right-edge cyan page arrow ('>')
 * - Bottom console swoop with 3D spherical System button, Storage slot,
 *   large digital clock/calendar, and Message (envelope) button
 * - Full 2D grid D-pad navigation + bottom dock navigation
 */

#define REV_GRID_COLS 4
#define REV_GRID_ROWS 3
#define REV_GRID_TOTAL (REV_GRID_COLS * REV_GRID_ROWS)
#define REV_BTN_SYSTEM 12
#define REV_BTN_SD     13
#define REV_BTN_MAIL   14

/* Draw a rounded channel card frame */
static void draw_channel_card(oops_surface_t *surf, int x, int y, int w, int h,
                              int selected, oops_color_t border_col) {
    /* Subtle shadow beneath card */
    oops_draw_rect_blend(surf, x + 3, y + h, w - 6, 4, 0x18000000u);
    oops_draw_rect_blend(surf, x + w, y + 3, 3, h - 3, 0x10000000u);

    /* Card background gradient (glossy white to soft silver) */
    oops_draw_rect_gradient(surf, x, y, w, h, 0xFFFFFFFFu, 0xFFEDEDEDu, 1);

    /* Outer border */
    oops_draw_line(surf, x + 4, y, x + w - 5, y, border_col);
    oops_draw_line(surf, x + 4, y + h - 1, x + w - 5, y + h - 1, border_col);
    oops_draw_line(surf, x, y + 4, x, y + h - 5, border_col);
    oops_draw_line(surf, x + w - 1, y + 4, x + w - 1, y + h - 5, border_col);

    /* Corner bevels */
    oops_draw_pixel(surf, x + 1, y + 2, border_col);
    oops_draw_pixel(surf, x + 2, y + 1, border_col);
    oops_draw_pixel(surf, x + w - 2, y + 2, border_col);
    oops_draw_pixel(surf, x + w - 3, y + 1, border_col);
    oops_draw_pixel(surf, x + 1, y + h - 3, border_col);
    oops_draw_pixel(surf, x + 2, y + h - 2, border_col);
    oops_draw_pixel(surf, x + w - 2, y + h - 3, border_col);
    oops_draw_pixel(surf, x + w - 3, y + h - 2, border_col);

    /* Inner specular highlight line */
    oops_draw_line_blend(surf, x + 2, y + 1, x + w - 3, y + 1, 0x90FFFFFFu);
    oops_draw_line_blend(surf, x + 1, y + 2, x + 1, y + h - 3, 0x50FFFFFFu);

    /* Selected state: glowing cyan double border */
    if (selected) {
        oops_color_t glow = 0xFF00A0E9u;
        oops_draw_rect(surf, x - 2, y - 2, w + 4, 2, glow);
        oops_draw_rect(surf, x - 2, y + h, w + 4, 2, glow);
        oops_draw_rect(surf, x - 2, y - 2, 2, h + 4, glow);
        oops_draw_rect(surf, x + w, y - 2, 2, h + 4, glow);

        oops_draw_rect_blend(surf, x - 4, y - 4, w + 8, 2, 0x4000A0E9u);
        oops_draw_rect_blend(surf, x - 4, y + h + 2, w + 8, 2, 0x4000A0E9u);
        oops_draw_rect_blend(surf, x - 4, y - 4, 2, h + 8, 0x4000A0E9u);
        oops_draw_rect_blend(surf, x + w + 2, y - 4, 2, h + 8, 0x4000A0E9u);
    }
}

/* Draw soft scanline texture on blank/unpopulated channels */
static void draw_static_channel(oops_surface_t *surf, int x, int y, int w, int h) {
    /* Base soft gray card */
    oops_draw_rect(surf, x + 2, y + 2, w - 4, h - 4, 0xFFECEEF2u);

    /* Subtle scanlines */
    for (int ly = y + 4; ly < y + h - 4; ly += 3) {
        oops_draw_line(surf, x + 4, ly, x + w - 5, ly, 0xFFE2E4EAu);
    }
    for (int sx = x + 10; sx < x + w - 10; sx += 16) {
        for (int sy = y + 8; sy < y + h - 8; sy += 12) {
            oops_draw_pixel(surf, sx, sy, 0xFFCAD0DAu);
        }
    }

    /* Faint embossed watermark */
    int wx = x + (w / 2) - 20;
    int wy = y + (h / 2) - 8;
    (void)oops_draw_text(surf, wx, wy, "OOPS", 0xFFB8C0CCu, 2);
}

/* Background renderer: soft platinum gradient */
static int revolution_render_background(oops_surface_t *surf, const struct home_model *m,
                                        const struct home_skin *skin) {
    (void)m;
    (void)skin;
    int sw = (int)surf->width;
    int sh = (int)surf->height;

    /* Light metallic platinum gradient (pure CPU cache-friendly vertical gradient) */
    oops_draw_rect_gradient(surf, 0, 0, sw, sh, 0xFFF5F7FAu, 0xFFDEE2E8u, 1);

    return 1;
}

/* Main channel grid & bottom console bar renderer */
static int revolution_render_main(oops_surface_t *surf, const struct home_model *m,
                                  const struct home_skin *skin) {
    int drawn = 0;
    const home_theme_t *theme = &skin->theme;
    int sw = (int)surf->width;
    int sh = (int)surf->height;

    int cw = (sw >= 1920) ? 390 : 260;
    int ch = (sw >= 1920) ? 230 : 152;
    int gap_x = (sw >= 1920) ? 34 : 22;
    int gap_y = (sw >= 1920) ? 30 : 20;
    int grid_w = (REV_GRID_COLS * cw) + ((REV_GRID_COLS - 1) * gap_x);
    int start_x = (sw - grid_w) / 2;
    int start_y = (sw >= 1920) ? 65 : 44;

    int cur = m->category_cursor[m->category_idx];
    int top_foc = (m->top_nav != HOME_TOP_NAV_NONE);

    /* ---- 1. Channel Grid (12 TV Channels) ------------------------------------------------ */
    for (int i = 0; i < REV_GRID_TOTAL; i++) {
        int col = i % REV_GRID_COLS;
        int row = i / REV_GRID_COLS;
        int cx = start_x + (col * (cw + gap_x));
        int cy = start_y + (row * (ch + gap_y));
        int selected = (i == cur && !top_foc);

        oops_color_t border_col = selected ? theme->accent : 0xFFBAC0CAu;
        draw_channel_card(surf, cx, cy, cw, ch, selected, border_col);
        drawn += 4;

        if (i < m->title_count) {
            const home_title_t *t = &m->titles[i];
            int pad = 4;
            int tx = cx + pad;
            int ty = cy + pad;
            int tw = cw - (pad * 2);
            int th = ch - (pad * 2);

            /* Stretch container to fill the full tile */
            home_draw_title_icon(surf, t, tx, ty, tw, th, theme);

            /* Subtle dark bottom gradient for crisp text legibility */
            int label_h = (sw >= 1920) ? 28 : 24;
            oops_draw_rect_gradient(surf, tx, ty + th - label_h, tw, label_h, 0x00000000u, 0xD0000000u, 1);

            char disp[32];
            const char *src = t->name ? t->name : t->id;
            int d = 0;
            while (src && src[d] != '\0' && d < 24) {
                disp[d] = src[d];
                d++;
            }
            disp[d] = '\0';
            (void)oops_draw_text(surf, tx + 8, ty + th - ((sw >= 1920) ? 20 : 18), disp, 0xFFFFFFFFu, 1);

            if (m->switcher.has_running_title && m->switcher.running_title_index == i) {
                oops_draw_rect_blend(surf, tx + 4, ty + 4, 66, 16, 0xCC00A0E9u);
                (void)oops_draw_text(surf, tx + 6, ty + 6, "RUNNING", 0xFFFFFFFFu, 1);
            } else if (t->favorite) {
                oops_draw_circle_blend(surf, tx + tw - 14, ty + 12, 10, 0xCC000000u, 1);
                (void)oops_draw_text(surf, tx + tw - 20, ty + 6, "[*]", 0xFFFFD700u, 1);
            }
            drawn += 6;
        } else {
            draw_static_channel(surf, cx, cy, cw, ch);
            drawn += 4;
        }
    }

    /* ---- 2. Right Page Flip Arrow ('>') -------------------------------------------------- */
    int arrow_x = start_x + grid_w + ((sw >= 1920) ? 24 : 16);
    int arrow_y = start_y + ch + (gap_y / 2) + ((sw >= 1920) ? 14 : 10);
    if (arrow_x + 24 < sw) {
        /* Cyan triangular arrow */
        oops_draw_line(surf, arrow_x, arrow_y - 16, arrow_x + 16, arrow_y, 0xFF00A0E9u);
        oops_draw_line(surf, arrow_x + 16, arrow_y, arrow_x, arrow_y + 16, 0xFF00A0E9u);
        oops_draw_line(surf, arrow_x, arrow_y - 16, arrow_x, arrow_y + 16, 0xFF00A0E9u);
        oops_draw_line_blend(surf, arrow_x + 2, arrow_y - 12, arrow_x + 12, arrow_y, 0x8000A0E9u);
        drawn += 4;
    }

    /* ---- 3. Bottom Console Bar with Curved Bezel Swoop ----------------------------------- */
    int bottom_y = sh - ((sw >= 1920) ? 145 : 100);

    /* Solid metallic lower plate */
    oops_draw_rect_gradient(surf, 0, bottom_y + 16, sw, sh - (bottom_y + 16),
                            0xFFDEE1E8u, 0xFFCCD1DAu, 1);

    /* Cyan sweeping curve line across bottom */
    for (int x = 0; x < sw; x += 8) {
        int dip = (x < sw / 2) ? ((x * 30) / (sw / 2)) : (((sw - x) * 30) / (sw / 2));
        int sy = bottom_y + 16 + (dip / 2);
        oops_draw_rect(surf, x, sy, 8, 3, 0xFF00A0E9u);
        oops_draw_rect_blend(surf, x, sy + 3, 8, 3, 0x3000A0E9u);
    }
    drawn += 10;

    /* ---- 4. Bottom-Left: 3D Spherical System Button (Slot 12) ---------------------------- */
    int sys_btn_x = (start_x >= 70) ? (start_x - ((sw >= 1920) ? 50 : 36)) : 50;
    int sys_btn_y = sh - ((sw >= 1920) ? 72 : 52);
    int btn_r = (sw >= 1920) ? 42 : 28;
    int sys_sel = (cur == REV_BTN_SYSTEM && !top_foc);

    oops_draw_circle(surf, sys_btn_x, sys_btn_y, btn_r + 2, 0xFFADB3BEu, 1);
    oops_draw_circle(surf, sys_btn_x, sys_btn_y, btn_r, 0xFFE6E9EEu, 1);
    oops_draw_circle(surf, sys_btn_x, sys_btn_y - (btn_r / 4), btn_r / 2, 0x80FFFFFFu, 1);

    if (sys_sel) {
        oops_draw_circle(surf, sys_btn_x, sys_btn_y, btn_r + 3, 0xFF00A0E9u, 0);
        oops_draw_circle(surf, sys_btn_x, sys_btn_y, btn_r + 4, 0x8000A0E9u, 0);
    }
    int sys_scale = 2;
    int sys_tw = oops_draw_text_width("SYS", sys_scale);
    int sys_tx = sys_btn_x - (sys_tw / 2);
    int sys_ty = sys_btn_y - (sys_scale * 8 / 2);
    (void)oops_draw_text(surf, sys_tx, sys_ty, "SYS",
                         sys_sel ? 0xFF0078D7u : 0xFF545C6Au, sys_scale);
    drawn += 6;

    /* SD Card slot icon (Slot 13) */
    int sd_x = sys_btn_x + btn_r + ((sw >= 1920) ? 26 : 20);
    int sd_y = sys_btn_y - 14;
    int sd_sel = (cur == REV_BTN_SD && !top_foc);

    oops_draw_rect(surf, sd_x, sd_y, 22, 28, 0xFFFFFFFFu);
    oops_draw_line(surf, sd_x, sd_y, sd_x + 16, sd_y, 0xFF9AA0AAu);
    oops_draw_line(surf, sd_x + 16, sd_y, sd_x + 22, sd_y + 6, 0xFF9AA0AAu);
    oops_draw_line(surf, sd_x + 22, sd_y + 6, sd_x + 22, sd_y + 28, 0xFF9AA0AAu);
    oops_draw_line(surf, sd_x, sd_y + 28, sd_x + 22, sd_y + 28, 0xFF9AA0AAu);
    oops_draw_line(surf, sd_x, sd_y, sd_x, sd_y + 28, 0xFF9AA0AAu);
    (void)oops_draw_text(surf, sd_x + 4, sd_y + 10, "SD",
                         sd_sel ? 0xFF0078D7u : 0xFF7A828Eu, 1);
    if (sd_sel) {
        oops_draw_rect(surf, sd_x - 2, sd_y - 2, 26, 32, 0xFF00A0E9u);
    }
    drawn += 6;

    /* ---- 5. Bottom-Center: Large Digital Clock & Date ------------------------------------ */
    char clk_buf[32];
    int hr = m->status.hour;
    int mn = m->status.minute;
    int is_pm = (hr >= 12);
    int disp_hr = hr % 12;
    if (disp_hr == 0) disp_hr = 12;
    oops_snprintf(clk_buf, sizeof(clk_buf), "%d:%02d %s", disp_hr, mn, is_pm ? "PM" : "AM");

    int clk_scale = (sw >= 1920) ? 3 : 2;
    int clk_w = oops_draw_text_width(clk_buf, clk_scale);
    int clk_x = (sw - clk_w) / 2;
    int clk_y = bottom_y + ((sw >= 1920) ? 50 : 34);
    (void)oops_draw_text(surf, clk_x, clk_y, clk_buf, 0xFF3E444Eu, clk_scale);

    /* Date display: "Fri 18/9" */
    char date_buf[32];
    oops_snprintf(date_buf, sizeof(date_buf), "Fri 18/9");
    int dt_w = oops_draw_text_width(date_buf, 1);
    (void)oops_draw_text(surf, (sw - dt_w) / 2, clk_y + (clk_scale * 8) + 4, date_buf, 0xFF767F8Cu, 1);
    drawn += 2;

    /* ---- 6. Bottom-Right: 3D Message Board (Envelope) Button (Slot 14) ------------------- */
    int mail_btn_x = (start_x >= 70) ? (sw - (start_x - ((sw >= 1920) ? 50 : 36))) : (sw - 50);
    int mail_btn_y = sys_btn_y;
    int mail_sel = (cur == REV_BTN_MAIL && !top_foc);

    oops_draw_circle(surf, mail_btn_x, mail_btn_y, btn_r + 2, 0xFFADB3BEu, 1);
    oops_draw_circle(surf, mail_btn_x, mail_btn_y, btn_r, 0xFFE6E9EEu, 1);
    oops_draw_circle(surf, mail_btn_x, mail_btn_y - (btn_r / 4), btn_r / 2, 0x80FFFFFFu, 1);

    if (mail_sel) {
        oops_draw_circle(surf, mail_btn_x, mail_btn_y, btn_r + 3, 0xFF00A0E9u, 0);
        oops_draw_circle(surf, mail_btn_x, mail_btn_y, btn_r + 4, 0x8000A0E9u, 0);
    }

    /* Envelope icon */
    int env_w = (sw >= 1920) ? 34 : 26;
    int env_h = (sw >= 1920) ? 22 : 18;
    int env_x = mail_btn_x - (env_w / 2);
    int env_y = mail_btn_y - (env_h / 2);
    oops_color_t env_col = mail_sel ? 0xFF0078D7u : 0xFF7A828Eu;

    oops_draw_rect(surf, env_x, env_y, env_w, env_h, env_col);
    oops_draw_rect(surf, env_x + 1, env_y + 1, env_w - 2, env_h - 2, 0xFFFFFFFFu);
    oops_draw_line(surf, env_x, env_y, env_x + (env_w / 2), env_y + (env_h / 2), env_col);
    oops_draw_line(surf, env_x + env_w, env_y, env_x + (env_w / 2), env_y + (env_h / 2), env_col);

    /* Unread notification indicator */
    if (m->status.notifications > 0) {
        oops_draw_circle(surf, mail_btn_x + (btn_r / 2), mail_btn_y - (btn_r / 2), 6, 0xFF00A0E9u, 1);
    }
    drawn += 6;

    return drawn;
}

/* 2D grid navigation: handles 4x3 channel grid + bottom console controls */
static int revolution_move(struct home_model *m, const struct home_skin *skin, int dir) {
    (void)skin;
    int cur = m->category_cursor[m->category_idx];

    if (dir == HOME_UP) {
        if (cur >= REV_BTN_SYSTEM) {
            /* From bottom dock back into channel grid row 2 */
            if (cur == REV_BTN_SYSTEM) {
                m->category_cursor[m->category_idx] = 8; /* Row 2, Col 0 */
            } else if (cur == REV_BTN_SD) {
                m->category_cursor[m->category_idx] = 9; /* Row 2, Col 1 */
            } else {
                m->category_cursor[m->category_idx] = 11; /* Row 2, Col 3 */
            }
        } else {
            int row = cur / REV_GRID_COLS;
            int col = cur % REV_GRID_COLS;
            if (row > 0) {
                m->category_cursor[m->category_idx] = (row - 1) * REV_GRID_COLS + col;
            } else {
                /* Wrap to bottom dock */
                m->category_cursor[m->category_idx] = (col >= 2) ? REV_BTN_MAIL : REV_BTN_SYSTEM;
            }
        }
    } else if (dir == HOME_DOWN) {
        if (cur < REV_GRID_TOTAL) {
            int row = cur / REV_GRID_COLS;
            int col = cur % REV_GRID_COLS;
            if (row < REV_GRID_ROWS - 1) {
                m->category_cursor[m->category_idx] = (row + 1) * REV_GRID_COLS + col;
            } else {
                /* Drop from Row 2 down to bottom console bar */
                if (col == 0) m->category_cursor[m->category_idx] = REV_BTN_SYSTEM;
                else if (col == 1) m->category_cursor[m->category_idx] = REV_BTN_SD;
                else m->category_cursor[m->category_idx] = REV_BTN_MAIL;
            }
        }
    } else if (dir == HOME_LEFT) {
        if (cur >= REV_BTN_SYSTEM) {
            if (cur == REV_BTN_MAIL) m->category_cursor[m->category_idx] = REV_BTN_SD;
            else if (cur == REV_BTN_SD) m->category_cursor[m->category_idx] = REV_BTN_SYSTEM;
            else m->category_cursor[m->category_idx] = REV_BTN_MAIL;
        } else {
            int row = cur / REV_GRID_COLS;
            int col = cur % REV_GRID_COLS;
            col = (col > 0) ? (col - 1) : (REV_GRID_COLS - 1);
            m->category_cursor[m->category_idx] = (row * REV_GRID_COLS) + col;
        }
    } else if (dir == HOME_RIGHT) {
        if (cur >= REV_BTN_SYSTEM) {
            if (cur == REV_BTN_SYSTEM) m->category_cursor[m->category_idx] = REV_BTN_SD;
            else if (cur == REV_BTN_SD) m->category_cursor[m->category_idx] = REV_BTN_MAIL;
            else m->category_cursor[m->category_idx] = REV_BTN_SYSTEM;
        } else {
            int row = cur / REV_GRID_COLS;
            int col = cur % REV_GRID_COLS;
            col = (col < REV_GRID_COLS - 1) ? (col + 1) : 0;
            m->category_cursor[m->category_idx] = (row * REV_GRID_COLS) + col;
        }
    }

    /* Keep title cursor in sync when on title channels */
    cur = m->category_cursor[m->category_idx];
    if (cur >= 0 && cur < REV_GRID_TOTAL && cur < m->title_count) {
        m->title_cursor = cur;
    }
    return 1;
}

/* Activation hook: maps channels to launcher subsystems */
static int revolution_activate(struct home_model *m, const struct home_skin *skin) {
    (void)skin;
    int cur = m->category_cursor[m->category_idx];

    switch (cur) {
        case REV_BTN_SYSTEM: /* System Settings */
            home_open(m, HOME_SCREEN_SETTINGS);
            return 1;

        case REV_BTN_SD: /* Storage Manager */
            home_open(m, HOME_SCREEN_SETTINGS_STORAGE);
            return 1;

        case REV_BTN_MAIL: /* Notifications */
            home_open(m, HOME_SCREEN_NOTIFICATIONS);
            return 1;

        default: /* Installed titles (slots 0..11) */
            if (cur >= 0 && cur < REV_GRID_TOTAL) {
                if (cur < m->title_count) {
                    m->selected_title = cur;
                    m->title_cursor = cur;
                    home_open(m, HOME_SCREEN_TITLE_OPTIONS);
                    return 1;
                }
            }
            home_show_toast(m, "CHANNELS", "CHANNEL UNPOPULATED");
            return 1;
    }
}

/* Calculate focused cursor bounding rectangle */
static int revolution_cursor_rect(const struct home_model *m, const struct home_skin *skin,
                                  int *x, int *y, int *w, int *h) {
    (void)skin;
    int sw = 1280;
    int sh = 720;
    int cw = 260;
    int ch = 152;
    int gap_x = 22;
    int gap_y = 20;
    int grid_w = (REV_GRID_COLS * cw) + ((REV_GRID_COLS - 1) * gap_x);
    int start_x = (sw - grid_w) / 2;
    int start_y = 44;

    int cur = m->category_cursor[m->category_idx];

    if (cur < REV_GRID_TOTAL) {
        int col = cur % REV_GRID_COLS;
        int row = cur / REV_GRID_COLS;
        *x = start_x + (col * (cw + gap_x));
        *y = start_y + (row * (ch + gap_y));
        *w = cw;
        *h = ch;
    } else if (cur == REV_BTN_SYSTEM) {
        int sys_btn_x = start_x - 36;
        int sys_btn_y = sh - 52;
        int btn_r = 28;
        *x = sys_btn_x - btn_r;
        *y = sys_btn_y - btn_r;
        *w = btn_r * 2;
        *h = btn_r * 2;
    } else if (cur == REV_BTN_SD) {
        int sd_x = (start_x - 36) + 28 + 20;
        int sd_y = sh - 52 - 14;
        *x = sd_x;
        *y = sd_y;
        *w = 22;
        *h = 28;
    } else {
        int mail_btn_x = sw - (start_x - 36);
        int mail_btn_y = sh - 52;
        int btn_r = 28;
        *x = mail_btn_x - btn_r;
        *y = mail_btn_y - btn_r;
        *w = btn_r * 2;
        *h = btn_r * 2;
    }
    return 1;
}

const home_skin_t g_skin_revolution = {
    .id = "revolution",
    .name = "REVOLUTION",
    .author = "OOPS Clean-Room",
    .description = "4x3 channel grid with bottom console bar",
    .theme = {
        .name = "REVOLUTION",
        .background = 0xFFF5F7FAu,
        .accent = 0xFF00A0E9u,
        .text = 0xFF2C3138u,
        .text_dim = 0xFF8A939Eu,
        .cursor = 0xFF00A0E9u,
        .panel = 0xFFFFFFFFu,
        .column_width = 240,
        .spine_width = 56,
        .tile_width = 240,
        .tile_height = 110,
        .row_height = 24,
        .margin_x = 48,
        .margin_y = 36,
        .text_scale = 2,
        .cursor_style = HOME_CURSOR_BOX
    },
    .categories = {
        { "menu", "CHANNELS", HOME_CAT_GAMES, 0 }
    },
    .category_count = 1,
    .default_category = 0,
    .primary_axis = HOME_AXIS_NONE,
    .item_axis = HOME_AXIS_NONE,
    .wrap_categories = 0,
    .wrap_items = 1,
    .bumper_nav = 0,
    .init = 0,
    .tick = 0,
    .move = revolution_move,
    .activate = revolution_activate,
    .item_count = 0,
    .get_cursor_rect = revolution_cursor_rect,
    .render_main = revolution_render_main,
    .render_background = revolution_render_background
};
