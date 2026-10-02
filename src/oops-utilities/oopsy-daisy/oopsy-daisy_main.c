/*
 * OOPSy-DAISY payload entry - the console-only half.
 *
 * Brings up the network and display, fetches the oops-apps release over HTTPS, and
 * installs the titles you pick by downloading each `.zip` and unpacking it into the
 * homebrew folder. The catalogue parse, the queue and the JSON the UI reads are
 * oopsy-daisy.c; this wires them to oops/http.h, oops/zip.h and the SDK webview.
 *
 * The UI is the oops-apps index page, rendered on device by oops/webview.h
 * (oopsy-daisy_page.c). The page is presentation only: the native side owns the
 * network, the filesystem and the pad, and drives the page through the JS engine. If
 * the SDK does not carry the webview yet, the same engine falls back to the native
 * canvas menu (oopsy-daisy.c's oopsy_render) so the app always builds.
 */

#include "oops/display.h"
#include "oops/draw.h"
#include "oops/fs.h"
#include "oops/input.h"
#include "oops/keyboard.h"
#include "oops/net.h"
#include "oops/netctl.h"
#include "oops/syscall.h"
#include "oops/system.h"
#include "oops/thread.h"
#include "oops/time.h"

/* The installer half needs the SDK's HTTPS client and zip extractor - recent additions.
 * Guard on them so OOPSy-DAISY builds either way: with them it installs; without, it
 * says the support is pending rather than failing to compile. */
#if defined(__has_include)
#if __has_include(<oops/http.h>) && __has_include(<oops/zip.h>)
#include <oops/http.h>
#include <oops/zip.h>
#define OOPSY_HAVE_INSTALLER 1
#endif
#endif

/* The UI half is the SDK webview (litehtml + QuickJS, assembled). It is opted into from
 * the Makefile with -DOOPSY_USE_WEBVIEW rather than auto-detected from the header,
 * because the webview is C++ and pulls in a freestanding libc++ (and, if litehtml
 * throws, the unwinder) - a build the Makefile has to wire deliberately (see the
 * OOPS_CXX_* block there). Without that switch the app builds the native canvas menu
 * below, which needs only the C features and always links. The header is still checked,
 * so a stale switch on an SDK that lacks the webview is a clear error rather than a
 * wall of missing includes. */
#if defined(OOPSY_USE_WEBVIEW)
#if !defined(__has_include)
#error "OOPSY_USE_WEBVIEW needs a compiler with __has_include"
#elif !__has_include(<oops/webview.h>)
#error "OOPSY_USE_WEBVIEW is set but <oops/webview.h> is not in this SDK"
#else
#include <oops/webview.h>
#include <oops/js.h>
#define OOPSY_HAVE_WEBVIEW 1
#endif
#endif

#include "oops/pkg.h"
#include "oopsy-daisy.h"

#include <stdint.h>

/* Where a title's file manifest is recorded at install, one path per line, named
 * <TITLE_ID>.list. A title's own directory cannot be listed on this system (the
 * firmware refuses getdents/ getdirentries even escaped), so uninstall replays this
 * manifest to delete the files instead of walking the directory. Kept outside
 * /data/homebrew so it is not mistaken for a title. */
#define OOPSY_STATE_DIR "/data/oopsy"

/* The rolling build. Its assets are the source of truth for what can be installed, and
 * matching the web index (project-oops.github.io/oops-apps), so there is one catalogue,
 * not two. */
#define RELEASE_API                                                                    \
    "https://api.github.com/repos/project-oops/oops-apps/releases/tags/latest-main"

/* The site's own card metadata (title, subtitle, kind, readiness), published by the
 * Pages build expressly for this on-device downloader ("a machine can read the app list
 * without scraping the page"). The catalogue above says what is installable; this says
 * how each card reads, so the UI matches the website without a second listing to keep.
 */
#define APPS_INDEX_JSON "https://project-oops.github.io/oops-apps/apps.json"

#define HOMEBREW_ROOT "/data/homebrew"

/* True if the process was already unsandboxed at startup (e.g. etaHEN).
 * If false, the process runs sandboxed for network/HTTPS operations and
 * only briefly escapes to rootvnode during archive extraction or uninstall,
 * returning to the sandbox immediately after. */
static int g_already_unsandboxed;

static void get_download_tmp(char *out, size_t sz, const char *name) {
    const char *dir = oops_fs_exists("/data") ? "/data" : "/app0";
    (void)oops_snprintf(out, sz, "%s/oopsy-%s.zip", dir, name);
}

/* Lifecycle at INFO, failures at ERROR, through the SDK's levelled klog so OOPSy-DAISY
 * honours /app0/oops-log (an `OOPSY=level` line) exactly like the rest of the
 * collection. */
static void log_info(const char *m) {
    oops_klog_level(OOPS_LOG_INFO, "OOPSY", m);
}
static void log_error(const char *m) {
    oops_klog_level(OOPS_LOG_ERROR, "OOPSY", m);
}

/* How a job repaints while it works. The install engine is shared by both UIs, so it
 * does not know whether it is driving the webview or the native canvas - it just asks
 * its caller to repaint at each step and on each percent of progress. */
typedef void (*oopsy_repaint_fn)(void *ctx);

#ifdef OOPSY_HAVE_INSTALLER

/* Name the specific HTTP failure, so a wall shows *which* step broke - on screen and in
 * the log - instead of one catch-all. The release-asset URL 302-redirects to a CDN
 * host, so a failure to follow that is the likely `PARSE` case. */
static const char *download_err_msg(int rc) {
    switch (rc) {
    case OOPS_HTTP_ERR_TLS_UNAVAIL:
        return "Download failed: secure networking unavailable.";
    case OOPS_HTTP_ERR_RESOLVE:
        return "Download failed: could not resolve the host.";
    case OOPS_HTTP_ERR_CONNECT:
        return "Download failed: could not connect (TLS?).";
    case OOPS_HTTP_ERR_SEND:
        return "Download failed: request send failed.";
    case OOPS_HTTP_ERR_RECV:
        return "Download failed: receive failed.";
    case OOPS_HTTP_ERR_PARSE:
        return "Download failed: bad response (a redirect not followed?).";
    case OOPS_HTTP_ERR_WRITE:
        return "Download failed: could not write the file to /data.";
    case OOPS_HTTP_ERR_NOMEM:
        return "Download failed: out of memory.";
    default:
        return "Download failed.";
    }
}

