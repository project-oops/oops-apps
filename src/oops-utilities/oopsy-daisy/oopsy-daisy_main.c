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

#include "oopsy-daisy.h"

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

static const char *get_download_tmp(void) {
    if (oops_fs_exists("/data")) {
        return "/data/oopsy-daisy-download.zip";
    }
    return "/app0/oopsy-daisy-download.zip";
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

    const char *dl_path = get_download_tmp();
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

    /* Now escape the sandbox to write to /data/homebrew */
    int esc_rc = oops_system_escape_sandbox();
    oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "install: sandbox escape rc=%d", esc_rc);
    if (esc_rc != 0) {
        oops_fs_free_data(zip_data);
        job->state = OOPSY_JOB_FAILED;
        job->error = "Sandbox escape failed.";
        log_error("install: sandbox escape failed");
        if (repaint)
            repaint(rctx);
        return;
    }
    (void)oops_fs_mkdir(HOMEBREW_ROOT, 0777);

    rc = oops_zip_extract_mem(zip_data, zip_size, HOMEBREW_ROOT);
    oops_fs_free_data(zip_data);
    oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "install: %s unzip rc=%d", job->name,
                       rc);
    if (rc != OOPS_ZIP_OK) {
        job->state = OOPSY_JOB_FAILED;
        job->error = "Unpack failed.";
        log_error("install: unpack failed");
    } else {
        job->state = OOPSY_JOB_DONE;
        job->done = job->total;
        oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "install: %s installed", job->name);
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

/* oopsyError('msg') - show a banner. The message is one of the app's fixed strings,
 * but any quote or newline is turned to a space so the composed statement is always
 * well-formed. */
static void wv_error(oops_webview_t *wv, const char *msg) {
    char code[320];
    int p = 0;
    const char *pre = "oopsyError('";
    for (int k = 0; pre[k] && p < (int)sizeof code - 4; k++)
        code[p++] = pre[k];
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
                          oopsy_queue_t *queue, const char *load_msg) {
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
    oops_js_register_fn(js, "__oopsy_scroll", cb_scroll, &bridge);

    for (int i = 0; i < 8; i++)
        oops_webview_pump(wv); /* let scripts + first layout settle */
    wv_eval(wv, "oopsyInit();");
    if (load_msg)
        wv_error(wv, load_msg);

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

    int on_queue = 0, running = 1;
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
            if (pressed & OOPS_BUTTON_CIRCLE) {
                on_queue = 0;
                wv_eval(wv, "oopsyScreen('browse');");
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
                if (wv_eval_bool(wv, "oopsyEnter()")) {
                    on_queue = 1;
                    wv_eval(wv, "oopsyScreen('queue');");
                }
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

#ifdef OOPSY_HAVE_WEBVIEW
    log_info("ui: webview (oops-apps index page)");
    run_webview_ui(disp, &cat, &queue, ok ? 0 : msg);
#else
    log_info("ui: native canvas (webview not in this SDK)");
    oopsy_view_t view;
    view.cat = &cat;
    view.queue = &queue;
    view.selected = 0;
    view.phase = ok ? OOPSY_BROWSE : OOPSY_FAILED;
    view.screen = OOPSY_SCREEN_BROWSE;
    view.message = ok ? 0 : msg;
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
