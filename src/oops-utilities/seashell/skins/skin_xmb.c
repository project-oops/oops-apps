#include "../skin.h"
#include "../home.h"
#include "oops/freestd.h"

/* Dynamic particle and wave state for authentic XMB feel */
#define XMB_PARTICLE_COUNT 48

typedef struct xmb_particle {
    int x;
    int y;
    int speed_x;
    int speed_y;
    int size;
    oops_color_t color;
} xmb_particle_t;

static int s_wave_phase = 0;
static xmb_particle_t s_particles[XMB_PARTICLE_COUNT];
static int s_particles_init = 0;

static uint32_t s_lcg_seed = 0x12345678u;
static uint32_t xmb_rand(void) {
    s_lcg_seed = s_lcg_seed * 1103515245u + 12345u;
    return (s_lcg_seed >> 16) & 0x7FFFu;
}

static void init_particles(int sw, int sh) {
    for (int i = 0; i < XMB_PARTICLE_COUNT; i++) {
        s_particles[i].x = (int)(xmb_rand() % (uint32_t)sw);
        s_particles[i].y = (int)(xmb_rand() % (uint32_t)sh);
        s_particles[i].speed_x = (int)(xmb_rand() % 2) + 1;
        s_particles[i].speed_y = (int)(xmb_rand() % 3) - 1;
        s_particles[i].size = (int)(xmb_rand() % 3) + 1;
        uint32_t alpha = (xmb_rand() % 120) + 40;
        s_particles[i].color = (alpha << 24) | 0x00A0D0FFu;
    }
    s_particles_init = 1;
}

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
    /* Parabolic approximation: 4 * d * (180 - d) / 40500 */
    return (4 * deg * (180 - deg)) / 405; /* 0..100 */
}

static void draw_xmb_category_icon(oops_surface_t *surf, int cat, int cx, int cy,
                                   int is_active, oops_color_t col) {
    int s = is_active ? 1 : 0;
    switch (cat) {
    case 0: /* USERS */
        oops_draw_circle_blend(surf, cx, cy - (s ? 8 : 6), s ? 7 : 5, col, 1);
        oops_draw_rect(surf, cx - (s ? 12 : 9), cy + (s ? 3 : 1), s ? 24 : 18,
                       s ? 10 : 8, col);
        break;
    case 1: /* SETTINGS */
        oops_draw_rect(surf, cx - (s ? 12 : 9), cy - (s ? 4 : 3), s ? 24 : 18,
                       s ? 16 : 12, col);
        oops_draw_rect(surf, cx - (s ? 5 : 4), cy - (s ? 9 : 7), s ? 10 : 8, 3, col);
        break;
    case 2: /* PHOTO */
        oops_draw_rect(surf, cx - (s ? 13 : 10), cy - (s ? 5 : 4), s ? 26 : 20,
                       s ? 16 : 12, col);
        oops_draw_circle_blend(surf, cx, cy + (s ? 3 : 2), s ? 5 : 4,
                               0xFF000000u | (col & 0x00FFFFFFu), 0);
        oops_draw_rect(surf, cx - (s ? 8 : 6), cy - (s ? 8 : 7), s ? 5 : 4, 3, col);
        break;
    case 3: /* MUSIC */
        oops_draw_line_blend(surf, cx - 4, cy + 4, cx - 4, cy - 8, col);
        oops_draw_line_blend(surf, cx + 5, cy + 1, cx + 5, cy - 11, col);
        oops_draw_line_blend(surf, cx - 4, cy - 8, cx + 5, cy - 11, col);
        oops_draw_circle_blend(surf, cx - 6, cy + 4, s ? 4 : 3, col, 1);
        oops_draw_circle_blend(surf, cx + 3, cy + 1, s ? 4 : 3, col, 1);
        break;
    case 4: /* VIDEO */
        oops_draw_rect(surf, cx - (s ? 12 : 9), cy - (s ? 8 : 6), s ? 24 : 18,
                       s ? 18 : 14, col);
        oops_draw_rect(surf, cx - (s ? 7 : 5), cy - (s ? 3 : 2), s ? 14 : 10, s ? 8 : 6,
                       0xFF000000u);
        break;
    case 5: /* GAME */
        oops_draw_rect(surf, cx - (s ? 14 : 10), cy - (s ? 6 : 4), s ? 28 : 20,
                       s ? 14 : 10, col);
        oops_draw_rect(surf, cx - (s ? 10 : 7), cy - (s ? 2 : 1), s ? 6 : 4, 2,
                       0xFF000000u);
        oops_draw_rect(surf, cx + (s ? 4 : 3), cy - (s ? 2 : 1), s ? 6 : 4, 2,
                       0xFF000000u);
        break;
    case 6: /* NETWORK */
    default:
        oops_draw_circle_blend(surf, cx, cy, s ? 9 : 7,
                               0xFF000000u | (col & 0x00FFFFFFu), 0);
        oops_draw_line_blend(surf, cx - (s ? 9 : 7), cy, cx + (s ? 9 : 7), cy, col);
        oops_draw_line_blend(surf, cx, cy - (s ? 9 : 7), cx, cy + (s ? 9 : 7), col);
        break;
    }
}

