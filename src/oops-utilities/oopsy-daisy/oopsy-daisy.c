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

/* The "size":N that precedes `before` in the same GitHub asset object, or 0 if none is near.
 * The asset shape is {"name":...,"size":N,...,"browser_download_url":URL}, so the size sits a
 * short way behind the url we anchor on - scan a bounded window back for the last one. */
static unsigned parse_size_before(const char *json, const char *before) {
    const char *SZ = "\"size\":";
    unsigned n = (unsigned)obs_strlen(SZ);
    size_t off = (size_t)(before - json);
    const char *win = (off > 512) ? before - 512 : json;
    const char *hit = 0;
    for (const char *q = win; q + n <= before; q++) {
        unsigned i = 0;
        while (i < n && q[i] == SZ[i]) i++;
        if (i == n) hit = q + n;
    }
    if (!hit) return 0;
    unsigned v = 0;
    while (*hit >= '0' && *hit <= '9') { v = v * 10u + (unsigned)(*hit - '0'); hit++; }
    return v;
}

int oopsy_parse_catalog(const char *json, oopsy_catalog_t *cat) {
    cat->count = 0;
    if (!json) return 0;

    const char *KEY = "\"browser_download_url\"";
    const unsigned KEYLEN = (unsigned)obs_strlen(KEY);
    const char *p = json;

    while (cat->count < OOPSY_MAX_ENTRIES) {
        const char *mp = obs_strstr(p, KEY);
        if (!mp) break;
        p = mp + KEYLEN;

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

        e->size = parse_size_before(json, mp);

        cat->count++;
    }
    return cat->count;
}

/* --- The download / install queue (pure) ------------------------------------------------- */

int oopsy_queue_add(oopsy_queue_t *q, const oopsy_entry_t *e) {
    if (!q || !e) return -1;
    for (int i = 0; i < q->count; i++) {
        if (obs_strcmp(q->jobs[i].name, e->name) == 0) return -1;   /* already queued */
    }
    if (q->count >= OOPSY_MAX_JOBS) return -1;
    oopsy_job_t *j = &q->jobs[q->count];
    obs_strncpy(j->name, e->name, sizeof(j->name)); j->name[sizeof(j->name) - 1] = '\0';
    obs_strncpy(j->url,  e->url,  sizeof(j->url));   j->url[sizeof(j->url)  - 1] = '\0';
    j->state = OOPSY_JOB_QUEUED;
    j->total = e->size;
    j->done  = 0;
    j->error = 0;
    return q->count++;
}

int oopsy_queue_active(const oopsy_queue_t *q) {
    if (!q) return -1;
    for (int i = 0; i < q->count; i++) {
        oopsy_job_state_t s = q->jobs[i].state;
        if (s == OOPSY_JOB_QUEUED || s == OOPSY_JOB_DOWNLOADING || s == OOPSY_JOB_INSTALLING)
            return i;
    }
    return -1;
}

int oopsy_queue_pending(const oopsy_queue_t *q) {
    if (!q) return 0;
    int n = 0;
    for (int i = 0; i < q->count; i++) {
        oopsy_job_state_t s = q->jobs[i].state;
        if (s != OOPSY_JOB_DONE && s != OOPSY_JOB_FAILED) n++;
    }
    return n;
}

int oopsy_job_pct(const oopsy_job_t *j) {
    if (!j) return 0;
    switch (j->state) {
        case OOPSY_JOB_QUEUED:      return 0;
        case OOPSY_JOB_DOWNLOADING:
            if (j->total > 0) {
                unsigned p = (unsigned)((unsigned long long)j->done * 100ull / j->total);
                return p > 99u ? 99 : (int)p;   /* the final step is the install */
            }
            return 50;                          /* size unknown: indeterminate midpoint */
        case OOPSY_JOB_INSTALLING:  return 96;
        case OOPSY_JOB_DONE:        return 100;
        case OOPSY_JOB_FAILED:
            return (j->total && j->done) ? (int)((unsigned long long)j->done * 100ull / j->total) : 0;
    }
    return 0;
}