/* Fetch and parse the catalogue. Returns 1 on success, 0 on failure (with *msg set).
 * Logged at INFO for the steps and DEBUG for the return codes, so turning `OOPSY` up in
 * /app0/oops-log traces the whole fetch without a rebuild. */
static int load_catalog(oopsy_catalog_t *cat, const char **msg) {
    oops_http_response_t resp;
    oops_klog_level(OOPS_LOG_INFO, "OOPSY", "catalogue: fetching " RELEASE_API);
    int rc = oops_http_get(RELEASE_API, &resp);
    oops_kprintf_level(OOPS_LOG_DEBUG, "OOPSY", "catalogue: http_get rc=%d", rc);
    if (rc == OOPS_HTTP_ERR_TLS_UNAVAIL) {
        *msg = "Secure networking is not available on this system yet.";
        log_error("catalogue: TLS unavailable");
        return 0;
    }
    if (rc != OOPS_HTTP_OK) {
        *msg = "Could not reach the release. Check the network.";
        log_error("catalogue: could not reach the release");
        return 0;
    }
    if (resp.status_code != 200 || !resp.body) {
        oops_kprintf_level(OOPS_LOG_ERROR, "OOPSY",
                           "catalogue: unexpected status=%d body=%d", resp.status_code,
                           resp.body ? 1 : 0);
        oops_http_response_free(&resp);
        *msg = "The release did not answer as expected.";
        return 0;
    }
    oopsy_parse_catalog(resp.body, cat);
    oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "catalogue: %d installable titles",
                       cat->count);
    oops_http_response_free(&resp);
    if (cat->count == 0) {
        *msg = "No installable titles in the latest build.";
        return 0;
    }
    return 1;
}

/* Carried through oops_http_get_to_file_cb so its per-chunk callback can advance the
 * job and repaint the UI while a (blocking) download runs. */
typedef struct {
    oopsy_job_t *job;
    oopsy_repaint_fn repaint;
    void *rctx;
    int last_pct;
} oopsy_dl_ctx_t;

/* Per chunk: record the bytes, and repaint only when the bar would actually move.
 * Repainting on every chunk would gate the download on a full relayout; a percent
 * change is smooth and cheap. */
static void on_dl_progress(uint64_t downloaded, uint64_t total, void *ud) {
    oopsy_dl_ctx_t *c = (oopsy_dl_ctx_t *)ud;
    c->job->done = (unsigned)downloaded;
    if (total)
        c->job->total = (unsigned)total; /* the catalogue size is the fallback */
    int pct = oopsy_job_pct(c->job);
    if (pct != c->last_pct) {
        c->last_pct = pct;
        if (c->repaint)
            c->repaint(c->rctx);
    }
}

/* Little-endian readers for the zip central directory, mirroring the SDK extractor's
 * own. */
static uint16_t oopsy_rd_u16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t oopsy_rd_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/* Record, one path per line, every member of the archive just extracted, to
 * OOPSY_STATE_DIR/<TITLE_ID>.list. The title id is the first path component of the
 * first entry (a title zip carries a top-level `<TITLE_ID>/`). This is what uninstall
 * replays to delete the files, since the firmware will not let us list the installed
 * directory. Best-effort: on any parse or write failure it just does not leave a
 * manifest, and uninstall falls back to the platform installer. Reads the same
 * central-directory records the SDK extractor reads, so it sees exactly the set of
 * files that landed on disk. */
static void oopsy_write_manifest(const void *zip_data, size_t zip_size) {
    if (!zip_data || zip_size < 22) {
        return;
    }
    const uint8_t *data = (const uint8_t *)zip_data;
    size_t max_search = (zip_size > 65557) ? 65557 : zip_size;
    const uint8_t *eocd = NULL;
    for (size_t i = 22; i <= max_search; i++) {
        const uint8_t *p = data + zip_size - i;
        if (oopsy_rd_u32(p) == 0x06054b50) {
            eocd = p;
            break;
        }
    }
    if (!eocd) {
        return;
    }
    uint16_t total = oopsy_rd_u16(eocd + 10);
    uint32_t cd_size = oopsy_rd_u32(eocd + 12);
    uint32_t cd_off = oopsy_rd_u32(eocd + 16);
    if ((size_t)cd_off + (size_t)cd_size > zip_size) {
        return;
    }
    const uint8_t *cd = data + cd_off;
    char id[16];
    int fd = -1;
    for (uint16_t e = 0; e < total; e++) {
        if (cd + 46 > data + zip_size || oopsy_rd_u32(cd) != 0x02014b50) {
            break;
        }
        uint16_t fn = oopsy_rd_u16(cd + 28);
        uint16_t ex = oopsy_rd_u16(cd + 30);
        uint16_t cm = oopsy_rd_u16(cd + 32);
        if (cd + 46 + (size_t)fn + ex + cm > data + zip_size) {
            break;
        }
        const char *name = (const char *)(cd + 46);
        if (fd < 0) {
            int k = 0;
            while (k < fn && k < 15 && name[k] != '/' && name[k] != '\\') {
                id[k] = name[k];
                k++;
            }
            id[k] = '\0';
            if (k < 4) {
                return; /* not a `<TITLE_ID>/...` layout - leave no manifest */
            }
            (void)oops_fs_mkdir(OOPSY_STATE_DIR, 0777);
            char mp[64];
            int j = 0;
            for (const char *c = OOPSY_STATE_DIR "/"; *c; c++) {
                mp[j++] = *c;
            }
            for (int i = 0; id[i]; i++) {
                mp[j++] = id[i];
            }
            for (const char *c = ".list"; *c; c++) {
                mp[j++] = *c;
            }
            mp[j] = '\0';
            fd = oops_fs_open(mp, OOPS_O_WRONLY | OOPS_O_CREAT | OOPS_O_TRUNC, 0644);
            if (fd < 0) {
                return;
            }
        }
        (void)oops_fs_write(fd, name, fn);
        (void)oops_fs_write(fd, "\n", 1);
        cd += 46 + (size_t)fn + ex + cm;
    }
    if (fd >= 0) {
        oops_fs_close(fd);
        oops_kprintf_level(OOPS_LOG_INFO, "OOPSY",
                           "install: wrote manifest for %s (%u entries)", id,
                           (unsigned)total);
    }
}

/* Work one job to completion: download the .zip with a live bar, then unpack it into
 * the homebrew folder (a title zip carries a top-level `<TITLE_ID>/`). Sets the job's
 * final state; a failure here stops only this job. `repaint`/`rctx` are how the calling
 * UI redraws itself. */
