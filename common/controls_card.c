/*
 * `controls_card.h` says what this is for and how a title uses it.
 */
#include "controls_card.h"

#include "oops/display.h"
#include "oops/draw.h"
#include "oops/freestd.h"
#include "oops/input.h"
#include "oops/system.h"
#include "oops/time.h"

#define CC_WIDTH 1920u
#define CC_HEIGHT 1080u
#define CC_ART_W 832u /* the drawing is 1024x640; this keeps its aspect */
#define CC_ART_H 520u
#define CC_MAX_ROWS 16
#define CC_MAX_NOTES 3
#define CC_FIELD 64

#define CC_BACKGROUND 0xFF0E1116u
#define CC_TEXT 0xFFE8ECF1u
#define CC_DIM 0xFF8A94A3u
#define CC_BUTTON 0xFFFFD23Cu /* amber, for the button names */

#ifndef OOPS_CONTROLS_SECONDS
#define OOPS_CONTROLS_SECONDS 10
#endif

struct cc_row {
    char button[CC_FIELD];
    char action[CC_FIELD];
};

static struct cc_row s_rows[CC_MAX_ROWS];
static char s_notes[CC_MAX_NOTES][128];
static int s_nrows;
static int s_nnotes;
static uint32_t s_art[CC_ART_W * CC_ART_H];

/* `src[0..len)` without its surrounding blanks, into `dst`. */
static void cc_trim_copy(char *dst, size_t size, const char *src, size_t len) {
    size_t n;

    while (len > 0u && (*src == ' ' || *src == '\t')) {
        src++;
        len--;
    }
    while (len > 0u &&
           (src[len - 1u] == ' ' || src[len - 1u] == '\t' || src[len - 1u] == '\r')) {
        len--;
    }
    n = len < size - 1u ? len : size - 1u;
    for (size_t i = 0; i < n; i++) {
        dst[i] = src[i];
    }
    dst[n] = '\0';
}

static void cc_parse(const char *table, size_t len) {
    size_t at = 0;

    s_nrows = 0;
    s_nnotes = 0;
    while (at < len) {
        size_t end = at;
        size_t eq = (size_t)-1;

        while (end < len && table[end] != '\n') {
            if (table[end] == '=' && eq == (size_t)-1) {
                eq = end;
            }
            end++;
        }
        if (table[at] == '>' && s_nnotes < CC_MAX_NOTES) {
            cc_trim_copy(s_notes[s_nnotes++], sizeof(s_notes[0]), table + at + 1,
                         end - at - 1u);
        } else if (table[at] != '#' && eq != (size_t)-1 && s_nrows < CC_MAX_ROWS) {
            cc_trim_copy(s_rows[s_nrows].button, CC_FIELD, table + at, eq - at);
            cc_trim_copy(s_rows[s_nrows].action, CC_FIELD, table + eq + 1,
                         end - eq - 1u);
            s_nrows++;
        }
        at = end + 1u;
    }
}

static void cc_text_centred(oops_surface_t *surf, int y, const char *text,
                            uint32_t color, int scale) {
    const int w = oops_draw_text_width(text, scale);
    (void)oops_draw_text(surf, ((int)surf->width - w) / 2, y, text, color, scale);
}

static void cc_draw(oops_display_t *disp, const char *title, int has_art,
                    int seconds_left) {
    oops_surface_t surf = oops_display_get_surface(disp);
    const int art_x = 110;
    const int rows_x = art_x + (int)CC_ART_W + 90;
    const int action_x = rows_x + 300;
    int y;
    char footer[96];

    if (!surf.pixels) {
        return;
    }
    oops_draw_clear(&surf, CC_BACKGROUND);
    cc_text_centred(&surf, 70, title, CC_TEXT, 5);
    cc_text_centred(&surf, 70 + 5 * 8 + 24, "Controls", CC_DIM, 3);

    if (has_art) {
        oops_surface_t art = {s_art, CC_ART_W, CC_ART_H, CC_ART_W, OOPS_SURFACE_LINEAR};
        oops_draw_blit_blend(&surf, art_x, 260, &art, 0, 0, (int)CC_ART_W,
                             (int)CC_ART_H);
    }

    y = 540 - s_nrows * 56 / 2;
    if (y < 220) {
        y = 220;
    }
    for (int i = 0; i < s_nrows; i++) {
        (void)oops_draw_text(&surf, rows_x, y, s_rows[i].button, CC_BUTTON, 3);
        (void)oops_draw_text(&surf, action_x, y, s_rows[i].action, CC_TEXT, 3);
        y += 56;
    }

    y = 880;
    for (int i = 0; i < s_nnotes; i++) {
        cc_text_centred(&surf, y, s_notes[i], CC_DIM, 2);
        y += 30;
    }
    (void)oops_snprintf(footer, sizeof(footer), "Press Cross to start (%d)",
                        seconds_left);
    cc_text_centred(&surf, 1010, footer, CC_TEXT, 2);

    (void)oops_display_flip(disp);
}