static void xmb_tick(struct home_model *m, const struct home_skin *skin) {
    (void)m;
    (void)skin;
    s_wave_phase = (s_wave_phase + 1) % 360;
}

static int xmb_render_main(oops_surface_t *surf, const struct home_model *m,
                           const struct home_skin *skin) {
    int drawn = 0;
    const home_theme_t *theme = &skin->theme;
    int sw = (int)surf->width;
    int sh = (int)surf->height;

    if (!s_particles_init) {
        init_particles(sw, sh);
    }

    /* ---- 1. Dynamic Sine-Wave Ribbons & Ambient Particles
     * -------------------------------- */
    int rib_y_base = sh / 2;
    for (int w = 0; w < 3; w++) {
        int phase_offset = s_wave_phase + (w * 45);
        int prev_y = rib_y_base;
        for (int x = 0; x < sw; x += 16) {
            int deg = ((x * 360) / sw) + phase_offset;
            int sin_val = fast_sin_deg(deg);
            int cur_y = rib_y_base + ((sin_val * (60 + (w * 25))) / 100) - (w * 30);
            if (x > 0) {
                uint32_t a = (w == 0) ? 0x2A : ((w == 1) ? 0x1C : 0x12);
                oops_color_t rcol = (a << 24) | (theme->accent & 0x00FFFFFFu);
                oops_draw_line_blend(surf, x - 16, prev_y, x, cur_y, rcol);
                oops_draw_line_blend(surf, x - 16, prev_y + 1, x, cur_y + 1, rcol);
            }
            prev_y = cur_y;
        }
        drawn += 4;
    }

    /* Particles */
    for (int p = 0; p < XMB_PARTICLE_COUNT; p++) {
        s_particles[p].x += s_particles[p].speed_x;
        s_particles[p].y += s_particles[p].speed_y;
        if (s_particles[p].x >= sw)
            s_particles[p].x = 0;
        if (s_particles[p].y < 0)
            s_particles[p].y = sh - 1;
        if (s_particles[p].y >= sh)
            s_particles[p].y = 0;

        oops_draw_rect_blend(surf, s_particles[p].x, s_particles[p].y,
                             s_particles[p].size, s_particles[p].size,
                             s_particles[p].color);
        drawn++;
    }

    /* ---- 2. Top-Right Status Clock, Battery & Profile
     * ----------------------------------- */
    int top_y = 36;
    int rx = sw - 320;
    (void)oops_draw_text(surf, rx, top_y,
                         (m->status.user && m->status.user[0]) ? m->status.user
                                                               : "USER 1",
                         theme->text, 1);
    int bat_x = rx + 110;
    oops_draw_rect(surf, bat_x, top_y + 1, 22, 11, theme->text_dim);
    oops_draw_rect(surf, bat_x + 22, top_y + 4, 2, 5, theme->text_dim);
    oops_draw_rect(surf, bat_x + 2, top_y + 3, 16, 7, theme->accent);

    char clk[32];
    int hr = m->status.hour;
    int mn = m->status.minute;
    if (hr == 0 && mn == 0) {
        hr = 20;
        mn = 31;
    }
    oops_snprintf(clk, sizeof(clk), "29/9  %02d:%02d", hr, mn);
    (void)oops_draw_text(surf, bat_x + 36, top_y, clk, theme->text, 1);
    drawn += 4;

    /* ---- 3. Horizontal Category Row
     * ----------------------------------------------------- */
    int focus_cat_x = 360;
    int cat_row_y = 200;
    int active_cat = m->category_idx;

    for (int c = 0; c < skin->category_count; c++) {
        int cx = focus_cat_x + ((c - active_cat) * 110);
        if (cx < -40 || cx > sw + 40)
            continue;

        int is_act = (c == active_cat);
        oops_color_t col = is_act ? 0xFFFFFFFFu : theme->text_dim;

        draw_xmb_category_icon(surf, c, cx, cat_row_y, is_act, col);

        if (is_act) {
            const char *lbl = skin->categories[c].label;
            int len = 0;
            while (lbl[len] != '\0')
                len++;
            int tx = cx - (len * 4);
            (void)oops_draw_text(surf, tx, cat_row_y + 26, lbl, 0xFFFFFFFFu, 1);
        }
        drawn += 2;
    }

    oops_draw_line_blend(surf, 40, cat_row_y + 44, sw - 40, cat_row_y + 44,
                         0x20FFFFFFu);

    /* ---- 4. Vertical Item Column Intersecting at focus_cat_x
     * ---------------------------- */
    int total_items = home_get_category_item_count(m, skin, active_cat);
    int cur = m->category_cursor[active_cat];
    int center_item_y = 350;

    for (int i = 0; i < total_items; i++) {
        int iy = center_item_y + ((i - cur) * 60);
        if (iy < 120 || iy > 640)
            continue;

        int selected = (i == cur && m->top_nav == HOME_TOP_NAV_NONE);

        const char *name = "";
        const char *sub = "";
        const struct home_title *title = 0;
        char badge_code = ' ';
        home_get_category_item_info(m, skin, active_cat, i, &name, &sub, &title,
                                    &badge_code);

        if (selected) {
            oops_draw_rect_blend(surf, focus_cat_x - 36, iy - 6, 620, 52,
                                 (theme->accent & 0x00FFFFFFu) | 0x28000000u);
            oops_draw_rect(surf, focus_cat_x - 36, iy - 6, 3, 52, theme->accent);

            if (title != 0) {
                home_draw_title_icon(surf, title, focus_cat_x - 24, iy, 42, 42, theme);
            } else {
                oops_draw_rect(surf, focus_cat_x - 24, iy, 42, 42, theme->panel);
                draw_xmb_category_icon(surf, active_cat, focus_cat_x - 3, iy + 21, 1,
                                       0xFFFFFFFFu);
            }

            char disp_name[128];
            if (title && title->favorite) {
                oops_snprintf(disp_name, sizeof(disp_name), "[*] %s", name);
            } else {
                oops_snprintf(disp_name, sizeof(disp_name), "%s", name);
            }
            (void)oops_draw_text(surf, focus_cat_x + 32, iy + 4, disp_name, 0xFFFFFFFFu,
                                 2);
            (void)oops_draw_text(surf, focus_cat_x + 32, iy + 26, sub, theme->accent,
                                 1);
        } else {
            if (title != 0) {
                home_draw_title_icon(surf, title, focus_cat_x - 16, iy + 6, 30, 30,
                                     theme);
            } else {
                oops_draw_rect(surf, focus_cat_x - 16, iy + 6, 30, 30, theme->panel);
                draw_xmb_category_icon(surf, active_cat, focus_cat_x - 1, iy + 21, 0,
                                       theme->text_dim);
            }
            (void)oops_draw_text(surf, focus_cat_x + 32, iy + 12, name, theme->text_dim,
                                 1);
        }
        drawn += 4;
    }

    /* ---- 5. Bottom Controller Button Guide
     * ---------------------------------------------- */
    int foot_y = sh - 46;
    (void)oops_draw_text(surf, 60, foot_y,
                         "[SELECT] ENTER / PLAY      [BACK] BACK      [OPTIONS] "
                         "OPTIONS      [NAV] LIBRARY",
                         theme->text_dim, 1);
    drawn++;

    return drawn;
}