static void process_job(oopsy_job_t *job, oopsy_repaint_fn repaint, void *rctx) {
    oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "install: download invoked for %s",
                       job->name);
    job->state = OOPSY_JOB_DOWNLOADING;
    job->done = 0;
    /* Draw the downloading state on screen before the blocking transfer starts -
     * several cycles so the queue view is laid out, rendered and flipped to the front
     * buffer while still responsive. */
    if (repaint) {
        repaint(rctx);
        repaint(rctx);
        repaint(rctx);
    }

    char dl_path[256];
    get_download_tmp(dl_path, sizeof dl_path, job->name);
    (void)oops_fs_unlink(dl_path);
    oopsy_dl_ctx_t ctx = {job, repaint, rctx, -1};
    oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "install: GET %s -> %s", job->url,
                       dl_path);
    int rc = oops_http_get_to_file_cb(job->url, dl_path, on_dl_progress, &ctx);
    oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "install: %s download rc=%d", job->name,
                       rc);
    if (rc != OOPS_HTTP_OK) {
        job->state = OOPSY_JOB_FAILED;
        job->error = download_err_msg(rc);
        log_error(job->error);
        (void)oops_fs_unlink(dl_path);
        /* Draw the failure so it is visible in the queue view, not only in the log.
         */
        if (repaint) {
            repaint(rctx);
            repaint(rctx);
            repaint(rctx);
        }
        return;
    }

    /* Buffer the archive into RAM before leaving the sandbox, since
     * oops_system_escape_sandbox unmounts /app0. */
    void *zip_data = NULL;
    size_t zip_size = 0;
    int read_rc = oops_fs_read_all(dl_path, &zip_data, &zip_size);
    (void)oops_fs_unlink(dl_path);
    if (read_rc != 0 || !zip_data) {
        job->state = OOPSY_JOB_FAILED;
        job->error = "Could not buffer downloaded archive.";
        log_error("install: failed to read downloaded zip into RAM");
        if (repaint)
            repaint(rctx);
        return;
    }

    job->state = OOPSY_JOB_INSTALLING;
    oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "install: %s unpacking (%zu bytes)",
                       job->name, zip_size);
    if (repaint)
        repaint(rctx);

    /* Escape the sandbox to write to /data/homebrew. Downloads complete inside the
     * sandbox first, because the escape unmounts /app0 and on Prospero the kernel
     * network subsystem refuses sceHttpSendRequest from escaped credentials. */
    int escaped = 0;
    if (!g_already_unsandboxed) {
        int esc_rc = oops_system_escape_sandbox();
        oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "install: sandbox escape rc=%d",
                           esc_rc);
        if (esc_rc != 0) {
            oops_fs_free_data(zip_data);
            job->state = OOPSY_JOB_FAILED;
            job->error = "Sandbox escape failed (sandbox-daemon not running).";
            log_error("install: sandbox escape failed");
            if (repaint)
                repaint(rctx);
            return;
        }
        escaped = 1;
    }
    (void)oops_fs_mkdir(HOMEBREW_ROOT, 0777);
    (void)oops_fs_chmod(HOMEBREW_ROOT, 0777);

    rc = oops_zip_extract_mem(zip_data, zip_size, HOMEBREW_ROOT);
    oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "install: %s unzip rc=%d", job->name,
                       rc);
    if (rc != OOPS_ZIP_OK) {
        job->state = OOPSY_JOB_FAILED;
        job->error = "Unpack failed.";
        log_error("install: unpack failed");
    } else {
        /* Record what landed, from the archive still in RAM, so it can be removed
         * later. */
        oopsy_write_manifest(zip_data, zip_size);
        job->state = OOPSY_JOB_DONE;
        job->done = job->total;
        oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "install: %s installed", job->name);
    }
    oops_fs_free_data(zip_data);

    /* Return to sandbox so subsequent HTTP downloads succeed. */
    if (escaped) {
        int unesc_rc = oops_system_unescape_sandbox();
        oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "install: sandbox unescape rc=%d",
                           unesc_rc);
    }

    if (repaint)
        repaint(rctx);
}

#else /* the SDK does not carry oops/http.h + oops/zip.h yet */

static int load_catalog(oopsy_catalog_t *cat, const char **msg) {
    (void)cat;
    *msg = "On-device install needs the SDK HTTPS + unzip support (pending).";
    return 0;
}
static void process_job(oopsy_job_t *job, oopsy_repaint_fn repaint, void *rctx) {
    job->state = OOPSY_JOB_FAILED;
    job->error = "On-device install pending SDK support.";
    if (repaint)
        repaint(rctx);
}

#endif /* OOPSY_HAVE_INSTALLER */

/* --- The native canvas menu (fallback when the SDK has no webview)
 * ------------------------- */

/* Draw the native view now (surface + flip). */
static void native_render(oops_display_t *disp, const oopsy_view_t *view) {
    oops_surface_t s = oops_display_get_surface(disp);
    if (s.pixels)
        oopsy_render(&s, view);
    oops_display_flip(disp);
}

#ifndef OOPSY_HAVE_WEBVIEW /* the native menu and its repaint are the fallback UI only \
                            */

typedef struct {
    oops_display_t *disp;
    const oopsy_view_t *view;
} native_ctx_t;
static void native_repaint(void *ctx) {
    native_ctx_t *n = (native_ctx_t *)ctx;
    native_render(n->disp, n->view);
}

static void run_native_ui(oops_display_t *disp, oopsy_catalog_t *cat,
                          oopsy_queue_t *queue, oopsy_view_t *view) {
    native_ctx_t nc = {disp, view};
    native_repaint(&nc);

    uint32_t last = 0;
    int running = 1;
    while (running) {
        if (oops_system_close_requested())
            break;

        /* Pad and keyboard map to the same OOPS_BUTTON_* bits, so read the keyboard
         * always and OR the pad on top - either drives the menu. */
        oops_pad_state_t pad;
        uint32_t buttons = oops_keyboard_poll_buttons();
        if (oops_input_poll(0, &pad) == 0)
            buttons |= pad.buttons;
        uint32_t pressed = buttons & ~last;
        last = buttons;

        if (view->screen == OOPSY_SCREEN_QUEUE) {
            if (pressed & OOPS_BUTTON_CIRCLE)
                view->screen = OOPSY_SCREEN_BROWSE; /* back to list */
        } else {
            if (pressed & OOPS_BUTTON_CIRCLE)
                running = 0; /* exit */
            if (pressed & OOPS_BUTTON_SQUARE)
                view->screen = OOPSY_SCREEN_QUEUE; /* open queue */
            if (view->phase == OOPSY_BROWSE && cat->count > 0) {
                if ((pressed & OOPS_BUTTON_UP) && view->selected > 0)
                    view->selected--;
                if ((pressed & OOPS_BUTTON_DOWN) && view->selected < cat->count - 1)
                    view->selected++;
                if (pressed & OOPS_BUTTON_CROSS)
                    oopsy_queue_add(queue, &cat->items[view->selected]);
            }
        }

        native_repaint(&nc);

        int ai = oopsy_queue_active(queue);
        if (ai >= 0 && queue->jobs[ai].state == OOPSY_JOB_QUEUED) {
            process_job(&queue->jobs[ai], native_repaint, &nc);
        }
    }
}

