/*
 * `loading_screen.h` says what this is for and how a title uses it.
 */
#include "loading_screen.h"

#include "oops/display.h"
#include "oops/draw.h"
#include "oops/freestd.h"
#include "oops/fs.h"
#include "oops/system.h"
#include "oops/time.h"

#define LS_WIDTH 1920u
#define LS_HEIGHT 1080u
#define LS_ICON 320u /* the icon's side on screen; icon0.png is 512 */
#define LS_BAR_W 960
#define LS_BAR_H 28
#define LS_REDRAW_MS 100u

#define LS_BACKGROUND 0xFF0E1116u
#define LS_TEXT 0xFFE8ECF1u
#define LS_DIM 0xFF8A94A3u
#define LS_BAR_TRACK 0xFF262C35u
#define LS_BAR_FILL 0xFF3FA7F5u

struct oops_loading {
    oops_display_t *disp;
    char title[96];
    char status[160];
    int has_icon;
    uint64_t last_ms;
};

/* One screen at a time, since there is one display: static rather than allocated, so
 * this needs no allocator in either kind of title. */
static struct oops_loading s_screen;
static uint32_t s_icon[LS_ICON * LS_ICON];
static unsigned char s_png[1024u * 1024u];

static void ls_copy(char *dst, size_t size, const char *src) {
    (void)oops_snprintf(dst, size, "%s", src ? src : "");
}

/* The package's icon, decoded and scaled to LS_ICON square. 0 without one. */
static int ls_load_icon(void) {
    const char *path = "/app0/sce_sys/icon0.png";
    const int64_t size = oops_fs_file_size(path);
    int fd;
    int64_t got = 0;

    if (size <= 0 || (uint64_t)size > sizeof(s_png)) {
        oops_log_info("LOAD", "no icon at %s (size %lld); showing the name alone", path,
                      (long long)size);
        return 0;
    }
    fd = oops_fs_open(path, OOPS_O_RDONLY, 0);
    if (fd < 0) {
        return 0;
    }
    while (got < size) {
        const int64_t r = oops_fs_read(fd, s_png + got, (size_t)(size - got));
        if (r <= 0) {
            break;
        }
        got += r;
    }
    (void)oops_fs_close(fd);
    if (got != size || oops_png_decode(s_png, (size_t)size, s_icon, LS_ICON, LS_ICON,
                                       (uint32_t *)0, (uint32_t *)0) != 0) {
        oops_log_warn("LOAD", "%s did not decode; showing the name alone", path);
        return 0;
    }
    return 1;
}

static void ls_text_centred(oops_surface_t *surf, int y, const char *text,
                            uint32_t color, int scale) {
    const int w = oops_draw_text_width(text, scale);
    (void)oops_draw_text(surf, ((int)surf->width - w) / 2, y, text, color, scale);
}

static void ls_draw(oops_loading_t *ls, uint64_t done, uint64_t total) {
    oops_surface_t surf = oops_display_get_surface(ls->disp);
    const int cx = (int)LS_WIDTH / 2;
    int y = 190;

    if (!surf.pixels) {
        return;
    }
    oops_draw_clear(&surf, LS_BACKGROUND);

    if (ls->has_icon) {
        oops_surface_t icon = {s_icon, LS_ICON, LS_ICON, LS_ICON, OOPS_SURFACE_LINEAR};
        oops_draw_blit_blend(&surf, cx - (int)LS_ICON / 2, y, &icon, 0, 0, (int)LS_ICON,
                             (int)LS_ICON);
        y += (int)LS_ICON + 48;
    } else {
        y += 200;
    }
    ls_text_centred(&surf, y, ls->title, LS_TEXT, 5);
    y += 5 * 8 + 90;

    if (total > 0u) {
        const uint64_t clamped = done > total ? total : done;
        const int fill = (int)((uint64_t)(LS_BAR_W - 8) * clamped / total);
        const unsigned pct = (unsigned)(clamped * 100u / total);
        char label[16];

        oops_draw_rect(&surf, cx - LS_BAR_W / 2, y, LS_BAR_W, LS_BAR_H, LS_BAR_TRACK);
        oops_draw_rect(&surf, cx - LS_BAR_W / 2 + 4, y + 4, fill, LS_BAR_H - 8,
                       LS_BAR_FILL);
        (void)oops_snprintf(label, sizeof(label), "%u%%", pct);
        (void)oops_draw_text(&surf, cx + LS_BAR_W / 2 + 20, y + (LS_BAR_H - 16) / 2,
                             label, LS_DIM, 2);
    }
    y += LS_BAR_H + 30;
    ls_text_centred(&surf, y, ls->status, LS_DIM, 2);

    (void)oops_display_flip(ls->disp);
}