void oops_controls_card_show(const char *title, const char *table, size_t table_len,
                             const void *png, size_t png_len) {
    const uint32_t dismiss = OOPS_BUTTON_CROSS | OOPS_BUTTON_OPTIONS;
    oops_display_t *disp;
    uint64_t start_ms;
    int has_art;
    int armed = 0;
    int shown_left = -1;

    cc_parse(table, table_len);
    if (s_nrows == 0) {
        oops_log_warn("CONTROLS", "the controls table has no rows; no card");
        return;
    }
    if (oops_display_any_open()) {
        oops_log_warn("CONTROLS", "a display is already open; no card");
        return;
    }
    disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, CC_WIDTH, CC_HEIGHT);
    if (!disp || !oops_display_is_ready(disp)) {
        oops_log_warn("CONTROLS", "the card's display did not open (error %d); no card",
                      disp ? oops_display_get_last_error(disp) : -1);
        if (disp) {
            oops_display_close(disp);
        }
        return;
    }
    has_art = png_len > 0u && oops_png_decode(png, png_len, s_art, CC_ART_W, CC_ART_H,
                                              (uint32_t *)0, (uint32_t *)0) == 0;

    /* `scePadInit` and the user lookup happen here and nowhere else: a poll without
     * it never opens the pad, and only the timeout ended the card. */
    if (oops_input_init() != 0) {
        oops_log_warn("CONTROLS", "the pad did not open; the card waits out its time");
    }

    /*
     * A press only counts after the dismiss buttons have been seen up: a player still
     * holding Cross from launching the title would otherwise skip the card unseen.
     */
    start_ms = oops_time_get_ms();
    for (;;) {
        const uint64_t elapsed = oops_time_get_ms() - start_ms;
        const int left = OOPS_CONTROLS_SECONDS - (int)(elapsed / 1000u);
        oops_pad_state_t pad;

        if (left <= 0) {
            break;
        }
        if (left != shown_left) {
            cc_draw(disp, title, has_art, left);
            shown_left = left;
        }
        if (oops_input_poll(0, &pad) == 0) {
            if (!(pad.buttons & dismiss)) {
                armed = 1;
            } else if (armed) {
                break;
            }
        }
        (void)oops_system_pump_events();
        oops_time_sleep_ms(16);
    }
    oops_input_close();
    oops_display_close(disp);
    oops_log_info("CONTROLS", "card closed; the display is free");
}

/*
 * The entry point `common/controls.mk` installs in front of the title's own: the card,
 * then the title as it always started. Declared with an opaque argument so this needs
 * no loader header; the title's entry takes the same single pointer.
 */
#ifdef OOPS_CONTROLS_NEXT
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
static const unsigned char s_table[] = {
#embed OOPS_CONTROLS_TXT
    , 0};
static const unsigned char s_png[] = {
#embed OOPS_CONTROLS_PNG
};
#pragma clang diagnostic pop

int OOPS_CONTROLS_NEXT(const void *args);
int oops_controls_entry(const void *args);

__attribute__((visibility("default"))) int oops_controls_entry(const void *args) {
    oops_controls_card_show(OOPS_CONTROLS_TITLE, (const char *)s_table,
                            sizeof(s_table) - 1u, s_png, sizeof(s_png));
    return OOPS_CONTROLS_NEXT(args);
}
#endif