#endif /* !OOPSY_HAVE_WEBVIEW */

/* --- The webview UI (the oops-apps index page, on device)
 * ---------------------------------- */

#ifdef OOPSY_HAVE_WEBVIEW

/* Passed to the bridge functions as userdata so they can serialise the live
 * catalogue and queue, and scroll the view to keep the highlighted card in sight.
 */
typedef struct {
    oopsy_catalog_t *cat;
    oopsy_queue_t *queue;
    oops_webview_t *wv;
} oopsy_bridge_t;

/* Downloads run on a worker thread so a slow or failing transfer never stalls the
 * UI. The mutex guards the queue's structure while the UI thread adds or reads jobs
 * and the worker claims them; the job state fields are aligned words the two
 * threads read and write without tearing. */
static oops_mutex_t g_dl_mutex;
static volatile int g_dl_run;
static oopsy_queue_t *g_dl_queue;

/* Static because the returned string is built once per call on the payload's single
 * thread; the catalogue can be all 48 entries, the queue up to 32 jobs. */
static char g_cat_json[8192];
static char g_queue_json[4096];

/* The site's apps.json, fetched once at start and returned verbatim to the page by
 * __oopsy_meta(). Sized for the whole catalogue's card metadata (16+ apps, each
 * with a rendered README); if it ever overruns, the copy stops at a NUL boundary
 * and the page falls back to catalogue-only cards. */
static char g_meta_json[262144];

/* A freestanding string compare - the payload's libc is minimal and this is the one
 * place the native side matches a page-supplied name against the catalogue. */
static int streq(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

#ifdef OOPSY_HAVE_INSTALLER
/* Fetch the site's card metadata into g_meta_json. Best-effort: on any failure the
 * buffer is left as an empty JSON array and the page renders from the release
 * catalogue alone. */
static void load_meta(void) {
    oops_http_response_t resp;
    int rc = oops_http_get(APPS_INDEX_JSON, &resp);
    oops_kprintf_level(OOPS_LOG_DEBUG, "OOPSY", "meta: http_get rc=%d", rc);
    if (rc == OOPS_HTTP_OK && resp.status_code == 200 && resp.body) {
        int i = 0;
        const char *b = resp.body;
        while (b[i] && i < (int)sizeof g_meta_json - 1) {
            g_meta_json[i] = b[i];
            i++;
        }
        g_meta_json[i] = '\0';
        if (b[i])
            oops_klog_level(OOPS_LOG_ERROR, "OOPSY", "meta: apps.json truncated");
        oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "meta: apps.json %d bytes", i);
        oops_http_response_free(&resp);
        return;
    }
    if (rc == OOPS_HTTP_OK)
        oops_http_response_free(&resp);
    g_meta_json[0] = '[';
    g_meta_json[1] = ']';
    g_meta_json[2] = '\0';
    oops_klog_level(OOPS_LOG_ERROR, "OOPSY",
                    "meta: apps.json unavailable; cards fall back to the release");
}
#else
static void load_meta(void) {
    g_meta_json[0] = '[';
    g_meta_json[1] = ']';
    g_meta_json[2] = '\0';
}
#endif

/* __oopsy_catalog() -> the catalogue as JSON. The page renders one card per array
 * entry, and its index is what the controller installs on X. */
static oops_js_value_t cb_catalog(oops_js_t *js, int argc, oops_js_value_t *argv,
                                  void *ud) {
    (void)argc;
    (void)argv;
    oopsy_bridge_t *b = (oopsy_bridge_t *)ud;
    int n = oopsy_catalog_json(b->cat, g_cat_json, (int)sizeof g_cat_json);
    char head[81];
    int hi = 0;
    for (; hi < 80 && g_cat_json[hi]; hi++)
        head[hi] = g_cat_json[hi];
    head[hi] = '\0';
    oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "cb_catalog count=%d len=%d head=%s",
                       b->cat ? b->cat->count : -1, n, head);
    return oops_js_make_string(js, g_cat_json);
}

/* __oopsy_queue() -> the download queue as JSON, polled by the page to draw its
 * progress bars. */
static oops_js_value_t cb_queue(oops_js_t *js, int argc, oops_js_value_t *argv,
                                void *ud) {
    (void)argc;
    (void)argv;
    oopsy_bridge_t *b = (oopsy_bridge_t *)ud;
    static int qcalls = 0;
    oops_mutex_lock(&g_dl_mutex);
    int n = oopsy_queue_json(b->queue, g_queue_json, (int)sizeof g_queue_json);
    oops_mutex_unlock(&g_dl_mutex);
    if (qcalls < 3 || (qcalls % 20) == 0)
        oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "cb_queue call=%d qcount=%d len=%d",
                           qcalls, b->queue ? b->queue->count : -1, n);
    qcalls++;
    return oops_js_make_string(js, g_queue_json);
}

/* __oopsy_meta() -> the site's apps.json (card metadata), which the page merges
 * with the catalogue to draw the same cards the website shows. */
static oops_js_value_t cb_meta(oops_js_t *js, int argc, oops_js_value_t *argv,
                               void *ud) {
    (void)argc;
    (void)argv;
    (void)ud;
    return oops_js_make_string(js, g_meta_json);
}

/* __oopsy_install(name) -> queue the named title for install. The page owns the
 * filtered grid, so it installs by name and the native side resolves the name to
 * its catalogue asset. The argument arrives already decoded by the JS bridge
 * (argv[0].u.string). Returns true if a job was queued. */
