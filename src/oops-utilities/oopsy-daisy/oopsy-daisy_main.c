/*
 * OOPSy-daisy payload entry - the console-only half.
 *
 * Brings up the network and display, fetches the oops-apps release over HTTPS, lets you pick a
 * title with the pad, and installs it by downloading its `.zip` and unpacking it into the
 * homebrew folder. The catalogue parse and the drawing are oopsy-daisy.c; this wires them to
 * oops/http.h and oops/zip.h.
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
#include "oops/time.h"

/* The installer half needs the SDK's HTTPS client and zip extractor - recent additions. Guard
 * on them so OOPSy-daisy builds either way: with them it installs; without, it says the support
 * is pending rather than failing to compile. */
#if defined(__has_include)
#  if __has_include(<oops/http.h>) && __has_include(<oops/zip.h>)
#    include <oops/http.h>
#    include <oops/zip.h>
#    define OOPSY_HAVE_INSTALLER 1
#  endif
#endif

#include "oopsy-daisy.h"

/* The rolling build. Its assets are the source of truth for what can be installed, and matching
 * the web index (project-oops.github.io/oops-apps), so there is one catalogue, not two. */
#define RELEASE_API \
    "https://api.github.com/repos/project-oops/oops-apps/releases/tags/latest-main"

#define DOWNLOAD_TMP  "/data/oopsy-daisy-download.zip"
#define HOMEBREW_ROOT "/data/homebrew"

/* Lifecycle at INFO, failures at ERROR, through the SDK's levelled klog so OOPSy-daisy honours
 * /app0/oops-log (an `OOPSY=level` line) exactly like the rest of the collection. */
static void log_info(const char *m)  { oops_klog_level(OOPS_LOG_INFO,  "OOPSY", m); }
static void log_error(const char *m) { oops_klog_level(OOPS_LOG_ERROR, "OOPSY", m); }

/* Draw the current view now (surface + flip). Shared by the loop and the download progress
 * callback, so the queue's bar advances during an otherwise-blocking fetch. */
static void render_now(oops_display_t *disp, const oopsy_view_t *view) {
    oops_surface_t s = oops_display_get_surface(disp);
    if (s.pixels) oopsy_render(&s, view);
    oops_display_flip(disp);
}

#ifdef OOPSY_HAVE_INSTALLER

/* Name the specific HTTP failure, so a wall shows *which* step broke - on screen and in the log -
 * instead of one catch-all. The release-asset URL 302-redirects to a CDN host, so a failure to
 * follow that is the likely `PARSE` case. */
static const char *download_err_msg(int rc) {
    switch (rc) {
        case OOPS_HTTP_ERR_TLS_UNAVAIL: return "Download failed: secure networking unavailable.";
        case OOPS_HTTP_ERR_RESOLVE:     return "Download failed: could not resolve the host.";
        case OOPS_HTTP_ERR_CONNECT:     return "Download failed: could not connect (TLS?).";
        case OOPS_HTTP_ERR_SEND:        return "Download failed: request send failed.";
        case OOPS_HTTP_ERR_RECV:        return "Download failed: receive failed.";
        case OOPS_HTTP_ERR_PARSE:       return "Download failed: bad response (a redirect not followed?).";
        case OOPS_HTTP_ERR_WRITE:       return "Download failed: could not write the file to /data.";
        case OOPS_HTTP_ERR_NOMEM:       return "Download failed: out of memory.";
        default:                        return "Download failed.";
    }
}

/* Fetch and parse the catalogue. Returns 1 on success, 0 on failure (with *msg set). Logged at
 * INFO for the steps and DEBUG for the return codes, so turning `OOPSY` up in /app0/oops-log
 * traces the whole fetch without a rebuild. */
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
        oops_kprintf_level(OOPS_LOG_ERROR, "OOPSY", "catalogue: unexpected status=%d body=%d",
                           resp.status_code, resp.body ? 1 : 0);
        oops_http_response_free(&resp);
        *msg = "The release did not answer as expected.";
        return 0;
    }
    oopsy_parse_catalog(resp.body, cat);
    oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "catalogue: %d installable titles", cat->count);
    oops_http_response_free(&resp);
    if (cat->count == 0) {
        *msg = "No installable titles in the latest build.";
        return 0;
    }
    return 1;
}

/* Carried through oops_http_get_to_file_cb so its per-chunk callback can advance the job and
 * repaint the queue while a (blocking) download runs. */
typedef struct {
    oopsy_job_t *job;
    oops_display_t *disp;
    const oopsy_view_t *view;
    int last_pct;
} oopsy_dl_ctx_t;

/* Per chunk: record the bytes, and repaint only when the bar would actually move. Repainting on
 * every chunk would gate the download on vsync; a percent change is smooth and cheap. */
static void on_dl_progress(uint64_t downloaded, uint64_t total, void *ud) {
    oopsy_dl_ctx_t *c = (oopsy_dl_ctx_t *)ud;
    c->job->done = (unsigned)downloaded;
    if (total) c->job->total = (unsigned)total;   /* the catalogue size is the fallback */
    int pct = oopsy_job_pct(c->job);
    if (pct != c->last_pct) { c->last_pct = pct; render_now(c->disp, c->view); }
}

