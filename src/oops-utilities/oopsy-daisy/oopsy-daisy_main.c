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

/* Download the selected title and unpack it into the homebrew folder. A title `.zip` carries a
 * top-level `<TITLE_ID>/`, so unpacking into /data/homebrew lands it where the console scans.
 * Each step is logged so a failed install says where it broke. */
static int install(const oopsy_entry_t *e, const char **msg) {
    oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "install: GET %s", e->url);
    int rc = oops_http_get_to_file(e->url, DOWNLOAD_TMP);
    oops_kprintf_level(OOPS_LOG_DEBUG, "OOPSY", "install: download rc=%d", rc);
    if (rc != OOPS_HTTP_OK) { *msg = download_err_msg(rc); log_error(*msg); return 0; }

    rc = oops_zip_extract(DOWNLOAD_TMP, HOMEBREW_ROOT);
    (void)oops_fs_unlink(DOWNLOAD_TMP);
    oops_kprintf_level(OOPS_LOG_DEBUG, "OOPSY", "install: unzip rc=%d", rc);
    if (rc != OOPS_ZIP_OK) { *msg = "Unpack failed."; log_error("install: unpack failed"); return 0; }
    oops_kprintf_level(OOPS_LOG_INFO, "OOPSY", "install: %s installed", e->name);
    return 1;
}

#else  /* the SDK does not carry oops/http.h + oops/zip.h yet */

static int load_catalog(oopsy_catalog_t *cat, const char **msg) {
    (void)cat;
    *msg = "On-device install needs the SDK HTTPS + unzip support (pending).";
    return 0;
}
static int install(const oopsy_entry_t *e, const char **msg) {
    (void)e;
    *msg = "On-device install pending SDK support.";
    return 0;
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
    oopsy_view_t view;
    view.cat = &cat;
    view.selected = 0;
    view.phase = OOPSY_LOADING;
    view.message = "Fetching the catalogue...";

    /* Draw the loading screen once before the (blocking) fetch, so it is not a black frame. */
    {
        oops_surface_t s = oops_display_get_surface(disp);
        if (s.pixels) oopsy_render(&s, &view);
        oops_display_flip(disp);
    }

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

        /* Pad and keyboard both map to the same OOPS_BUTTON_* bits (arrows -> UP/DOWN,
         * Enter -> CROSS, Esc -> CIRCLE), so read the keyboard always and OR the pad on top -
         * either drives the menu, and neither being attached is fine. */
        oops_pad_state_t pad;
        uint32_t buttons = oops_keyboard_poll_buttons();
        if (oops_input_poll(0, &pad) == 0) buttons |= pad.buttons;
        uint32_t pressed = buttons & ~last;
        if (pressed & OOPS_BUTTON_CIRCLE) running = 0;

        if (view.phase == OOPSY_BROWSE || view.phase == OOPSY_DONE ||
            view.phase == OOPSY_FAILED) {
            if ((pressed & OOPS_BUTTON_UP) && view.selected > 0) view.selected--;
            if ((pressed & OOPS_BUTTON_DOWN) && view.selected < cat.count - 1) view.selected++;

            if ((pressed & OOPS_BUTTON_CROSS) && cat.count > 0) {
                view.phase = OOPSY_INSTALLING;
                view.message = "Installing...";
                oops_surface_t s = oops_display_get_surface(disp);
                if (s.pixels) oopsy_render(&s, &view);
                oops_display_flip(disp);

                const char *im = 0;
                if (install(&cat.items[view.selected], &im)) {
                    view.phase = OOPSY_DONE;
                    view.message = "Installed. Launch it from the dashboard.";
                } else {
                    view.phase = OOPSY_FAILED;
                    view.message = im;
                }
            }
        }
        last = buttons;

        oops_surface_t s = oops_display_get_surface(disp);
        if (s.pixels) oopsy_render(&s, &view);
        oops_display_flip(disp);
    }

    log_info("exiting");
    oops_keyboard_close();
    oops_input_close();
    oops_display_close(disp);
    oops_net_ctl_term();
    oops_net_term();
    return 0;
}
