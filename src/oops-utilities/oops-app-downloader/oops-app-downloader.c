/*
 * oops-app-downloader: the pure parts - the bring-up render and the install-path helper.
 * Shared by the console payload and the host self-test.
 */

#include "oops-app-downloader.h"
#include "oops/draw.h"

#define BG        0xFF08090Bu
#define ACCENT    0xFF3FB950u
#define PENDING   0xFFD8A657u   /* amber */

static const char INSTALL_PREFIX[] = "/data/homebrew/";

int oops_dl_install_path(const char *title_id, char *buf, int buf_len) {
    if (!title_id || !buf) return -1;

    /* [A-Z]{4}[0-9]{5}, then NUL - a Sony title id is exactly nine characters. */
    for (int i = 0; i < 4; i++) {
        if (title_id[i] < 'A' || title_id[i] > 'Z') return -1;
    }
    for (int i = 4; i < 9; i++) {
        if (title_id[i] < '0' || title_id[i] > '9') return -1;
    }
    if (title_id[9] != '\0') return -1;

    const int prefix_len = (int)(sizeof(INSTALL_PREFIX) - 1);
    const int need = prefix_len + 9;
    if (buf_len < need + 1) return -1;

    for (int i = 0; i < prefix_len; i++) buf[i] = INSTALL_PREFIX[i];
    for (int i = 0; i < 9; i++) buf[prefix_len + i] = title_id[i];
    buf[need] = '\0';
    return need;
}

/* One status row: a state tag and its label. Returns the next y. */
static int row(oops_surface_t *surf, int y, int ready, const char *label) {
    oops_draw_text(surf, 60, y, ready ? "[ready]  " : "[soon]   ",
                   ready ? ACCENT : PENDING, 2);
    oops_draw_text(surf, 220, y, label, OOPS_COLOR_WHITE, 2);
    return y + 30;
}

int oops_dl_render(oops_surface_t *surf, const oops_dl_status_t *st) {
    if (!surf || !surf->pixels || !st) return 0;

    oops_draw_clear(surf, BG);
    oops_draw_rect(surf, 0, 0, surf->width, 6, ACCENT);

    int rows = 0;
    int y = 70;

    oops_draw_text(surf, 60, y, "OOPS App Downloader", OOPS_COLOR_WHITE, 5);
    y += 70; rows++;
    oops_draw_text(surf, 60, y, "Install homebrew directly on this console.",
                   OOPS_COLOR_GRAY, 2);
    y += 30; rows++;

    if (st->ip) {
        oops_draw_text(surf, 60, y, "console:", OOPS_COLOR_GRAY, 2);
        oops_draw_text(surf, 220, y, st->ip, ACCENT, 2);
        y += 30; rows++;
    }

    y += 20;
    oops_draw_text(surf, 60, y, "Capabilities", OOPS_COLOR_GRAY, 2);
    y += 34; rows++;

    y = row(surf, y, 1,                  "display + controller"); rows++;
    y = row(surf, y, st->net_ready,      "network sockets"); rows++;
    y = row(surf, y, st->install_ready,  "install to /data/homebrew"); rows++;
    y = row(surf, y, st->http_ready,     "download over HTTPS  (needs SDK http)"); rows++;
    y = row(surf, y, st->unzip_ready,    "unpack title.zip     (needs SDK unzip)"); rows++;

    y += 24;
    oops_draw_text(surf, 60, y, "Press O to exit.", OOPS_COLOR_GRAY, 2);
    rows++;

    return rows;
}