/* Work one job to completion: download the .zip with a live bar, then unpack it into the homebrew
 * folder (a title zip carries a top-level `<TITLE_ID>/`). Sets the job's final state; a failure
 * here stops only this job. */
static void process_job(oopsy_job_t *job, oops_display_t *disp, const oopsy_view_t *view) {
    oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "install: GET %s", job->url);
    job->state = OOPSY_JOB_DOWNLOADING;
    job->done = 0;
    render_now(disp, view);

    (void)oops_fs_unlink(DOWNLOAD_TMP);
    oopsy_dl_ctx_t ctx = { job, disp, view, -1 };
    int rc = oops_http_get_to_file_cb(job->url, DOWNLOAD_TMP, on_dl_progress, &ctx);
    oops_kprintf_level(OOPS_LOG_DEBUG, "OOPSY", "install: download rc=%d", rc);
    if (rc != OOPS_HTTP_OK) {
        job->state = OOPSY_JOB_FAILED;
        job->error = download_err_msg(rc);
        log_error(job->error);
        render_now(disp, view);
        return;
    }

    job->state = OOPSY_JOB_INSTALLING;
    render_now(disp, view);
    rc = oops_zip_extract(DOWNLOAD_TMP, HOMEBREW_ROOT);
    (void)oops_fs_unlink(DOWNLOAD_TMP);
    oops_kprintf_level(OOPS_LOG_DEBUG, "OOPSY", "install: unzip rc=%d", rc);
    if (rc != OOPS_ZIP_OK) {
        job->state = OOPSY_JOB_FAILED;
        job->error = "Unpack failed.";
        log_error("install: unpack failed");
    } else {
        job->state = OOPSY_JOB_DONE;
        job->done = job->total;
        oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "install: %s installed", job->name);
    }
    render_now(disp, view);
}

#else  /* the SDK does not carry oops/http.h + oops/zip.h yet */

static int load_catalog(oopsy_catalog_t *cat, const char **msg) {
    (void)cat;
    *msg = "On-device install needs the SDK HTTPS + unzip support (pending).";
    return 0;
}
static void process_job(oopsy_job_t *job, oops_display_t *disp, const oopsy_view_t *view) {
    (void)disp; (void)view;
    job->state = OOPSY_JOB_FAILED;
    job->error = "On-device install pending SDK support.";
}

#endif /* OOPSY_HAVE_INSTALLER */

int oopsy_daisy_start(const payload_args_t *args);

int oopsy_daisy_start(const payload_args_t *args) {
    if (args) sys_call_init(args);
    log_info("entry reached");

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
    oops_keyboard_init();   /* so a USB keyboard drives the same nav as the pad */
    oops_system_install_close_handler();

    oopsy_catalog_t cat;
    cat.count = 0;
    oopsy_queue_t queue;
    queue.count = 0;
    oopsy_view_t view;
    view.cat = &cat;
    view.queue = &queue;
    view.selected = 0;
    view.phase = OOPSY_LOADING;
    view.screen = OOPSY_SCREEN_BROWSE;
    view.message = "Fetching the catalogue...";

    render_now(disp, &view);   /* a loading frame before the blocking fetch */

    const char *msg = 0;
    if (load_catalog(&cat, &msg)) {
        view.phase = OOPSY_BROWSE;
        view.message = 0;
    } else {
        view.phase = OOPSY_FAILED;
        view.message = msg;
    }

    uint32_t last = 0;
    int running = 1;
    while (running) {
        if (oops_system_close_requested()) break;

        /* Pad and keyboard map to the same OOPS_BUTTON_* bits, so read the keyboard always and OR
         * the pad on top - either drives the menu. */
        oops_pad_state_t pad;
        uint32_t buttons = oops_keyboard_poll_buttons();
        if (oops_input_poll(0, &pad) == 0) buttons |= pad.buttons;
        uint32_t pressed = buttons & ~last;
        last = buttons;

        if (view.screen == OOPSY_SCREEN_QUEUE) {
            if (pressed & OOPS_BUTTON_CIRCLE) view.screen = OOPSY_SCREEN_BROWSE;    /* back to list */
        } else {
            if (pressed & OOPS_BUTTON_CIRCLE) running = 0;                          /* exit */
            if (pressed & OOPS_BUTTON_SQUARE) view.screen = OOPSY_SCREEN_QUEUE;     /* open queue */
            if (view.phase == OOPSY_BROWSE && cat.count > 0) {
                if ((pressed & OOPS_BUTTON_UP) && view.selected > 0) view.selected--;
                if ((pressed & OOPS_BUTTON_DOWN) && view.selected < cat.count - 1) view.selected++;
                if (pressed & OOPS_BUTTON_CROSS) oopsy_queue_add(&queue, &cat.items[view.selected]);
            }
        }

        render_now(disp, &view);

        /* Work the queue one job at a time: process_job blocks (with a live bar) until its job is
         * done, then the loop resumes for input and the next job. */
        int ai = oopsy_queue_active(&queue);
        if (ai >= 0 && queue.jobs[ai].state == OOPSY_JOB_QUEUED) {
            process_job(&queue.jobs[ai], disp, &view);
        }
    }

    log_info("exiting");
    oops_keyboard_close();
    oops_input_close();
    oops_display_close(disp);
    oops_net_ctl_term();
    oops_net_term();
    return 0;
}