static oops_js_value_t cb_install(oops_js_t *js, int argc, oops_js_value_t *argv,
                                  void *ud) {
    (void)js;
    oopsy_bridge_t *b = (oopsy_bridge_t *)ud;
    if (!oops_system_can_escape_sandbox()) {
        oops_kprintf_level(OOPS_LOG_ERROR, "OOPSY",
                           "install: refused - sandbox escape unavailable");
        return oops_js_make_bool(0);
    }
    if (argc < 1 || argv[0].type != OOPS_JS_TYPE_STRING || !argv[0].u.string)
        return oops_js_make_bool(0);
    const char *name = argv[0].u.string;
    for (int i = 0; i < b->cat->count; i++) {
        if (streq(b->cat->items[i].name, name)) {
            oops_mutex_lock(&g_dl_mutex);
            int j = oopsy_queue_add(b->queue, &b->cat->items[i]);
            oops_mutex_unlock(&g_dl_mutex);
            oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "install: %s -> job %d", name,
                               j);
            return oops_js_make_bool(j >= 0);
        }
    }
    oops_kprintf_level(OOPS_LOG_ERROR, "OOPSY", "install: %s not in catalogue", name);
    return oops_js_make_bool(0);
}

/* A well-formed title id: four upper-case letters then five digits, like GLCB00001. */
static int valid_title_id(const char *id) {
    for (int i = 0; i < 4; i++)
        if (id[i] < 'A' || id[i] > 'Z')
            return 0;
    for (int i = 4; i < 9; i++)
        if (id[i] < '0' || id[i] > '9')
            return 0;
    return id[9] == '\0';
}

/* __oopsy_installed(csv) -> of the comma-separated title ids the page passes, a JSON
 * array of the ones installed under /data/homebrew. The firmware refuses to enumerate
 * that directory (every getdents/getdirentries variant returns -1, even escaped), but
 * opening a known child works, so we test each id by existence rather than by listing.
 * Title ids are [A-Z0-9], so no JSON escaping. */
static char g_installed_json[8192];
static oops_js_value_t cb_installed(oops_js_t *js, int argc, oops_js_value_t *argv,
                                    void *ud) {
    (void)ud;
    int p = 0;
    g_installed_json[p++] = '[';
    int first = 1;
    if (argc >= 1 && argv[0].type == OOPS_JS_TYPE_STRING && argv[0].u.string) {
        const char *s = argv[0].u.string;
        while (*s) {
            char id[16];
            int k = 0;
            while (*s && *s != ',') {
                if (k < 15)
                    id[k++] = *s;
                s++;
            }
            id[k] = '\0';
            if (*s == ',')
                s++;
            if (k != 9 || !valid_title_id(id))
                continue;
            char path[64];
            int j = 0;
            for (const char *c = HOMEBREW_ROOT "/"; *c; c++)
                path[j++] = *c;
            for (int i = 0; i < 9; i++)
                path[j++] = id[i];
            path[j] = '\0';
            bool registered = false;
            int rc_reg = oops_app_exists(id, &registered);
            if (oops_fs_exists(path) || (rc_reg == 0 && registered)) {
                if (!first && p < (int)sizeof g_installed_json - 1)
                    g_installed_json[p++] = ',';
                first = 0;
                if (p < (int)sizeof g_installed_json - 1)
                    g_installed_json[p++] = '"';
                for (int i = 0; i < 9 && p < (int)sizeof g_installed_json - 2; i++)
                    g_installed_json[p++] = id[i];
                if (p < (int)sizeof g_installed_json - 1)
                    g_installed_json[p++] = '"';
            }
        }
    }
    if (p < (int)sizeof g_installed_json - 1)
        g_installed_json[p++] = ']';
    g_installed_json[p] = '\0';
    oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "installed: unsandboxed=%d -> %s",
                       g_already_unsandboxed, g_installed_json);
    return oops_js_make_string(js, g_installed_json);
}

/* Build OOPSY_STATE_DIR/<id>.list (the manifest path) into buf. `id` is a validated
 * 9-char id. */
static void oopsy_manifest_path(char *buf, const char *id) {
    int j = 0;
    for (const char *c = OOPSY_STATE_DIR "/"; *c; c++)
        buf[j++] = *c;
    for (int i = 0; i < 9; i++)
        buf[j++] = id[i];
    for (const char *c = ".list"; *c; c++)
        buf[j++] = *c;
    buf[j] = '\0';
}

/* Build HOMEBREW_ROOT/<rel[0..n)> into buf. */
static void oopsy_hb_path(char *buf, size_t cap, const char *rel, size_t n) {
    size_t j = 0;
    for (const char *c = HOMEBREW_ROOT "/"; *c && j + 1 < cap; c++)
        buf[j++] = *c;
    for (size_t i = 0; i < n && j + 1 < cap; i++)
        buf[j++] = rel[i];
    buf[j] = '\0';
}

/* Replay a title's install manifest to delete it: unlink every listed file, then remove
 * the directories those paths imply, deepest first (rmdir needs an empty directory, and
 * the firmware will not enumerate one for us). Returns 1 if a manifest was found and
 * processed, 0 if none. */
static int oopsy_delete_by_manifest(const char *id) {
    char mp[64];
    oopsy_manifest_path(mp, id);
    char *buf = NULL;
    size_t sz = 0;
    if (oops_fs_read_all(mp, (void **)&buf, &sz) != 0 || !buf || sz == 0) {
        if (buf)
            oops_fs_free_data(buf);
        return 0;
    }
    char fp[512];
    /* Pass 1: unlink files. Directory entries (a trailing '/') and blank lines are left
     * to below. */
    for (size_t i = 0; i < sz;) {
        size_t s = i;
        while (i < sz && buf[i] != '\n')
            i++;
        size_t len = i - s;
        if (i < sz)
            i++;
        if (len == 0 || buf[s + len - 1] == '/')
            continue;
        oopsy_hb_path(fp, sizeof fp, buf + s, len);
        (void)oops_fs_unlink(fp);
    }
    /* Passes: rmdir every ancestor directory of each path. Repeated so a directory
     * emptied in one pass lets its parent go in the next; title trees are shallow, so 8
     * passes is ample. */
    for (int pass = 0; pass < 8; pass++) {
        for (size_t i = 0; i < sz;) {
            size_t s = i;
            while (i < sz && buf[i] != '\n')
                i++;
            size_t len = i - s;
            if (i < sz)
                i++;
            for (size_t k = 0; k < len; k++) {
                if (buf[s + k] == '/') {
                    oopsy_hb_path(fp, sizeof fp, buf + s, k);
                    (void)oops_fs_rmdir(fp);
                }
            }
        }
    }
    oops_fs_free_data(buf);
    (void)oops_fs_unlink(mp); /* the manifest has served its purpose */
    return 1;
}