/* --- The screen -------------------------------------------------------------------------- */

#define TRACK 0xFF20242Au   /* progress-bar track */

static int render_queue(oops_surface_t *surf, const oopsy_view_t *v, int y) {
    int rows = 0;
    oops_draw_text(surf, 60, y, "Downloads", ACCENT, 3); y += 46; rows++;

    if (!v->queue || v->queue->count == 0) {
        oops_draw_text(surf, 60, y, "Nothing queued yet. Press [] on a title to add it.",
                       OOPS_COLOR_GRAY, 2);
        y += 40; rows++;
    } else {
        for (int i = 0; i < v->queue->count && i < 12; i++) {
            const oopsy_job_t *j = &v->queue->jobs[i];
            const char *label; oops_color_t lc;
            switch (j->state) {
                case OOPSY_JOB_QUEUED:      label = "queued";      lc = OOPS_COLOR_GRAY;  break;
                case OOPSY_JOB_DOWNLOADING: label = "downloading"; lc = OOPS_COLOR_WHITE; break;
                case OOPSY_JOB_INSTALLING:  label = "installing";  lc = OOPS_COLOR_WHITE; break;
                case OOPSY_JOB_DONE:        label = "installed";   lc = ACCENT;           break;
                default:                    label = j->error ? j->error : "failed";
                                            lc = OOPS_COLOR_RED;                          break;
            }
            oops_draw_text(surf, 60, y, j->name, OOPS_COLOR_WHITE, 2);
            oops_draw_text(surf, 500, y, label, lc, 2);
            y += 26;
            int bx = 60, bw = 760, bh = 12, pct = oopsy_job_pct(j);
            oops_draw_rect(surf, bx, y, bw, bh, TRACK);
            int fw = bw * pct / 100;
            if (fw > 0) {
                oops_color_t fc = (j->state == OOPSY_JOB_FAILED) ? OOPS_COLOR_RED : ACCENT;
                oops_draw_rect(surf, bx, y, fw, bh, fc);
            }
            y += bh + 18; rows++;
        }
    }
    y += 12;
    oops_draw_text(surf, 60, y, "O back", OOPS_COLOR_GRAY, 2); rows++;
    return rows;
}

int oopsy_render(oops_surface_t *surf, const oopsy_view_t *v) {
    if (!surf || !surf->pixels || !v) return 0;

    oops_draw_clear(surf, BG);
    oops_draw_rect(surf, 0, 0, (int)surf->width, 6, ACCENT);

    int rows = 0, y = 60;
    oops_draw_text(surf, 60, y, "OOPSy-daisy", ACCENT, 5); y += 64; rows++;

    if (v->screen == OOPSY_SCREEN_QUEUE) {
        return rows + render_queue(surf, v, y);
    }

    oops_draw_text(surf, 60, y, "Install homebrew onto this console.", OOPS_COLOR_GRAY, 2);
    y += 40; rows++;

    if (v->message) {
        oops_color_t c = (v->phase == OOPSY_FAILED) ? OOPS_COLOR_RED : OOPS_COLOR_WHITE;
        oops_draw_text(surf, 60, y, v->message, c, 2);
        y += 40; rows++;
    }

    if (v->cat && v->phase == OOPSY_BROWSE) {
        for (int i = 0; i < v->cat->count && i < 16; i++) {
            int sel = (i == v->selected);
            if (sel) oops_draw_text(surf, 40, y, ">", ACCENT, 2);
            oops_draw_text(surf, 74, y, v->cat->items[i].name, sel ? ACCENT : OOPS_COLOR_WHITE, 2);
            y += 26; rows++;
        }
        y += 20;
        oops_draw_text(surf, 60, y, "X queue    [] downloads    O exit", OOPS_COLOR_GRAY, 2); rows++;
    }
    return rows;
}
