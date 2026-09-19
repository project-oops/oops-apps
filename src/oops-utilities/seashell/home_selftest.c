/*
 * home - the host self-test.
 *
 * Validates the full clean-room Prospero shell reimplementation headlessly:
 * - Shell model state transitions, mode switching (Games <-> Media), top-bar navigation
 * - Control Centre 13-dock quick menu & Switcher lifecycle (running app resume / close)
 * - Deep settings trees (System, Storage visual meter, Developer & Debug, Emulator)
 * - Common Dialogs subsystem (Confirm prompt, Progress bar modal, Virtual IME keyboard, Error modal)
 * - Toast notification pop-up timer & auto-dismissal
 * - Renderer verification across all themes/layouts into an in-memory 1280x720 surface
 *
 * Runs anywhere without a console, display, or physical controller.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "home.h"

/* Link stubs for SDK display queries */
uint32_t *oops_display_get_framebuffer(oops_display_t *disp) { (void)disp; return NULL; }
unsigned int oops_display_get_width(const oops_display_t *disp) { (void)disp; return 0; }
unsigned int oops_display_get_height(const oops_display_t *disp) { (void)disp; return 0; }

#define HOME_TEST_W 1280
#define HOME_TEST_H 720

static uint32_t s_pixels[HOME_TEST_W * HOME_TEST_H];

static oops_surface_t test_surface(void) {
    oops_surface_t surf;
    surf.pixels = s_pixels;
    surf.width = (uint32_t)HOME_TEST_W;
    surf.height = (uint32_t)HOME_TEST_H;
    surf.pitch = (uint32_t)HOME_TEST_W;
    return surf;
}

static uint32_t pixel_at(const oops_surface_t *surf, int x, int y) {
    if (x < 0 || y < 0 || x >= (int)surf->width || y >= (int)surf->height) {
        return 0u;
    }
    size_t index = ((size_t)y * (size_t)surf->pitch) + (size_t)x;
    return surf->pixels[index];
}

/* Host action recorder */
typedef struct recorder {
    int calls;
    home_action_t last;
    int last_arg;
    int accept;
} recorder_t;

static int record_action(void *ctx, home_action_t action, int arg) {
    recorder_t *rec = (recorder_t *)ctx;
    rec->calls++;
    rec->last = action;
    rec->last_arg = arg;
    return rec->accept;
}

static int select_label(home_model_t *m, const char *label) {
    home_menu_t *menu = home_current_menu(m);
    if (menu == 0) {
        return 0;
    }
    for (int i = 0; i < menu->count; i++) {
        if (menu->items[i].label != 0 && strcmp(menu->items[i].label, label) == 0) {
            menu->cursor = i;
            return 1;
        }
    }
    return 0;
}

static const home_theme_t *theme_with_layout(home_layout_t layout) {
    const char *id = "modern";
    if (layout == HOME_LAYOUT_XMB) id = "xmb";
    else if (layout == HOME_LAYOUT_LIST) id = "list";
    else if (layout == HOME_LAYOUT_BLADES) id = "blades";
    else if (layout == HOME_LAYOUT_TILES) id = "modern";
    const home_skin_t *skin = home_skin_find(id);
    return skin ? &skin->theme : 0;
}