/* __oopsy_uninstall(title_id) -> remove an installed title. Three layers, since two
 * kinds of title reach this console by two paths: the platform installer
 * (sceAppInstUtil) owns a *registered* title's files and its home-screen entry, and a
 * replay of our own install manifest removes a title we extracted into /data/homebrew
 * (whose directory the firmware will not let us list). Success is the title's
 * /data/homebrew directory being gone afterward. Logs each layer so a failure says
 * which path was expected to remove it. */
static oops_js_value_t cb_uninstall(oops_js_t *js, int argc, oops_js_value_t *argv,
                                    void *ud) {
    (void)js;
    (void)ud;
    if (argc < 1 || argv[0].type != OOPS_JS_TYPE_STRING || !argv[0].u.string)
        return oops_js_make_bool(0);
    const char *id = argv[0].u.string;
    if (!valid_title_id(id)) {
        oops_kprintf_level(OOPS_LOG_ERROR, "OOPSY", "uninstall: bad id '%s'", id);
        return oops_js_make_bool(0);
    }
    if (!oops_system_can_escape_sandbox()) {
        oops_kprintf_level(OOPS_LOG_ERROR, "OOPSY",
                           "uninstall: %s refused - sandbox escape unavailable", id);
        return oops_js_make_bool(0);
    }
    char hb[64], am[64];
    int k = 0;
    for (const char *c = HOMEBREW_ROOT "/"; *c; c++)
        hb[k++] = *c;
    for (int i = 0; i < 9; i++)
        hb[k++] = id[i];
    hb[k] = '\0';
    k = 0;
    for (const char *c = "/user/appmeta/"; *c; c++)
        am[k++] = *c;
    for (int i = 0; i < 9; i++)
        am[k++] = id[i];
    am[k] = '\0';

    /* Uninstallation requires access to /data/homebrew and /user/appmeta outside the
     * sandbox. */
    int escaped = 0;
    if (!g_already_unsandboxed) {
        int esc_rc = oops_system_escape_sandbox();
        oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "uninstall: sandbox escape rc=%d",
                           esc_rc);
        if (esc_rc == 0)
            escaped = 1;
    }

    /* 1. The payload we extracted: replay its manifest (the proven, self-contained
     * path), then sweep any leftover with a tree walk (which only helps where the
     * firmware relents on enumeration) and a final rmdir of the root. Done first so a
     * title we installed is gone no matter what follows. */
    int had_manifest = oopsy_delete_by_manifest(id);
    int rc_data = oops_fs_exists(hb) ? oops_fs_rmtree(hb) : 0;
    (void)oops_fs_rmdir(hb);
    int gone = !oops_fs_exists(hb);

    /* 2. The platform installer: the clean path for a *registered* title (files +
     * home-screen entry), a no-op for a zip-only one. Only invoked for a title the
     * service says it knows, so a plain daisy install never reaches an untested service
     * call. Its result is logged either way. */
    bool app_before = false, app_after = false;
    int rc_exist = oops_app_exists(id, &app_before);
    int rc_app = -999; /* not attempted */
    if (rc_exist == 0 && app_before) {
        rc_app = oops_app_uninstall(id);
        (void)oops_app_exists(id, &app_after);
        if (!oops_fs_exists(hb))
            gone = 1;
    }

    /* 3. The home-screen registration under /user/appmeta, best-effort. */
    int rc_meta = oops_fs_exists(am) ? oops_fs_rmtree(am) : 0;

    oops_kprintf_level(
        OOPS_LOG_INFO, "OOPSY",
        "uninstall: %s manifest=%d data_rc=%d appExists rc=%d before=%d after=%d "
        "appUninstall rc=%d meta_rc=%d gone=%d",
        id, had_manifest, rc_data, rc_exist, (int)app_before, (int)app_after, rc_app,
        rc_meta, gone);

    if (escaped) {
        int unesc_rc = oops_system_unescape_sandbox();
        oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "uninstall: sandbox unescape rc=%d",
                           unesc_rc);
    }
    return oops_js_make_bool(gone);
}

/* A JS number arrives as either an int or a float tag depending on its value; take
 * either. */
static int arg_int(const oops_js_value_t *v) {
    if (v->type == OOPS_JS_TYPE_INT)
        return v->u.integer;
    if (v->type == OOPS_JS_TYPE_FLOAT)
        return (int)v->u.number;
    return 0;
}

/* __oopsy_scroll(top, height) -> keep the document range [top, top+height] on
 * screen. The page owns the grid, so it computes the highlighted card's position
 * and asks the view to reveal it. */
static oops_js_value_t cb_scroll(oops_js_t *js, int argc, oops_js_value_t *argv,
                                 void *ud) {
    (void)js;
    oopsy_bridge_t *b = (oopsy_bridge_t *)ud;
    if (argc >= 2 && b->wv)
        oops_webview_ensure_visible(b->wv, arg_int(&argv[0]), arg_int(&argv[1]));
    return oops_js_make_undefined();
}

typedef struct {
    oops_webview_t *wv;
    oops_display_t *disp;
} webview_ctx_t;

/* One frame: pump (timers, the queue poll, microtasks, relayout), paint the page
 * into the display surface, present. */
static void webview_paint(webview_ctx_t *c) {
    static unsigned fr = 0;
    /* A sparse heartbeat - enough to see the UI is alive and where a stall begins,
     * without flooding the log every frame. */
    if ((fr % 300u) == 0)
        oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "paint heartbeat fr=%u", fr);
    oops_webview_pump(c->wv);
    oops_surface_t s = oops_display_get_surface(c->disp);
    if (s.pixels)
        oops_webview_render(c->wv, &s);
    oops_display_flip(c->disp);
    fr++;
}

/* Run one fixed statement in the page. The statements here are literals or built
 * from an integer index and the app's own fixed strings, never from anything a page
 * or the network supplied. */
static void wv_eval(oops_webview_t *wv, const char *code) {
    oops_js_t *js = oops_webview_get_js(wv);
    oops_js_value_t r;
    int rc = oops_js_eval(js, code, "oopsy", &r);
    oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "wv_eval rc=%d code=%s", rc, code);
    if (rc == 0)
        oops_js_free_value(js, &r);
}