oops_loading_t *oops_loading_open(const char *title) {
    oops_loading_t *ls = &s_screen;

    if (ls->disp) {
        oops_log_warn("LOAD", "a loading screen is already open");
        return (oops_loading_t *)0;
    }
    if (oops_display_any_open()) {
        oops_log_info("LOAD", "a display is already open; no loading screen");
        return (oops_loading_t *)0;
    }
    ls->disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, LS_WIDTH, LS_HEIGHT);
    if (!ls->disp || !oops_display_is_ready(ls->disp)) {
        oops_log_warn("LOAD", "no display for the loading screen (error %d)",
                      ls->disp ? oops_display_get_last_error(ls->disp) : -1);
        if (ls->disp) {
            oops_display_close(ls->disp);
            ls->disp = (oops_display_t *)0;
        }
        return (oops_loading_t *)0;
    }
    ls_copy(ls->title, sizeof(ls->title), title);
    ls_copy(ls->status, sizeof(ls->status), "Starting");
    ls->has_icon = ls_load_icon();
    ls->last_ms = oops_time_get_ms();
    ls_draw(ls, 0u, 0u);
    oops_log_info("LOAD", "loading screen open (%s)",
                  ls->has_icon ? "icon" : "no icon");
    return ls;
}

void oops_loading_update(oops_loading_t *ls, uint64_t done, uint64_t total,
                         const char *status) {
    uint64_t now;

    if (!ls || !ls->disp) {
        return;
    }
    if (status) {
        ls_copy(ls->status, sizeof(ls->status), status);
    }
    now = oops_time_get_ms();
    if (now - ls->last_ms < LS_REDRAW_MS && !(total > 0u && done >= total)) {
        return;
    }
    ls->last_ms = now;
    ls_draw(ls, done, total);
}

void oops_loading_tar_progress(const oops_tar_progress_t *progress, void *user) {
    char status[160];

    (void)oops_snprintf(status, sizeof(status),
                        "Unpacking game data - %zu files  %.100s", progress->files_done,
                        progress->name ? progress->name : "");
    oops_loading_update((oops_loading_t *)user, progress->bytes_done,
                        progress->bytes_total, status);
}

void oops_loading_close(oops_loading_t *ls) {
    if (!ls || !ls->disp) {
        return;
    }
    oops_log_info("LOAD", "loading screen closing after %llu completed flips",
                  (unsigned long long)oops_display_get_flip_count(ls->disp));
    oops_display_close(ls->disp);
    ls->disp = (oops_display_t *)0;
    oops_log_info("LOAD", "loading screen closed; the display is free");
}

int oops_loading_unpack_once(const char *title, const char *archive, const char *dest,
                             const char *marker) {
    oops_loading_t *ls;
    int rc;

    if (oops_fs_exists(marker)) {
        return 0;
    }
    ls = oops_loading_open(title);
    rc = oops_tar_unpack_once(archive, dest, marker, oops_loading_tar_progress, ls);
    if (rc != 0 && ls) {
        ls_copy(ls->status, sizeof(ls->status),
                "The game data could not be unpacked - the log says why");
        ls_draw(ls, 0u, 0u);
        oops_time_sleep_ms(5000);
    } else if (ls) {
        oops_loading_update(ls, 1u, 1u, "Starting");
    }
    oops_loading_close(ls);
    return rc;
}