int main(void) {
    unsigned int failures = 0;
    home_model_t m;

    /* ---- 1. Shell model & realistic defaults ------------------------------------------ */

    home_model_init(&m);
    if (home_screen(&m) != HOME_SCREEN_GAMES) {
        printf("FAIL: a fresh shell must open on the Games home screen\n");
        failures++;
    }
    if (m.title_count <= 0 || home_selected_title(&m) == 0) {
        printf("FAIL: the default library must be populated with titles\n");
        failures++;
    }
    if (strcmp(home_selected_title(&m)->name, "ASTRO'S PLAYROOM") != 0) {
        printf("FAIL: default active title should be ASTRO'S PLAYROOM\n");
        failures++;
    }
    if (m.media_count <= 0) {
        printf("FAIL: default media apps must be populated\n");
        failures++;
    }
    if (m.card_count != 13) {
        printf("FAIL: control centre must have all 13 Prospero dock items, found %d\n", m.card_count);
        failures++;
    }
    if (m.activity_count != 0) {
        printf("FAIL: activity cards must not contain placeholder data\n");
        failures++;
    }

    /* ---- 2. Carousel navigation -------------------------------------------------------- */

    home_model_init(&m);
    home_move(&m, HOME_LEFT);
    if (m.title_cursor != m.title_count - 1) {
        printf("FAIL: left from the first tile must wrap to the last\n");
        failures++;
    }
    home_move(&m, HOME_RIGHT);
    if (m.title_cursor != 0) {
        printf("FAIL: right from the last tile must wrap to the first\n");
        failures++;
    }

    /* Top bar navigation */
    home_move(&m, HOME_UP);
    if (m.top_nav != HOME_TOP_NAV_TABS) {
        printf("FAIL: up from carousel must enter top bar tabs\n");
        failures++;
    }
    home_move(&m, HOME_RIGHT);
    if (m.top_nav != HOME_TOP_NAV_SEARCH) {
        printf("FAIL: right on top bar should move to search\n");
        failures++;
    }
    home_move(&m, HOME_DOWN);
    if (m.top_nav != HOME_TOP_NAV_NONE) {
        printf("FAIL: down from top bar must return focus to carousel\n");
        failures++;
    }

    /* Mode switching (Games <-> Media) */
    home_switch_mode(&m, HOME_MODE_MEDIA);
    if (m.mode != HOME_MODE_MEDIA || home_screen(&m) != HOME_SCREEN_MEDIA) {
        printf("FAIL: switching to media mode must change active screen to media\n");
        failures++;
    }
    if (strcmp(home_selected_title(&m)->name, "MEDIA PLAYER (USB & LOCAL)") != 0) {
        printf("FAIL: media mode selected title is incorrect\n");
        failures++;
    }
    home_switch_mode(&m, HOME_MODE_GAMES);
    if (m.mode != HOME_MODE_GAMES || home_screen(&m) != HOME_SCREEN_GAMES) {
        printf("FAIL: switching back to games mode failed\n");
        failures++;
    }

    /* ---- 3. Screen stack & deep menus -------------------------------------------------- */

    home_model_init(&m);
    home_open(&m, HOME_SCREEN_SETTINGS);
    home_open(&m, HOME_SCREEN_SETTINGS_SYSTEM);
    if (home_screen(&m) != HOME_SCREEN_SETTINGS_SYSTEM) {
        printf("FAIL: opening system settings failed\n");
        failures++;
    }
    if (home_back(&m) == 0 || home_screen(&m) != HOME_SCREEN_SETTINGS) {
        printf("FAIL: backing out of system settings must return to settings\n");
        failures++;
    }
    (void)home_back(&m);
    if (home_screen(&m) != HOME_SCREEN_GAMES) {
        printf("FAIL: backing out of settings must return to games home\n");
        failures++;
    }

    /* ---- 4. Title Options & Information ------------------------------------------------ */

    home_model_init(&m);
    (void)home_activate(&m); /* Activating tile opens Title Options */
    if (home_screen(&m) != HOME_SCREEN_TITLE_OPTIONS) {
        printf("FAIL: activating a tile must open its options menu\n");
        failures++;
    }
    if (select_label(&m, "INFORMATION") == 0) {
        printf("FAIL: title options menu must include INFORMATION\n");
        failures++;
    } else {
        (void)home_activate(&m);
        if (home_screen(&m) != HOME_SCREEN_TITLE_INFO) {
            printf("FAIL: activating INFORMATION must open title info screen\n");
            failures++;
        }
    }

    /* ---- 5. Control Centre & Switcher -------------------------------------------------- */

    home_model_init(&m);
    home_toggle_control_centre(&m);
    if (home_screen(&m) != HOME_SCREEN_CONTROL) {
        printf("FAIL: home_toggle_control_centre must open Control Centre\n");
        failures++;
    }
    /* Second toggle closes Control Centre */
    home_toggle_control_centre(&m);
    if (home_screen(&m) != HOME_SCREEN_GAMES) {
        printf("FAIL: home_toggle_control_centre must close Control Centre\n");
        failures++;
    }

    /* Switcher screen navigation */
    home_open(&m, HOME_SCREEN_SWITCHER);
    const home_menu_t *sw_menu = home_current_menu_const(&m);
    if (sw_menu == 0 || sw_menu->count < 2) {
        printf("FAIL: Switcher menu must contain idle status and actions\n");
        failures++;
    }
    m.switcher.has_running_title = 1;
    m.switcher.running_title_index = 0;
    home_open(&m, HOME_SCREEN_SWITCHER);
    sw_menu = home_current_menu_const(&m);
    if (sw_menu == 0 || sw_menu->count < 3) {
        printf("FAIL: Switcher menu with active game must contain running title and actions\n");
        failures++;
    }

    /* ---- 6. Common Dialogs & IME Keyboard ---------------------------------------------- */

    /* Confirm Dialog */
    home_model_init(&m);
    home_show_dialog(&m, HOME_DIALOG_CONFIRM, "CONFIRM DELETE", "ARE YOU SURE?");
    if (m.dialog.type != HOME_DIALOG_CONFIRM) {
        printf("FAIL: confirm dialog did not activate\n");
        failures++;
    }
    home_move(&m, HOME_RIGHT); /* Switch to OK */
    if (m.dialog.confirm_choice != 1) {
        printf("FAIL: right on confirm dialog should select OK\n");
        failures++;
    }
    (void)home_activate(&m);
    if (m.dialog.type != HOME_DIALOG_NONE) {
        printf("FAIL: activating confirm choice should close dialog\n");
        failures++;
    }

    /* Virtual Keyboard (IME) */
    home_model_init(&m);
    home_show_dialog(&m, HOME_DIALOG_IME, "SEARCH", "KEYBOARD");
    m.dialog.ime_row = 1; /* 'Q' */
    m.dialog.ime_col = 0;
    (void)home_activate(&m);
    if (m.dialog.ime_len != 1 || m.dialog.ime_buffer[0] != 'Q') {
        printf("FAIL: typing 'Q' on IME keyboard failed\n");
        failures++;
    }
    /* Move to backspace key (row 4, col 4) */
    m.dialog.ime_row = 4;
    m.dialog.ime_col = 4;
    (void)home_activate(&m);
    if (m.dialog.ime_len != 0) {
        printf("FAIL: backspace on IME keyboard failed\n");
        failures++;
    }
    home_close_dialog(&m);

    /* Error Dialog */
    home_show_error(&m, "CE-108255-1", "THE APPLICATION CRASHED");
    if (m.dialog.type != HOME_DIALOG_ERROR || strcmp(m.dialog.error_code, "CE-108255-1") != 0) {
        printf("FAIL: error dialog formatting failed\n");
        failures++;
    }
    (void)home_activate(&m);
    if (m.dialog.type != HOME_DIALOG_NONE) {
        printf("FAIL: dismissing error dialog failed\n");
        failures++;
    }

    /* ---- 7. Toast Notification Timer --------------------------------------------------- */

    home_model_init(&m);
    home_show_toast(&m, "DOWNLOAD READY", "PATCH 1.003");
    if (m.toast.active != 1 || m.toast.frames_left <= 0) {
        printf("FAIL: toast notification did not activate\n");
        failures++;
    }
    for (int f = 0; f < 200; f++) {
        home_tick(&m);
    }
    if (m.toast.active != 0) {
        printf("FAIL: toast notification did not auto-dismiss after timer\n");
        failures++;
    }

    /* ---- 8. Host Seam & Actions --------------------------------------------------------- */

    home_model_init(&m);
    {
        recorder_t rec;
        rec.calls = 0;
        rec.last = HOME_ACTION_NONE;
        rec.last_arg = -1;
        rec.accept = 1;
        home_host_t host;
        host.ctx = &rec;
        host.perform = record_action;
        home_set_host(&m, &host);

        home_open(&m, HOME_SCREEN_SETTINGS_DEVELOPER);
        if (select_label(&m, "INSTALL PACKAGE (PKG)") == 0) {
            printf("FAIL: developer settings must offer INSTALL PACKAGE (PKG)\n");
            failures++;
        } else {
            (void)home_activate(&m);
            if (rec.last != HOME_ACTION_INSTALL_PACKAGE) {
                printf("FAIL: INSTALL PACKAGE did not reach the host\n");
                failures++;
            }
        }
    }

    /* ---- 9. Pad Input Mapping (Real Prospero Controls) ---------------------------------- */

    home_model_init(&m);
    {
        home_input_t in;
        home_input_reset(&in);
        (void)home_input_apply(&in, &m, 0u);

        /* PS Button toggles Control Centre */
        (void)home_input_apply(&in, &m, HOME_BUTTON_PS);
        if (home_screen(&m) != HOME_SCREEN_CONTROL) {
            printf("FAIL: PS button must toggle Control Centre\n");
            failures++;
        }
        (void)home_input_apply(&in, &m, 0u);
        (void)home_input_apply(&in, &m, HOME_BUTTON_PS);
        if (home_screen(&m) != HOME_SCREEN_GAMES) {
            printf("FAIL: second PS button press must close Control Centre\n");
            failures++;
        }

        /* L1 / R1 mode switching */
        (void)home_input_apply(&in, &m, 0u);
        (void)home_input_apply(&in, &m, OOPS_BUTTON_R1);
        if (m.mode != HOME_MODE_MEDIA) {
            printf("FAIL: R1 button must switch to Media mode\n");
            failures++;
        }
        (void)home_input_apply(&in, &m, 0u);
        (void)home_input_apply(&in, &m, OOPS_BUTTON_L1);
        if (m.mode != HOME_MODE_GAMES) {
            printf("FAIL: L1 button must switch to Games mode\n");
            failures++;
        }

        /* Square opens Library */
        (void)home_input_apply(&in, &m, 0u);
        (void)home_input_apply(&in, &m, OOPS_BUTTON_SQUARE);
        if (home_screen(&m) != HOME_SCREEN_LIBRARY) {
            printf("FAIL: Square button must open Game Library\n");
            failures++;
        }

        /* Circle backs out */
        (void)home_input_apply(&in, &m, 0u);
        (void)home_input_apply(&in, &m, OOPS_BUTTON_CIRCLE);
        if (home_screen(&m) != HOME_SCREEN_GAMES) {
            printf("FAIL: Circle button must back out to Games home\n");
            failures++;
        }

        /* Triangle opens Search */
        (void)home_input_apply(&in, &m, 0u);
        (void)home_input_apply(&in, &m, OOPS_BUTTON_TRIANGLE);
        if (home_screen(&m) != HOME_SCREEN_SEARCH) {
            printf("FAIL: Triangle button must open Search\n");
            failures++;
        }

        /* Progressive repeat acceleration: initial press moves immediately, hold triggers repeat */
        home_model_init(&m);
        home_input_reset(&in);
        (void)home_input_apply(&in, &m, 0u); /* prime input */

        int start_cur = m.title_cursor;
        (void)home_input_apply(&in, &m, OOPS_BUTTON_RIGHT);
        if (m.title_cursor != start_cur + 1) {
            printf("FAIL: initial RIGHT press should advance title cursor\n");
            failures++;
        }

        /* Hold for frames before HOME_REPEAT_DELAY_FRAMES (18) */
        for (int f = 1; f < HOME_REPEAT_DELAY_FRAMES; f++) {
            (void)home_input_apply(&in, &m, OOPS_BUTTON_RIGHT);
        }
        if (m.title_cursor != start_cur + 1) {
            printf("FAIL: holding RIGHT should not repeat before delay threshold\n");
            failures++;
        }

        /* Frame 18: first repeat triggered */
        (void)home_input_apply(&in, &m, OOPS_BUTTON_RIGHT);
        if (m.title_cursor != start_cur + 2) {
            printf("FAIL: holding RIGHT at delay frame should repeat cursor to %d (got %d)\n",
                   start_cur + 2, m.title_cursor);
            failures++;
        }
    }

    /* ---- 10. Renderer across all screens and themes ------------------------------------- */

    home_model_init(&m);
    if (home_render(0, &m, home_theme_at(0)) != 0) {
        printf("FAIL: rendering with no surface must return 0\n");
        failures++;
    }

    for (int t = 0; t < home_theme_count(); t++) {
        const home_theme_t *theme = home_theme_at(t);
        for (int s = 0; s < HOME_SCREEN_COUNT; s++) {
            oops_surface_t surf = test_surface();
            home_model_init(&m);
            home_open(&m, (home_screen_t)s);
            if (home_render(&surf, &m, theme) <= 0) {
                printf("FAIL: skin %s drew nothing on screen %d\n", theme->name, s);
                failures++;
                continue;
            }
            int cx = 0, cy = 0, cw = 0, ch = 0;
            if (home_cursor_rect(&m, theme, &cx, &cy, &cw, &ch) != 0) {
                if (cx < 0 || cy < 0 || cx + cw > HOME_TEST_W || cy + ch > HOME_TEST_H) {
                    printf("FAIL: skin %s puts cursor out of bounds on screen %d\n",
                           theme->name, s);
                    failures++;
                }
            }
        }
    }

    /* Render dialog and toast overlay test */
    {
        oops_surface_t surf = test_surface();
        home_model_init(&m);
        home_show_dialog(&m, HOME_DIALOG_CONFIRM, "CONFIRMATION", "PROCEED WITH LAUNCH?");
        home_show_toast(&m, "STATUS", "READY");
        if (home_render(&surf, &m, home_theme_at(0)) <= 0) {
            printf("FAIL: dialog and toast overlay render failed\n");
            failures++;
        }
    }

    /* Test theme layouts and pixel rendering */
    {
        if (theme_with_layout(HOME_LAYOUT_TILES) == 0 ||
            theme_with_layout(HOME_LAYOUT_XMB) == 0 ||
            theme_with_layout(HOME_LAYOUT_LIST) == 0 ||
            theme_with_layout(HOME_LAYOUT_BLADES) == 0) {
            printf("FAIL: missing required layouts in themes\n");
            failures++;
        }
        oops_surface_t surf = test_surface();
        home_model_init(&m);
        (void)home_render(&surf, &m, home_theme_at(0));
        uint32_t bg = pixel_at(&surf, 0, 0);
        if (bg != home_theme_at(0)->background) {
            printf("FAIL: pixel_at origin does not match background\n");
            failures++;
        }

        /* Test all 4 layout rendering branches and scrolling cursor bounds across 32 titles */
        home_title_t titles[32];
        for (int i = 0; i < 32; i++) {
            titles[i].id = "TEST00001";
            titles[i].name = "SAMPLE TITLE";
            titles[i].category = "NATIVE";
            titles[i].version = "1.00";
            titles[i].size_mb = 100;
            titles[i].installed = 1;
            titles[i].minutes_played = 0;
            titles[i].last_played_days_ago = 0;
            titles[i].trophy_unlocked = 0;
            titles[i].trophy_total = 10;
            titles[i].icon_pixels = NULL;
            titles[i].icon_width = 0;
            titles[i].icon_height = 0;
        }

        home_set_titles(&m, titles, 32);
        if (m.title_count != 32) {
            printf("FAIL: expanded title capacity failed to store 32 titles (got %d)\n", m.title_count);
            failures++;
        }

        home_layout_t layouts[4] = {
            HOME_LAYOUT_TILES, HOME_LAYOUT_XMB, HOME_LAYOUT_LIST, HOME_LAYOUT_BLADES
        };

        for (int l = 0; l < 4; l++) {
            const home_theme_t *theme = theme_with_layout(layouts[l]);
            if (theme == 0) continue;

            /* Switch model to corresponding skin */
            const char *skin_id = "modern";
            if (layouts[l] == HOME_LAYOUT_XMB) skin_id = "xmb";
            else if (layouts[l] == HOME_LAYOUT_LIST) skin_id = "list";
            else if (layouts[l] == HOME_LAYOUT_BLADES) skin_id = "blades";
            for (int s = 0; s < home_skin_count(); s++) {
                if (strcmp(home_skin_at(s)->id, skin_id) == 0) {
                    home_set_skin(&m, s);
                    break;
                }
            }

            /* Verify rendering with multiple titles */
            surf = test_surface();
            int drawn = home_render(&surf, &m, theme);
            if (drawn <= 0) {
                printf("FAIL: layout %d (%s) drew nothing on 32-title model\n", layouts[l], theme->name);
                failures++;
            }

            /* Verify cursor rect bounds stay within 1280x720 across all 32 cursor positions */
            for (int c = 0; c < 32; c++) {
                m.category_cursor[m.category_idx] = c;
                m.title_cursor = c;
                int cx = 0, cy = 0, cw = 0, ch = 0;
                if (home_cursor_rect(&m, theme, &cx, &cy, &cw, &ch) == 0) {
                    printf("FAIL: cursor rect returned 0 for layout %d at cursor %d\n", layouts[l], c);
                    failures++;
                } else if (cx < 0 || cy < 0 || cx + cw > HOME_TEST_W || cy + ch > HOME_TEST_H) {
                    printf("FAIL: layout %d (%s) cursor rect out of bounds [%d,%d,%d,%d] at cursor %d\n",
                           layouts[l], theme->name, cx, cy, cw, ch, c);
                    failures++;
                }
            }
        }
    }

    /* ---- XMB Rotated Navigation & Activation -------------------------------------------- */
    {
        home_model_init(&m);
        /* Switch skin to XMB */
        while (strcmp(home_current_skin(&m)->id, "xmb") != 0) {
            home_next_skin(&m);
        }

        /* Default category is 5 (GAME) */
        if (m.category_idx != 5) {
            printf("FAIL: XMB should default to GAME category (5), got %d\n", m.category_idx);
            failures++;
        }

        /* Test Left/Right rotates categories */
        home_move(&m, HOME_LEFT);
        if (m.category_idx != 4) {
            printf("FAIL: XMB left from GAME should move to VIDEO (4), got %d\n", m.category_idx);
            failures++;
        }
        home_move(&m, HOME_RIGHT);
        if (m.category_idx != 5) {
            printf("FAIL: XMB right from VIDEO should move back to GAME (5), got %d\n", m.category_idx);
            failures++;
        }

        /* Test Up/Down scrolls items under active category */
        int init_cur = m.category_cursor[5];
        home_move(&m, HOME_DOWN);
        if (m.category_cursor[5] != init_cur + 1) {
            printf("FAIL: XMB down should scroll items under GAME\n");
            failures++;
        }
        if (m.title_cursor != m.category_cursor[5]) {
            printf("FAIL: XMB title_cursor must stay in sync with category_cursor[GAME]\n");
            failures++;
        }

        /* Test activation on GAME opens Title Options */
        if (home_activate(&m) == 0 || home_screen(&m) != HOME_SCREEN_TITLE_OPTIONS) {
            printf("FAIL: XMB activation under GAME should open title options\n");
            failures++;
        }
        (void)home_back(&m);

        /* Test switching to SETTINGS category and activating an item */
        m.category_idx = 1; /* SETTINGS */
        m.category_cursor[1] = 1; /* Storage */
        if (home_activate(&m) == 0 || home_screen(&m) != HOME_SCREEN_SETTINGS_STORAGE) {
            printf("FAIL: XMB activation under SETTINGS(Storage) should open storage screen\n");
            failures++;
        }
        (void)home_back(&m);
    }

    /* ---- Blades Navigation & Activation ------------------------------------------------- */
    {
        home_model_init(&m);
        /* Switch skin to BLADES */
        while (strcmp(home_current_skin(&m)->id, "blades") != 0) {
            home_next_skin(&m);
        }

        /* Default category is 1 (games) */
        if (m.category_idx != 1) {
            printf("FAIL: Blades should default to GAMES blade (1), got %d\n", m.category_idx);
            failures++;
        }

        /* Test Left/Right switches blades */
        home_move(&m, HOME_LEFT);
        if (m.category_idx != 0) {
            printf("FAIL: Blades left from GAMES should move to LIVE (0), got %d\n", m.category_idx);
            failures++;
        }
        home_move(&m, HOME_RIGHT);
        if (m.category_idx != 1) {
            printf("FAIL: Blades right from LIVE should move back to GAMES (1), got %d\n", m.category_idx);
            failures++;
        }

        /* Test Up/Down scrolls items on active blade */
        int init_bcur = m.category_cursor[1];
        home_move(&m, HOME_DOWN);
        if (m.category_cursor[1] != init_bcur + 1) {
            printf("FAIL: Blades down should scroll items on GAMES blade\n");
            failures++;
        }
        if (m.title_cursor != m.category_cursor[1]) {
            printf("FAIL: Blades title_cursor must stay in sync with category_cursor[GAMES]\n");
            failures++;
        }

        /* Test activating item on GAMES blade opens Title Options */
        if (home_activate(&m) == 0 || home_screen(&m) != HOME_SCREEN_TITLE_OPTIONS) {
            printf("FAIL: Blades activation under GAMES should open title options\n");
            failures++;
        }
        (void)home_back(&m);

        /* Test bumper L1 / R1 navigation via input apply */
        home_input_t input;
        home_input_reset(&input);
        (void)home_input_apply(&input, &m, 0); /* Prime */
        (void)home_input_apply(&input, &m, OOPS_BUTTON_R1);
        if (m.category_idx != 2) {
            printf("FAIL: R1 in Blades layout should shift to MEDIA blade (2), got %d\n", m.category_idx);
            failures++;
        }
        (void)home_input_apply(&input, &m, 0);
        (void)home_input_apply(&input, &m, OOPS_BUTTON_L1);
        if (m.category_idx != 1) {
            printf("FAIL: L1 in Blades layout should shift back to GAMES blade (1), got %d\n", m.category_idx);
            failures++;
        }
    }

    /* ---- Revolution Navigation & Activation --------------------------------------------- */
    {
        home_model_init(&m);
        /* Switch skin to REVOLUTION */
        while (strcmp(home_current_skin(&m)->id, "revolution") != 0) {
            home_next_skin(&m);
        }

        /* Default category is 0 */
        if (m.category_idx != 0) {
            printf("FAIL: Revolution should default to category 0, got %d\n", m.category_idx);
            failures++;
        }

        /* Test 2D navigation across 4x3 grid */
        m.category_cursor[0] = 0;
        home_move(&m, HOME_RIGHT);
        if (m.category_cursor[0] != 1) {
            printf("FAIL: Revolution RIGHT from 0 should be 1, got %d\n", m.category_cursor[0]);
            failures++;
        }
        home_move(&m, HOME_DOWN);
        if (m.category_cursor[0] != 5) {
            printf("FAIL: Revolution DOWN from 1 should be 5, got %d\n", m.category_cursor[0]);
            failures++;
        }
        home_move(&m, HOME_LEFT);
        if (m.category_cursor[0] != 4) {
            printf("FAIL: Revolution LEFT from 5 should be 4, got %d\n", m.category_cursor[0]);
            failures++;
        }
        home_move(&m, HOME_UP);
        if (m.category_cursor[0] != 0) {
            printf("FAIL: Revolution UP from 4 should be 0, got %d\n", m.category_cursor[0]);
            failures++;
        }

        /* Test navigation to bottom dock buttons:
         * Row 2: cols 0,1,2,3 correspond to slots 8, 9, 10, 11
         */
        m.category_cursor[0] = 8;
        home_move(&m, HOME_DOWN);
        if (m.category_cursor[0] != 12) { /* REV_BTN_SYSTEM */
            printf("FAIL: Revolution DOWN from slot 8 should move to System button (12), got %d\n", m.category_cursor[0]);
            failures++;
        }
        /* Dock horizontal navigation */
        home_move(&m, HOME_RIGHT);
        if (m.category_cursor[0] != 13) { /* REV_BTN_SD */
            printf("FAIL: Revolution RIGHT from System button should move to SD (13), got %d\n", m.category_cursor[0]);
            failures++;
        }
        home_move(&m, HOME_RIGHT);
        if (m.category_cursor[0] != 14) { /* REV_BTN_MAIL */
            printf("FAIL: Revolution RIGHT from SD should move to Mail (14), got %d\n", m.category_cursor[0]);
            failures++;
        }
        home_move(&m, HOME_LEFT);
        if (m.category_cursor[0] != 13) { /* Back to SD */
            printf("FAIL: Revolution LEFT from Mail should move to SD (13), got %d\n", m.category_cursor[0]);
            failures++;
        }
        /* Return from dock up to grid */
        home_move(&m, HOME_UP);
        if (m.category_cursor[0] != 9) { /* Returns to slot 9 */
            printf("FAIL: Revolution UP from SD should return to slot 9, got %d\n", m.category_cursor[0]);
            failures++;
        }

        /* Test button activations */
        /* System button opens Settings */
        m.category_cursor[0] = 12;
        if (home_activate(&m) == 0 || home_screen(&m) != HOME_SCREEN_SETTINGS) {
            printf("FAIL: Revolution activate System button should open Settings\n");
            failures++;
        }
        (void)home_back(&m);

        /* SD slot opens Storage */
        m.category_cursor[0] = 13;
        if (home_activate(&m) == 0 || home_screen(&m) != HOME_SCREEN_SETTINGS_STORAGE) {
            printf("FAIL: Revolution activate SD slot should open Storage\n");
            failures++;
        }
        (void)home_back(&m);

        /* Mail button opens Notifications */
        m.category_cursor[0] = 14;
        if (home_activate(&m) == 0 || home_screen(&m) != HOME_SCREEN_NOTIFICATIONS) {
            printf("FAIL: Revolution activate Mail button should open Notifications\n");
            failures++;
        }
        (void)home_back(&m);

        /* Channel 1 (Mii) opens Profile */
        m.category_cursor[0] = 1;
        if (home_activate(&m) == 0 || home_screen(&m) != HOME_SCREEN_PROFILE) {
            printf("FAIL: Revolution activate Mii channel should open Profile\n");
            failures++;
        }
        (void)home_back(&m);

        /* Channel 2 (Photo) opens Captures */
        m.category_cursor[0] = 2;
        if (home_activate(&m) == 0 || home_screen(&m) != HOME_SCREEN_CAPTURES) {
            printf("FAIL: Revolution activate Photo channel should open Captures\n");
            failures++;
        }
        (void)home_back(&m);

        /* Channel 3 (Shop) opens Library */
        m.category_cursor[0] = 3;
        if (home_activate(&m) == 0 || home_screen(&m) != HOME_SCREEN_LIBRARY) {
            printf("FAIL: Revolution activate Shop channel should open Library\n");
            failures++;
        }
        (void)home_back(&m);

        /* Verify rendering full frame in Revolution skin */
        oops_surface_t rsurf = test_surface();
        int rdrawn = home_render(&rsurf, &m, &home_current_skin(&m)->theme);
        if (rdrawn <= 0) {
            printf("FAIL: Revolution skin drew nothing\n");
            failures++;
        }

        /* Verify cursor rect bounds stay within 1280x720 across all channels and dock buttons */
        for (int c = 0; c <= 14; c++) {
            m.category_cursor[0] = c;
            int cx = 0, cy = 0, cw = 0, ch = 0;
            if (home_cursor_rect(&m, &home_current_skin(&m)->theme, &cx, &cy, &cw, &ch) == 0) {
                printf("FAIL: Revolution cursor rect returned 0 at slot %d\n", c);
                failures++;
            } else if (cx < 0 || cy < 0 || cx + cw > HOME_TEST_W || cy + ch > HOME_TEST_H) {
                printf("FAIL: Revolution cursor rect out of bounds [%d,%d,%d,%d] at slot %d\n",
                       cx, cy, cw, ch, c);
                failures++;
            }
        }
    }

    /* The frame digest: the console loop skips rendering and flipping whenever
     * this does not change, so a state change it fails to notice is a shell that
     * stops redrawing. Each case below is a thing the renderer shows. */
    {
        home_model_init(&m);

        uint64_t base = home_model_digest(&m);

        if (home_model_digest(&m) != base) {
            printf("FAIL: digest is not stable across calls on an unchanged model\n");
            failures++;
        }
        if (home_model_digest(NULL) != 0) {
            printf("FAIL: digest of NULL is not 0\n");
            failures++;
        }

        /*
         * Note for anyone tempted to add it: two separately initialised models do
         * NOT digest alike, and asserting that they do fails. Menu items carry
         * `const char *` fields pointing into the model's own character buffers -
         * build_system_menu() hands out m->dev.console_info_str, and others do
         * the same - so the hashed bytes include the address the model lives at.
         * Within one model those pointers are fixed for its lifetime, which is
         * all the console loop relies on; across two models they differ by
         * construction.
         */

        /* Stable over more repeats than a frame ever needs. */
        for (int i = 0; i < 8; i++) {
            if (home_model_digest(&m) != base) {
                printf("FAIL: digest drifted on an untouched model\n");
                failures++;
                break;
            }
        }

        struct {
            const char *what;
            uint64_t seen;
        } steps[10];
        unsigned step_count = 0;

        home_move(&m, HOME_RIGHT);
        steps[step_count].what = "carousel cursor moved";
        steps[step_count++].seen = home_model_digest(&m);

        home_open(&m, HOME_SCREEN_LIBRARY);
        steps[step_count].what = "screen opened";
        steps[step_count++].seen = home_model_digest(&m);

        home_move(&m, HOME_DOWN);
        steps[step_count].what = "menu cursor moved";
        steps[step_count++].seen = home_model_digest(&m);

        home_next_theme(&m);
        steps[step_count].what = "theme changed";
        steps[step_count++].seen = home_model_digest(&m);

        home_show_toast(&m, "TOAST", "SHOWN");
        steps[step_count].what = "toast raised";
        steps[step_count++].seen = home_model_digest(&m);

        home_tick(&m);
        steps[step_count].what = "toast timer ticked";
        steps[step_count++].seen = home_model_digest(&m);

        home_show_dialog(&m, HOME_DIALOG_IME, "SEARCH", "");
        steps[step_count].what = "dialog opened";
        steps[step_count++].seen = home_model_digest(&m);

        home_move(&m, HOME_RIGHT);
        steps[step_count].what = "IME key cursor moved";
        steps[step_count++].seen = home_model_digest(&m);

        home_close_dialog(&m);
        steps[step_count].what = "dialog closed";
        steps[step_count++].seen = home_model_digest(&m);

        home_toggle_control_centre(&m);
        steps[step_count].what = "control centre toggled";
        steps[step_count++].seen = home_model_digest(&m);

        /* Every step must differ from the one before it, and from the start. */
        uint64_t prev = base;
        for (unsigned i = 0; i < step_count; i++) {
            if (steps[i].seen == prev) {
                printf("FAIL: digest unchanged after %s - the loop would not redraw\n",
                       steps[i].what);
                failures++;
            }
            prev = steps[i].seen;
        }

        /*
         * Returning to a state must return the digest to it, or an idle shell
         * would redraw for ever. Carousel movement is the reversible case that
         * matters: it is what a user leans on.
         *
         * Screen navigation is deliberately not asserted here. home_open() builds
         * the target screen's menu and home_back() only pops the stack, so the
         * built menu stays in the model and the digest does not come back. That
         * is the digest being conservative about state the renderer would not
         * have shown - it costs one extra frame, which is the direction this is
         * allowed to be wrong in.
         */
        home_model_init(&m);
        uint64_t settled = home_model_digest(&m);
        home_move(&m, HOME_RIGHT);
        if (home_model_digest(&m) == settled) {
            printf("FAIL: carousel move did not change the digest\n");
            failures++;
        }
        home_move(&m, HOME_LEFT);
        if (home_model_digest(&m) != settled) {
            printf("FAIL: moving right then left did not restore the digest\n");
            failures++;
        }
    }

    /* Test live telemetry and settings menu updates */
    {
        home_model_init(&m);
        home_refresh_telemetry(&m);
        if (m.dev.console_info_str[0] == '\0' || m.dev.pltauth_str[0] == '\0' || m.dev.hw_telemetry_str[0] == '\0') {
            printf("FAIL: home_refresh_telemetry did not generate formatted strings\n");
            failures++;
        }
        home_menu_t *sys_menu = &m.menus[HOME_SCREEN_SETTINGS_SYSTEM];
        if (sys_menu->count < 3 || strcmp(sys_menu->items[0].label, "CONSOLE INFORMATION") != 0) {
            printf("FAIL: system settings menu did not populate properly\n");
            failures++;
        }
        home_menu_t *dev_menu = &m.menus[HOME_SCREEN_SETTINGS_DEVELOPER];
        if (dev_menu->count < 4 || strcmp(dev_menu->items[3].label, "PLTAUTH STATUS") != 0) {
            printf("FAIL: developer settings menu did not populate pltauth status\n");
            failures++;
        }
    }

    /* Test icon rendering and first-letter fallback */
    {
        oops_surface_t surf = test_surface();
        home_model_init(&m);

        /* Case 1: Default titles have icon_pixels == NULL, should render initial letter */
        (void)home_render(&surf, &m, home_theme_at(0));
        const home_theme_t *theme = home_theme_at(0);
        int tx = theme->margin_x;
        int base_y = theme->margin_y + (theme->row_height * 2);
        int ty = base_y - 10; /* selected title */
        int center_x = tx + (theme->tile_width / 2);
        int center_y = ty + (theme->tile_height + 20) / 2;
        int text_pixels_found = 0;
        for (int dy = -20; dy <= 20; dy++) {
            for (int dx = -20; dx <= 20; dx++) {
                uint32_t px = pixel_at(&surf, center_x + dx, center_y + dy);
                if (px == theme->text) {
                    text_pixels_found++;
                }
            }
        }
        if (text_pixels_found == 0) {
            printf("FAIL: fallback initial-letter was not rendered for title without icon\n");
            failures++;
        }

        /* Case 2: Title with valid icon_pixels blits icon over card */
        static uint32_t s_mock_icon[96 * 96];
        uint32_t icon_marker_color = 0xFF55FFAAu;
        for (int p = 0; p < 96 * 96; p++) {
            s_mock_icon[p] = icon_marker_color;
        }
        home_title_t icon_title = m.titles[0];
        icon_title.icon_pixels = s_mock_icon;
        icon_title.icon_width = 96;
        icon_title.icon_height = 96;
        home_set_titles(&m, &icon_title, 1);

        (void)home_render(&surf, &m, theme);
        int icon_pixels_found = 0;
        int icon_x = tx + (theme->tile_width - 96) / 2;
        int icon_y = ty + 8;
        for (int dy = 10; dy < 80; dy++) {
            for (int dx = 10; dx < 80; dx++) {
                if (pixel_at(&surf, icon_x + dx, icon_y + dy) == icon_marker_color) {
                    icon_pixels_found++;
                }
            }
        }
        if (icon_pixels_found == 0) {
            printf("FAIL: icon_pixels was not blitted onto tile\n");
            failures++;
        }
    }

    /* ---- 13. Search, Favorites, Library Filtering, and Switcher lifecycle ----------- */
    {
        home_model_t tm;
        home_model_init(&tm);
        recorder_t trec;
        trec.calls = 0;
        trec.last = HOME_ACTION_NONE;
        trec.last_arg = 0;
        trec.accept = 1;
        home_host_t thost;
        thost.ctx = &trec;
        thost.perform = record_action;
        home_set_host(&tm, &thost);

        /* 13.1 Favorites toggle */
        if (tm.titles[1].favorite != 0) {
            printf("FAIL: title favorite should default to 0\n");
            failures++;
        }
        (void)home_activate(&tm); /* Games carousel activation opens title options */
        tm.selected_title = 1;
        home_open(&tm, HOME_SCREEN_TITLE_OPTIONS);
        home_menu_t *opt_menu = home_current_menu(&tm);
        int fav_item_idx = -1;
        for (int it = 0; it < opt_menu->count; it++) {
            if (opt_menu->items[it].action == HOME_ACTION_TOGGLE_FAVORITE) {
                fav_item_idx = it;
                break;
            }
        }
        if (fav_item_idx < 0) {
            printf("FAIL: TITLE_OPTIONS menu missing TOGGLE_FAVORITE item\n");
            failures++;
        } else {
            opt_menu->cursor = fav_item_idx;
            (void)home_activate(&tm);
            if (tm.titles[1].favorite != 1) {
                printf("FAIL: toggling favorite should set favorite to 1\n");
                failures++;
            }
            opt_menu->cursor = fav_item_idx;
            (void)home_activate(&tm);
            if (tm.titles[1].favorite != 0) {
                printf("FAIL: toggling favorite again should set favorite to 0\n");
                failures++;
            }
            /* Re-pin title 1 for library test */
            opt_menu->cursor = fav_item_idx;
            (void)home_activate(&tm);
        }

        /* 13.2 Library category filtering */
        home_open(&tm, HOME_SCREEN_LIBRARY);
        if (tm.library_filter != HOME_FILTER_ALL) {
            printf("FAIL: library_filter should default to HOME_FILTER_ALL\n");
            failures++;
        }
        /* Item 0 is FILTER CATEGORY, activating it cycles filter */
        (void)home_activate(&tm); /* to HOME_FILTER_NATIVE */
        if (tm.library_filter != HOME_FILTER_NATIVE) {
            printf("FAIL: activating filter should advance to HOME_FILTER_NATIVE\n");
            failures++;
        }
        (void)home_activate(&tm); /* to HOME_FILTER_HOMEBREW */
        (void)home_activate(&tm); /* to HOME_FILTER_FAVORITES */
        if (tm.library_filter != HOME_FILTER_FAVORITES) {
            printf("FAIL: activating filter should advance to HOME_FILTER_FAVORITES\n");
            failures++;
        }
        home_menu_t *lib_menu = home_current_menu(&tm);
        int lib_titles = 0;
        for (int it = 0; it < lib_menu->count; it++) {
            if (lib_menu->items[it].action == HOME_ACTION_LAUNCH_TITLE) {
                lib_titles++;
            }
        }
        if (lib_titles != 1) {
            printf("FAIL: FAVORITES filter should only list pinned titles (found %d, expected 1)\n", lib_titles);
            failures++;
        }

        /* 13.3 Switcher lifecycle */
        if (tm.switcher.has_running_title != 0) {
            printf("FAIL: switcher should not have running title initially\n");
            failures++;
        }
        /* Launch title index 2 */
        tm.selected_title = 2;
        home_open(&tm, HOME_SCREEN_TITLE_OPTIONS);
        opt_menu = home_current_menu(&tm);
        opt_menu->cursor = 0; /* PLAY */
        (void)home_activate(&tm);
        if (tm.switcher.has_running_title != 1 || tm.switcher.running_title_index != 2 || tm.switcher.is_suspended != 0) {
            printf("FAIL: launching title should set switcher to running index 2\n");
            failures++;
        }
        if (tm.switcher.recent_count == 0 || tm.switcher.recent_indices[0] != 2) {
            printf("FAIL: launching title should place index 2 into recent_indices[0]\n");
            failures++;
        }
        /* Suspend title */
        home_open(&tm, HOME_SCREEN_SWITCHER);
        home_menu_t *tsw_menu = home_current_menu(&tm);
        int susp_item = -1;
        for (int it = 0; it < tsw_menu->count; it++) {
            if (tsw_menu->items[it].action == HOME_ACTION_SUSPEND_TITLE) {
                susp_item = it;
                break;
            }
        }
        if (susp_item < 0) {
            printf("FAIL: switcher menu missing SUSPEND_TITLE item\n");
            failures++;
        } else {
            tsw_menu->cursor = susp_item;
            (void)home_activate(&tm);
            if (tm.switcher.is_suspended != 1) {
                printf("FAIL: SUSPEND_TITLE should mark title as suspended\n");
                failures++;
            }
            /* Resume title */
            home_open(&tm, HOME_SCREEN_SWITCHER);
            tsw_menu = home_current_menu(&tm);
            int res_item = -1;
            for (int it = 0; it < tsw_menu->count; it++) {
                if (tsw_menu->items[it].action == HOME_ACTION_RESUME_TITLE) {
                    res_item = it;
                    break;
                }
            }
            if (res_item < 0) {
                printf("FAIL: switcher menu missing RESUME_TITLE item\n");
                failures++;
            } else {
                tsw_menu->cursor = res_item;
                (void)home_activate(&tm);
                if (tm.switcher.is_suspended != 0) {
                    printf("FAIL: RESUME_TITLE should clear is_suspended\n");
                    failures++;
                }
            }
        }
        /* Terminate title */
        home_open(&tm, HOME_SCREEN_SWITCHER);
        tsw_menu = home_current_menu(&tm);
        int term_item = -1;
        for (int it = 0; it < tsw_menu->count; it++) {
            if (tsw_menu->items[it].action == HOME_ACTION_TERMINATE_TITLE) {
                term_item = it;
                break;
            }
        }
        if (term_item < 0) {
            printf("FAIL: switcher menu missing TERMINATE_TITLE item\n");
            failures++;
        } else {
            tsw_menu->cursor = term_item;
            (void)home_activate(&tm);
            if (tm.switcher.has_running_title != 0) {
                printf("FAIL: TERMINATE_TITLE should clear has_running_title\n");
                failures++;
            }
        }

        /* 13.4 Universal Search */
        home_show_dialog(&tm, HOME_DIALOG_IME, "SEARCH", "");
        tm.dialog.ime_row = 4;
        tm.dialog.ime_col = 8; /* Done button */
        tm.dialog.ime_buffer[0] = 'R';
        tm.dialog.ime_buffer[1] = 'E';
        tm.dialog.ime_buffer[2] = 'T';
        tm.dialog.ime_buffer[3] = 'U';
        tm.dialog.ime_buffer[4] = 'R';
        tm.dialog.ime_buffer[5] = 'N';
        tm.dialog.ime_buffer[6] = '\0';
        tm.dialog.ime_len = 6;
        (void)home_activate(&tm);
        if (home_screen(&tm) != HOME_SCREEN_SEARCH) {
            printf("FAIL: IME done must transition to HOME_SCREEN_SEARCH\n");
            failures++;
        }
        if (strcmp(tm.last_search, "RETURN") != 0) {
            printf("FAIL: last_search should be 'RETURN', got '%s'\n", tm.last_search);
            failures++;
        }
        home_menu_t *srch_menu = home_current_menu(&tm);
        int search_matches = 0;
        for (int it = 0; it < srch_menu->count; it++) {
            if (srch_menu->items[it].action == HOME_ACTION_LAUNCH_TITLE) {
                search_matches++;
            }
        }
        if (search_matches != 1) {
            printf("FAIL: search for 'RETURN' should find 1 match (found %d)\n", search_matches);
            failures++;
        }
    }

    if (failures == 0) {
        printf("home selftest: ok (full Prospero shell model, seam, dialogs, IME, and renderer)\n");

        return 0;
    }
    printf("home selftest: %u failure(s)\n", failures);
    return 1;
}