/* Evaluate an expression and return its truthiness - for page functions that report
 * an outcome, such as whether oopsyEnter() actually queued a title. */
static int wv_eval_bool(oops_webview_t *wv, const char *code) {
    oops_js_t *js = oops_webview_get_js(wv);
    oops_js_value_t r;
    int rc = oops_js_eval(js, code, "oopsy", &r);
    int val = 0;
    if (rc == 0) {
        if (r.type == OOPS_JS_TYPE_BOOL)
            val = r.u.boolean;
        else if (r.type == OOPS_JS_TYPE_INT)
            val = (r.u.integer != 0);
        oops_js_free_value(js, &r);
    }
    oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "wv_eval_bool rc=%d val=%d code=%s", rc,
                       val, code);
    return val;
}

/* oopsyAlert('title', 'msg') - show a modal banner. Quotes and newlines are sanitized.
 */
static void wv_alert(oops_webview_t *wv, const char *title, const char *msg) {
    char code[512];
    int p = 0;
    const char *pre = "oopsyAlert('";
    for (int k = 0; pre[k] && p < (int)sizeof code - 4; k++)
        code[p++] = pre[k];
    for (int k = 0; title && title[k] && p < (int)sizeof code - 8; k++) {
        char ch = title[k];
        if (ch == '\'' || ch == '\\' || ch == '\n' || ch == '\r')
            ch = ' ';
        code[p++] = ch;
    }
    const char *mid = "','";
    for (int k = 0; mid[k] && p < (int)sizeof code - 4; k++)
        code[p++] = mid[k];
    for (int k = 0; msg && msg[k] && p < (int)sizeof code - 4; k++) {
        char ch = msg[k];
        if (ch == '\'' || ch == '\\' || ch == '\n' || ch == '\r')
            ch = ' ';
        code[p++] = ch;
    }
    const char *post = "');";
    for (int k = 0; post[k] && p < (int)sizeof code - 1; k++)
        code[p++] = post[k];
    code[p] = '\0';
    wv_eval(wv, code);
}

/* Pull the next queued title and run its download+unpack off the UI thread. Passing
 * NULL for the repaint keeps this thread away from the webview - the UI thread
 * reads the job state and repaints. */
static void *download_worker(void *arg) {
    (void)arg;
    while (g_dl_run) {
        oopsy_job_t *job = 0;
        oops_mutex_lock(&g_dl_mutex);
        int ai = g_dl_queue ? oopsy_queue_active(g_dl_queue) : -1;
        if (ai >= 0 && g_dl_queue->jobs[ai].state == OOPSY_JOB_QUEUED) {
            job = &g_dl_queue->jobs[ai];
            job->state = OOPSY_JOB_DOWNLOADING;
        }
        oops_mutex_unlock(&g_dl_mutex);
        if (job)
            process_job(job, 0, 0);
        else
            oops_time_sleep_ms(50);
    }
    return 0;
}

static int run_webview_ui(oops_display_t *disp, oopsy_catalog_t *cat,
                          oopsy_queue_t *queue, const char *load_msg,
                          const char *alert_title) {
    /* Size the webview to the actual scanout surface, not a fixed 1280x720 - the
     * display backend hands back 1920x1080 here, and a smaller webview would lay
     * the page out in one corner. */
    oops_surface_t s0 = oops_display_get_surface(disp);
    int vw = (s0.width > 0) ? (int)s0.width : 1280;
    int vh = (s0.height > 0) ? (int)s0.height : 720;
    oops_webview_t *wv = oops_webview_create(vw, vh);
    if (!wv) {
        log_error("webview would not create");
        return -1;
    }
    oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "webview: created %dx%d", vw, vh);

    /* Load the page, then register the bridge on the live JS context and (re)run
     * its init - so the catalogue is populated whether or not the page's own
     * on-load init ran before the bindings existed. */
    oops_webview_load_html(wv, oopsy_page_html, "https://local.oops/");
    log_info("webview: html loaded");

    static oopsy_bridge_t bridge;
    bridge.cat = cat;
    bridge.queue = queue;
    bridge.wv = wv;
    oops_mutex_init(&g_dl_mutex, "oopsy-dl");
    /* The site's card metadata, fetched before the page's init reads
     * __oopsy_meta(). Best-effort: if it fails the page still renders the release
     * catalogue, just without kinds/subtitles. */
    load_meta();

    oops_js_t *js = oops_webview_get_js(wv);
    oops_js_register_fn(js, "__oopsy_catalog", cb_catalog, &bridge);
    oops_js_register_fn(js, "__oopsy_queue", cb_queue, &bridge);
    oops_js_register_fn(js, "__oopsy_meta", cb_meta, &bridge);
    oops_js_register_fn(js, "__oopsy_install", cb_install, &bridge);
    oops_js_register_fn(js, "__oopsy_installed", cb_installed, &bridge);
    oops_js_register_fn(js, "__oopsy_uninstall", cb_uninstall, &bridge);
    oops_js_register_fn(js, "__oopsy_scroll", cb_scroll, &bridge);

    for (int i = 0; i < 8; i++)
        oops_webview_pump(wv); /* let scripts + first layout settle */
    wv_eval(wv, "oopsyInit();");
    if (load_msg)
        wv_alert(wv, alert_title ? alert_title : "Notice", load_msg);

    log_info("webview: initialised");

    webview_ctx_t wc = {wv, disp};
    webview_paint(&wc);
    log_info("webview: first frame presented");
    webview_paint(&wc); /* a second frame so a first-paint relayout is settled */
    wv_eval(wv, "oLayoutDump();"); /* now geometry is computed - dump grid/card rects */

    /* Start the background download worker now that the queue and bridges are live.
     */
    g_dl_queue = queue;
    g_dl_run = 1;
    oops_thread_t dl_worker =
        oops_thread_create("oopsy-dl", download_worker, 0, 700, 256u * 1024u);
    if (!dl_worker)
        oops_kprintf_level(OOPS_LOG_ERROR, "OOPSY", "download worker would not start");

    int on_queue = (load_msg != NULL), running = 1;
    uint32_t last = 0;
    while (running) {
        if (oops_system_close_requested())
            break;

        oops_pad_state_t pad;
        uint32_t buttons = oops_keyboard_poll_buttons();
        if (oops_input_poll(0, &pad) == 0)
            buttons |= pad.buttons;
        uint32_t pressed = buttons & ~last;
        last = buttons;

        if (on_queue) {
            /* A modal is up - the downloads view, a confirm dialog, or an alert banner.
             * O closes it back to the grid; X confirms or dismisses the alert. */
            if (pressed & OOPS_BUTTON_CIRCLE) {
                on_queue = 0;
                wv_eval(wv, "oopsyBack();");
            }
            if (pressed & OOPS_BUTTON_CROSS) {
                wv_eval(wv, "oopsyConfirm();");
                if (!wv_eval_bool(
                        wv, "(__screen==='queue'||__screen==='confirm'||__err!=='')"))
                    on_queue = 0;
            }
        } else {
            /* The page owns the filtered grid and the highlight; the controller
             * forwards intents. Install is by name: oopsyEnter() calls back
             * __oopsy_install() for the highlighted title. The shoulders cycle the
             * kind and readiness filters, like the site's chips. */
            if (pressed & OOPS_BUTTON_CIRCLE)
                running = 0; /* exit */
            if (pressed & OOPS_BUTTON_SQUARE) {
                on_queue = 1;
                wv_eval(wv, "oopsyScreen('queue');");
            }
            if (pressed & OOPS_BUTTON_UP)
                wv_eval(wv, "oopsyNav('up');");
            if (pressed & OOPS_BUTTON_DOWN)
                wv_eval(wv, "oopsyNav('down');");
            if (pressed & OOPS_BUTTON_LEFT)
                wv_eval(wv, "oopsyNav('left');");
            if (pressed & OOPS_BUTTON_RIGHT)
                wv_eval(wv, "oopsyNav('right');");
            if (pressed & OOPS_BUTTON_L1)
                wv_eval(wv, "oopsyCycleKind(-1);");
            if (pressed & OOPS_BUTTON_R1)
                wv_eval(wv, "oopsyCycleKind(1);");
            if (pressed & OOPS_BUTTON_L2)
                wv_eval(wv, "oopsyCycleStatus(-1);");
            if (pressed & OOPS_BUTTON_R2)
                wv_eval(wv, "oopsyCycleStatus(1);");
            if (pressed & OOPS_BUTTON_CROSS) {
                /* Only switch to the downloads view when a title was actually
                 * queued. A title with no build is not installable, so oopsyEnter()
                 * reports false and shows its own message on the grid - switching
                 * to an empty queue would look like a hang. */
                oops_kprintf_level(OOPS_LOG_INFO, "OOPSY",
                                   "input: X - install selected");
                /* oopsyEnter opens the right modal itself - the download queue for an
                 * installable title, or the uninstall confirm for one already installed
                 * - and returns true when a modal is now up. */
                if (wv_eval_bool(wv, "oopsyEnter()"))
                    on_queue = 1;
            }
        }

        /* The worker advances the download on its own thread; reflect its state
         * live while the queue view is up or a job is working, so progress and any
         * error appear without blocking. */
        if (on_queue || oopsy_queue_active(queue) >= 0)
            wv_eval(wv, "oopsyRefresh();");

        webview_paint(&wc);
    }

    /* Stop the worker before tearing down the webview it feeds. */
    g_dl_run = 0;
    if (dl_worker)
        oops_thread_join(dl_worker, 0);
    oops_webview_destroy(wv);
    return 0;
}

