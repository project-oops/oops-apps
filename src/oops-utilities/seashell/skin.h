#ifndef OOPS_APPS_SKIN_H
#define OOPS_APPS_SKIN_H

#include "oops/draw.h"
#include "oops/input.h"

/* Forward declarations */
struct home_model;
struct home_title;
struct home_item;
struct home_skin;

#define HOME_MAX_SKIN_CATEGORIES 8

/* Semantic category types */
typedef enum home_category_type {
    HOME_CAT_GAMES = 0,       /* Installed title list */
    HOME_CAT_MEDIA = 1,       /* Media applications */
    HOME_CAT_SETTINGS = 2,    /* Settings categories */
    HOME_CAT_USERS = 3,       /* User profiles & power */
    HOME_CAT_PHOTO = 4,       /* Captures & screenshots */
    HOME_CAT_MUSIC = 5,       /* Music player */
    HOME_CAT_NETWORK = 6,     /* Net tool / network info */
    HOME_CAT_CUSTOM = 7       /* Skin-defined custom items */
} home_category_type_t;

/* Navigation axis mapping */
typedef enum home_axis {
    HOME_AXIS_NONE = 0,
    HOME_AXIS_HORIZONTAL = 1,
    HOME_AXIS_VERTICAL = 2
} home_axis_t;

/* Visual cursor style */
typedef enum home_cursor_style {
    HOME_CURSOR_BAR = 0,
    HOME_CURSOR_BOX = 1,
    HOME_CURSOR_UNDERLINE = 2,
    HOME_CURSOR_NONE = 3
} home_cursor_style_t;

/* One category declared by a skin */
typedef struct home_skin_category {
    const char *id;           /* Short mnemonic: "game", "video", "settings", "live" */
    const char *label;        /* Display text: "GAME", "VIDEO", "SETTINGS", "NETWORK" */
    home_category_type_t type;
    int default_cursor;
} home_skin_category_t;

/* Visual theme metrics & color palette */
typedef struct home_theme {
    const char *name;
    oops_color_t background;
    oops_color_t accent;
    oops_color_t text;
    oops_color_t text_dim;
    oops_color_t cursor;
    oops_color_t panel;
    int column_width;
    int spine_width;
    int tile_width;
    int tile_height;
    int row_height;
    int margin_x;
    int margin_y;
    int text_scale;
    home_cursor_style_t cursor_style;
} home_theme_t;

/* Skin definition structure */
typedef struct home_skin {
    const char *id;           /* Unique skin identifier: "modern", "xmb", "blades", "list", "amber" */
    const char *name;         /* Human readable display name */
    const char *author;       /* Skin author */
    const char *description;  /* Overview description */

    home_theme_t theme;

    /* Category declarations */
    home_skin_category_t categories[HOME_MAX_SKIN_CATEGORIES];
    int category_count;
    int default_category;

    /* Declarative navigation rules */
    home_axis_t primary_axis; /* Axis that rotates categories (e.g. HORIZONTAL for XMB/Blades) */
    home_axis_t item_axis;    /* Axis that scrolls items within category (e.g. VERTICAL for XMB/Blades) */
    int wrap_categories;      /* 1 = wrap around, 0 = clamp */
    int wrap_items;           /* 1 = wrap around, 0 = clamp */
    int bumper_nav;           /* 1 = L1/R1 switches categories */

    /* Lifecycle hooks */
    void (*init)(struct home_model *m, const struct home_skin *skin);
    void (*tick)(struct home_model *m, const struct home_skin *skin);

    /* Navigation / Action hooks (return 1 if handled, 0 to use generic engine fallback) */
    int (*move)(struct home_model *m, const struct home_skin *skin, int dir);
    int (*activate)(struct home_model *m, const struct home_skin *skin);
    int (*back)(struct home_model *m, const struct home_skin *skin);

    /* Item count & query (optional, return -1 to use default category provider) */
    int (*item_count)(const struct home_model *m, const struct home_skin *skin, int cat_idx);

    /* Cursor bounding box */
    int (*get_cursor_rect)(const struct home_model *m, const struct home_skin *skin,
                           int *x, int *y, int *w, int *h);

    /* Rendering hooks */
    int (*render_main)(oops_surface_t *surf, const struct home_model *m, const struct home_skin *skin);
    int (*render_background)(oops_surface_t *surf, const struct home_model *m, const struct home_skin *skin);
} home_skin_t;

/* Skin Registry API */
int home_skin_count(void);
const home_skin_t *home_skin_at(int index);
const home_skin_t *home_skin_find(const char *id);

/* Shared Skin Helpers (provided by home.c) */
void home_draw_title_icon(oops_surface_t *surf, const struct home_title *title,
                          int x, int y, int w, int h, const home_theme_t *theme);
void home_draw_cursor(oops_surface_t *surf, int x, int y, int w, int h,
                      const home_theme_t *theme);
int  home_get_category_item_count(const struct home_model *m, const struct home_skin *skin,
                                  int cat_idx);
void home_get_category_item_info(const struct home_model *m, const struct home_skin *skin,
                                 int cat_idx, int item_idx,
                                 const char **out_name, const char **out_sub,
                                 const struct home_title **out_title, char *out_badge);
int  home_category_item_activate(struct home_model *m, const struct home_skin *skin,
                                 int cat_idx, int item_idx);
int  home_generic_move(struct home_model *m, const struct home_skin *skin, int dir);

#endif /* OOPS_APPS_SKIN_H */

