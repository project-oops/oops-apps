/*
 * OOPSy-daisy: the pure parts - catalogue parsing, the install path, and the screen.
 * Shared by the console payload and the host self-test.
 */

#include "oopsy-daisy.h"
#include "oops/draw.h"
#include "oops/freestd.h"   /* obs_strlen, obs_strstr - the SDK's freestanding string helpers */

#define BG      0xFF08090Bu
#define ACCENT  0xFFFFD23Bu   /* daisy yellow */

static const char INSTALL_PREFIX[] = "/data/homebrew/";

int oopsy_install_path(const char *title_id, char *buf, int buf_len) {
    if (!title_id || !buf) return -1;
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

/* Does the byte range [s, s+n) end with `suffix`? Length via the SDK's obs_strlen. */
static int ends_with(const char *s, unsigned n, const char *suffix) {
    unsigned sl = (unsigned)obs_strlen(suffix);
    if (n < sl) return 0;
    for (unsigned i = 0; i < sl; i++) {
        if (s[n - sl + i] != suffix[i]) return 0;
    }
    return 1;
}

/* The filename part of a URL byte range: everything after the last '/'. */
static const char *basename_range(const char *s, unsigned n, unsigned *out_len) {
    const char *fn = s;
    for (unsigned i = 0; i < n; i++) {
        if (s[i] == '/') fn = s + i + 1;
    }
    *out_len = (unsigned)((s + n) - fn);
    return fn;
}

int oopsy_parse_catalog(const char *json, oopsy_catalog_t *cat) {
    cat->count = 0;
    if (!json) return 0;

    const char *KEY = "\"browser_download_url\"";
    const unsigned KEYLEN = (unsigned)obs_strlen(KEY);
    const char *p = json;

    while (cat->count < OOPSY_MAX_ENTRIES) {
        p = obs_strstr(p, KEY);
        if (!p) break;
        p += KEYLEN;

        while (*p && *p != '"') p++;        /* to the value's opening quote */
        if (!*p) break;
        p++;
        const char *start = p;
        while (*p && *p != '"') p++;         /* to the closing quote */
        if (!*p) break;
        unsigned len = (unsigned)(p - start);
        p++;

        unsigned fnlen;
        const char *fn = basename_range(start, len, &fnlen);
        if (!ends_with(fn, fnlen, ".zip")) continue;

        /* Only a packaged title: `<app>-title-<gen>.zip`. */
        const char *marker = 0;
        for (unsigned i = 0; i + 7u <= fnlen; i++) {
            if (fn[i] == '-' && fn[i + 1] == 't' && fn[i + 2] == 'i' && fn[i + 3] == 't' &&
                fn[i + 4] == 'l' && fn[i + 5] == 'e' && fn[i + 6] == '-') {
                marker = fn + i;
                break;
            }
        }
        if (!marker) continue;

        oopsy_entry_t *e = &cat->items[cat->count];
        unsigned nl = (unsigned)(marker - fn);
        if (nl >= sizeof(e->name)) nl = (unsigned)sizeof(e->name) - 1;
        for (unsigned i = 0; i < nl; i++) e->name[i] = fn[i];
        e->name[nl] = '\0';

        unsigned ul = len;
        if (ul >= sizeof(e->url)) ul = (unsigned)sizeof(e->url) - 1;
        for (unsigned i = 0; i < ul; i++) e->url[i] = start[i];
        e->url[ul] = '\0';

        cat->count++;
    }
    return cat->count;
}

int oopsy_render(oops_surface_t *surf, const oopsy_view_t *v) {
    if (!surf || !surf->pixels || !v) return 0;

    oops_draw_clear(surf, BG);
    oops_draw_rect(surf, 0, 0, (int)surf->width, 6, ACCENT);

    int rows = 0, y = 60;
    oops_draw_text(surf, 60, y, "OOPSy-daisy", ACCENT, 5); y += 64; rows++;
    oops_draw_text(surf, 60, y, "Install homebrew onto this console.", OOPS_COLOR_GRAY, 2);
    y += 40; rows++;

    if (v->message) {
        oops_color_t c = (v->phase == OOPSY_FAILED) ? OOPS_COLOR_RED
                       : (v->phase == OOPSY_DONE)   ? ACCENT
                                                    : OOPS_COLOR_WHITE;
        oops_draw_text(surf, 60, y, v->message, c, 2);
        y += 40; rows++;
    }

    if (v->cat && v->phase != OOPSY_LOADING) {
        for (int i = 0; i < v->cat->count && i < 16; i++) {
            int sel = (i == v->selected);
            if (sel) oops_draw_text(surf, 40, y, ">", ACCENT, 2);
            oops_draw_text(surf, 74, y, v->cat->items[i].name, sel ? ACCENT : OOPS_COLOR_WHITE, 2);
            y += 26; rows++;
        }
        y += 20;
        oops_draw_text(surf, 60, y, "X install    O exit", OOPS_COLOR_GRAY, 2); rows++;
    }
    return rows;
}