#endif /* OOPSY_HAVE_WEBVIEW */

int oopsy_daisy_start(const payload_args_t *args);

int oopsy_daisy_start(const payload_args_t *args) {
    if (args)
        sys_call_init(args);
    log_info("entry reached");

    /* Run the payload's static constructors before any C++ runs. The webview stack
     * (litehtml, libc++, QuickJS) has global constructors that build its dispatch
     * tables; without this the first C++ call reaches a zero table and faults.
     * Guarded and a no-op for the native build, whose init array is empty (oops-sdk
     * system.c).
     */
    oops_run_init_array();

    oops_time_init();
    oops_net_init();
    oops_net_ctl_init();

    oops_display_t *disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, 1280, 720);
    if (!disp || !oops_display_is_ready(disp)) {
        log_error("display would not open");
        oops_net_ctl_term();
        oops_net_term();
        return -1;
    }
    oops_input_init();
    oops_keyboard_init(); /* so a USB keyboard drives the same nav as the pad */
    oops_system_install_close_handler();

    oopsy_catalog_t cat;
    cat.count = 0;
    oopsy_queue_t queue;
    queue.count = 0;

    /* A native loading frame before the blocking fetch - the webview is not up yet,
     * so this is the one place both UIs share the canvas draw. */
    oopsy_view_t boot;
    boot.cat = &cat;
    boot.queue = &queue;
    boot.selected = 0;
    boot.phase = OOPSY_LOADING;
    boot.screen = OOPSY_SCREEN_BROWSE;
    boot.message = "Fetching the catalogue...";
    native_render(disp, &boot);

    const char *msg = 0;
    int ok = load_catalog(&cat, &msg);

    static const char *s_sandbox_req_msg =
        "OOPSy-DAISY requires sandbox escaping to work correctly, please install "
        "sandbox-daemon or use etaHEN";

    int can_esc = oops_system_can_escape_sandbox();
    if (!can_esc) {
        log_error(
            "sandbox escape unavailable: neither etaHEN nor sandbox-daemon detected");
    } else if (oops_fs_exists("/data")) {
        g_already_unsandboxed = 1;
        log_info("already unsandboxed (etaHEN / pre-escaped namespace)");
    }

    const char *ui_msg = ok ? (can_esc ? 0 : s_sandbox_req_msg) : msg;
    const char *ui_title = (!can_esc) ? "Sandbox Escape Required" : "Notice";

#ifdef OOPSY_HAVE_WEBVIEW
    log_info("ui: webview (oops-apps index page)");
    run_webview_ui(disp, &cat, &queue, ui_msg, ui_title);
#else
    log_info("ui: native canvas (webview not in this SDK)");
    oopsy_view_t view;
    view.cat = &cat;
    view.queue = &queue;
    view.selected = 0;
    view.phase = (ok && can_esc) ? OOPSY_BROWSE : OOPSY_FAILED;
    view.screen = OOPSY_SCREEN_BROWSE;
    view.message = ui_msg;
    run_native_ui(disp, &cat, &queue, &view);
#endif

    log_info("exiting");
    oops_keyboard_close();
    oops_input_close();
    oops_display_close(disp);
    oops_net_ctl_term();
    oops_net_term();
    return 0;
}
