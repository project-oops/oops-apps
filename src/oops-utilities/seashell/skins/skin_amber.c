#include "../skin.h"
#include "../home.h"

extern const home_skin_t g_skin_xmb;

static int amber_render_main(oops_surface_t *surf, const struct home_model *m,
                             const struct home_skin *skin) {
    if (g_skin_xmb.render_main) {
        return g_skin_xmb.render_main(surf, m, skin);
    }
    return 0;
}

static int amber_cursor_rect(const struct home_model *m, const struct home_skin *skin,
                            int *x, int *y, int *w, int *h) {
    if (g_skin_xmb.get_cursor_rect) {
        return g_skin_xmb.get_cursor_rect(m, skin, x, y, w, h);
    }
    return 0;
}

const home_skin_t g_skin_amber = {
    .id = "amber",
    .name = "AMBER",
    .author = "OOPS Team",
    .description = "Warm amber aesthetic Cross Media Bar with golden ribbons",
    .theme = {
        .name = "AMBER",
        .background = 0xFF120C04u,
        .accent = 0xFFFFB13Cu,
        .text = 0xFFFFE9C7u,
        .text_dim = 0xFF8A6A38u,
        .cursor = 0xFFFFB13Cu,
        .panel = 0xFF1E1408u,
        .column_width = 220,
        .spine_width = 52,
        .tile_width = 120,
        .tile_height = 120,
        .row_height = 30,
        .margin_x = 56,
        .margin_y = 64,
        .text_scale = 2,
        .cursor_style = HOME_CURSOR_UNDERLINE
    },
    .categories = {
        { "users", "USERS", HOME_CAT_USERS, 0 },
        { "settings", "SETTINGS", HOME_CAT_SETTINGS, 0 },
        { "photo", "PHOTO", HOME_CAT_PHOTO, 0 },
        { "music", "MUSIC", HOME_CAT_MUSIC, 0 },
        { "video", "VIDEO", HOME_CAT_MEDIA, 0 },
        { "game", "GAME", HOME_CAT_GAMES, 0 },
        { "network", "NETWORK", HOME_CAT_NETWORK, 0 }
    },
    .category_count = 7,
    .default_category = 5, /* GAME */
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
    .get_cursor_rect = amber_cursor_rect,
    .render_main = amber_render_main,
    .render_background = 0
};