static int xmb_cursor_rect(const struct home_model *m, const struct home_skin *skin,
                           int *x, int *y, int *w, int *h) {
    (void)skin;
    int cur = m->category_cursor[m->category_idx];
    *x = 360 - 36;
    *y = 350 + ((cur - cur) * 60) - 6; /* Active item is always at 350-6 */
    *w = 620;
    *h = 52;
    return 1;
}

const home_skin_t g_skin_xmb = {
    .id = "xmb",
    .name = "XMB",
    .author = "OOPS Clean-Room",
    .description = "Cross Media Bar with dynamic particle ribbons",
    .theme = {.name = "XMB",
              .background = 0xFF0B1020u,
              .accent = 0xFF4C8DFFu,
              .text = 0xFFF0F4FFu,
              .text_dim = 0xFF7A85A8u,
              .cursor = 0xFF4C8DFFu,
              .panel = 0xFF161C30u,
              .column_width = 200,
              .spine_width = 56,
              .tile_width = 120,
              .tile_height = 120,
              .row_height = 28,
              .margin_x = 48,
              .margin_y = 56,
              .text_scale = 2,
              .cursor_style = HOME_CURSOR_BAR},
    .categories = {{"users", "USERS", HOME_CAT_USERS, 0},
                   {"settings", "SETTINGS", HOME_CAT_SETTINGS, 0},
                   {"photo", "PHOTO", HOME_CAT_PHOTO, 0},
                   {"music", "MUSIC", HOME_CAT_MUSIC, 0},
                   {"video", "VIDEO", HOME_CAT_MEDIA, 0},
                   {"game", "GAME", HOME_CAT_GAMES, 0},
                   {"network", "NETWORK", HOME_CAT_NETWORK, 0}},
    .category_count = 7,
    .default_category = 5, /* GAME */
    .primary_axis = HOME_AXIS_HORIZONTAL,
    .item_axis = HOME_AXIS_VERTICAL,
    .wrap_categories = 1,
    .wrap_items = 1,
    .bumper_nav = 1,
    .init = 0,
    .tick = xmb_tick,
    .move = 0,
    .activate = 0,
    .item_count = 0,
    .get_cursor_rect = xmb_cursor_rect,
    .render_main = xmb_render_main,
    .render_background = 0};
