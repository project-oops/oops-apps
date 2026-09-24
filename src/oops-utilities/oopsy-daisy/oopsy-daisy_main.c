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
#include "oops/http.h"
#include "oops/input.h"
#include "oops/net.h"
#include "oops/netctl.h"
#include "oops/syscall.h"
#include "oops/system.h"
#include "oops/time.h"
#include "oops/zip.h"

#include "oopsy-daisy.h"

/* The rolling build. Its assets are the source of truth for what can be installed, and matching
 * the web index (project-oops.github.io/oops-apps), so there is one catalogue, not two. */
#define RELEASE_API \
    "https://api.github.com/repos/project-oops/oops-apps/releases/tags/latest-main"

#define DOWNLOAD_TMP  "/data/oopsy-daisy-download.zip"
#define HOMEBREW_ROOT "/data/homebrew"

static void klog(const char *m) { oops_klog("OOPSY", m); }

/* Fetch and parse the catalogue. Returns 1 on success, 0 on failure (with *msg set). */
static int load_catalog(oopsy_catalog_t *cat, const char **msg) {
    oops_http_response_t resp;
    int rc = oops_http_get(RELEASE_API, &resp);
    if (rc == OOPS_HTTP_ERR_TLS_UNAVAIL) {
        *msg = "Secure networking is not available on this system yet.";
        return 0;
    }
    if (rc != OOPS_HTTP_OK) {
        *msg = "Could not reach the release. Check the network.";
        return 0;
    }
    if (resp.status_code != 200 || !resp.body) {
        oops_http_response_free(&resp);
        *msg = "The release did not answer as expected.";
        return 0;
    }
    oopsy_parse_catalog(resp.body, cat);
    oops_http_response_free(&resp);
    if (cat->count == 0) {
        *msg = "No installable titles in the latest build.";
        return 0;
    }
    return 1;
}

/* Download the selected title and unpack it into the homebrew folder. A title `.zip` carries a
 * top-level `<TITLE_ID>/`, so unpacking into /data/homebrew lands it where the console scans. */
static int install(const oopsy_entry_t *e, const char **msg) {
    int rc = oops_http_get_to_file(e->url, DOWNLOAD_TMP);
    if (rc == OOPS_HTTP_ERR_TLS_UNAVAIL) { *msg = "Secure networking unavailable."; return 0; }
    if (rc != OOPS_HTTP_OK) { *msg = "Download failed."; return 0; }

    rc = oops_zip_extract(DOWNLOAD_TMP, HOMEBREW_ROOT);
    (void)oops_fs_unlink(DOWNLOAD_TMP);
    if (rc != OOPS_ZIP_OK) { *msg = "Unpack failed."; return 0; }
    return 1;
}

int oopsy_daisy_start(const payload_args_t *args);

int oopsy_daisy_start(const payload_args_t *args) {
    if (args) sys_call_init(args);
    klog("OOPSy-daisy entry reached");

    oops_time_init();
    oops_net_init();
    oops_net_ctl_init();

    oops_display_t *disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, 1280, 720);
    if (!disp || !oops_display_is_ready(disp)) {
        klog("display would not open");
        oops_net_ctl_term();
        oops_net_term();
        return -1;
    }
    oops_input_init();
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

        oops_pad_state_t pad;
        if (oops_input_poll(0, &pad) == 0) {
            uint32_t pressed = pad.buttons & ~last;
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
            last = pad.buttons;
        }

        oops_surface_t s = oops_display_get_surface(disp);
        if (s.pixels) oopsy_render(&s, &view);
        oops_display_flip(disp);
    }

    klog("OOPSy-daisy exiting");
    oops_input_close();
    oops_display_close(disp);
    oops_net_ctl_term();
    oops_net_term();
    return 0;
}
