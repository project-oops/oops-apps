/*
 * home payload entry.
 *
 * Executed by a homebrew ELF loader (elfldr) with payload_args in rdi. Opens the display and the
 * pad, then loops: read the pad, apply it to the model, draw, flip.
 *
 * The model, the skins and the drawing are `home.c`, shared with the host self-test. This file is
 * the console-only part: the display, the input, the loop, and **the host dispatch** - the seam
 * where a shell action becomes something a machine actually does.
 */

#include "oops/display.h"
#include "oops/draw.h"
#include "oops/input.h"
#include "oops/krw.h"
#include "oops/syscall.h"

#include "home.h"

/* A line to the system log, the one output a payload always has. */
static void klog(const char *msg) {
    char buf[192];
    const char *prefix = "[HOME] ";
    int n = 0;
    while (prefix[n] != '\0' && n < 16) {
        buf[n] = prefix[n];
        n++;
    }
    int m = 0;
    while (msg[m] != '\0' && n < (int)sizeof(buf) - 2) {
        buf[n] = msg[m];
        n++;
        m++;
    }
    buf[n] = '\n';
    n++;
    buf[n] = '\0';
    (void)sys_call(SYS_klog, 7, (long)buf, 0, 0, 0, 0);
}

/*
 * The host dispatch seam.
 *
 * In Orbistoun, this dispatch hooks directly into emulator services (process loader, package
 * manager, save state manager, frame grabber). On console hardware, these call into system
 * daemons. Returning 0 tells the model the action was not handled so it can present feedback.
 */
static int console_perform(void *ctx, home_action_t action, int arg) {
    (void)ctx;
    (void)arg;
    switch (action) {
        case HOME_ACTION_LAUNCH_TITLE:
            klog("launch requested - delegating to title loader");
            return 0;
        case HOME_ACTION_SUSPEND_TITLE:
            klog("suspend requested");
            return 0;
        case HOME_ACTION_RESUME_TITLE:
            klog("resume requested");
            return 0;
        case HOME_ACTION_TERMINATE_TITLE:
            klog("terminate requested");
            return 0;
        case HOME_ACTION_INSTALL_PACKAGE:
            klog("install package requested");
            return 0;
        case HOME_ACTION_RUN_PAYLOAD:
            klog("run payload requested");
            return 0;
        case HOME_ACTION_DELETE_TITLE:
            klog("delete title requested");
            return 0;
        case HOME_ACTION_CHECK_UPDATE:
            klog("check update requested");
            return 0;
        case HOME_ACTION_MANAGE_CONTENT:
            klog("manage content requested");
            return 0;
        case HOME_ACTION_SYNC_SAVE:
        case HOME_ACTION_EXPORT_SAVE:
        case HOME_ACTION_IMPORT_SAVE:
        case HOME_ACTION_DELETE_SAVE:
            klog("save data operation requested");
            return 0;
        case HOME_ACTION_SAVE_STATE:
            klog("emulator save state requested");
            return 0;
        case HOME_ACTION_LOAD_STATE:
            klog("emulator load state requested");
            return 0;
        case HOME_ACTION_TAKE_SCREENSHOT:
            klog("screenshot capture requested");
            return 0;
        case HOME_ACTION_TOGGLE_MUSIC:
            klog("music playback toggle requested");
            return 0;
        case HOME_ACTION_REST_MODE:
        case HOME_ACTION_RESTART:
        case HOME_ACTION_POWER_OFF:
            klog("power action requested");
            return 0;
        default:
            return 0;
    }
}

/*
 * The exit gesture: L1, R1 and Options together.
 */
static int exit_combo(uint32_t buttons) {
    return ((buttons & OOPS_BUTTON_L1) != 0u) &&
           ((buttons & OOPS_BUTTON_R1) != 0u) &&
           ((buttons & OOPS_BUTTON_OPTIONS) != 0u);
}

int home_start(const payload_args_t *args);

int home_start(const payload_args_t *args) {
    if (args != 0) {
        sys_call_init(args);
    }
    klog("home payload entry reached");

    oops_display_t *disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, 1280, 720);
    if (disp == 0 || !oops_display_is_ready(disp)) {
        klog("display would not open - see oops_display_get_last_error");
        return -1;
    }
    oops_input_init();

    home_model_t model;
    home_model_init(&model);

    home_host_t host;
    host.ctx = 0;
    host.perform = console_perform;
    home_set_host(&model, &host);

    home_input_t input;
    home_input_reset(&input);

    klog("home: cross selects, circle backs, square library, triangle search, "
         "options context menu, PS button control centre, L1/R1 games/media, L1+R1+options exits");

    int running = 1;
    while (running != 0) {
        oops_pad_state_t pad;
        uint32_t buttons = 0u;
        if (oops_input_poll(0, &pad) == 0) {
            buttons = pad.buttons;
        }

        if (exit_combo(buttons) != 0) {
            running = 0;
        } else {
            (void)home_input_apply(&input, &model, buttons);
        }

        home_tick(&model);

        oops_surface_t surf = oops_display_get_surface(disp);
        if (surf.pixels != 0) {
            (void)home_render(&surf, &model, home_theme_at(model.theme));
        }
        oops_display_flip(disp);
    }

    klog("home exiting");
    oops_input_close();
    oops_display_close(disp);
    return 0;
}
