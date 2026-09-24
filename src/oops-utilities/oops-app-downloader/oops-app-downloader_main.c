/*
 * oops-app-downloader payload entry.
 *
 * Brings up the network and display and shows the bring-up screen (oops-app-downloader.c),
 * holding until circle. The catalogue/download/unpack pipeline drops in here once oops-sdk
 * gains an HTTPS client and an unzip helper (README.md); for now this reports honestly what is
 * wired and what is pending.
 */

#include "oops/display.h"
#include "oops/draw.h"
#include "oops/input.h"
#include "oops/net.h"
#include "oops/netctl.h"
#include "oops/syscall.h"
#include "oops/system.h"
#include "oops/time.h"

#include "oops-app-downloader.h"

int oops_app_downloader_start(const payload_args_t *args);

int oops_app_downloader_start(const payload_args_t *args) {
    if (args) {
        sys_call_init(args);
    }
    oops_klog("APP-DL", "oops-app-downloader entry reached");

    oops_time_init();
    oops_net_init();
    oops_net_ctl_init();

    oops_net_info_t info;
    for (size_t i = 0; i < sizeof(info); i++) {
        ((unsigned char *)&info)[i] = 0;
    }
    int have_ip = (oops_net_ctl_get_info(&info) == 0 && info.ip_address[0]);

    oops_display_t *disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, 1280, 720);
    if (!disp || !oops_display_is_ready(disp)) {
        oops_klog("APP-DL", "display would not open");
        oops_net_ctl_term();
        oops_net_term();
        return -1;
    }
    oops_input_init();
    oops_system_install_close_handler(); /* cooperate with the dashboard Close (oops/system.h) */

    oops_dl_status_t st;
    st.ip = have_ip ? info.ip_address : (const char *)0;
    st.net_ready = 1;      /* sockets are available today */
    st.install_ready = 1;  /* /data/homebrew is writable via oops/fs.h */
    st.http_ready = 0;     /* pending: oops/http.h */
    st.unzip_ready = 0;    /* pending: oops/zip.h */

    uint32_t last = 0;
    int running = 1;
    while (running) {
        if (oops_system_close_requested()) {
            break;
        }
        oops_pad_state_t pad;
        if (oops_input_poll(0, &pad) == 0) {
            if ((pad.buttons & OOPS_BUTTON_CIRCLE) && !(last & OOPS_BUTTON_CIRCLE)) {
                running = 0;
            }
            last = pad.buttons;
        }

        oops_surface_t surf = oops_display_get_surface(disp);
        if (surf.pixels) {
            (void)oops_dl_render(&surf, &st);
        }
        oops_display_flip(disp);
    }

    oops_klog("APP-DL", "oops-app-downloader exiting");
    oops_input_close();
    oops_display_close(disp);
    oops_net_ctl_term();
    oops_net_term();
    return 0;
}
