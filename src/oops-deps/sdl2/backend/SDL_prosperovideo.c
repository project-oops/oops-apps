/*
 * The console video driver for upstream SDL2: the display, the one window, and the GL
 * context.
 *
 * oops-gl has one implicit context, created with the display. SDL receives a sentinel
 * handle for it; `MakeCurrent` on any other context fails. `GL_CreateContext`
 * writes the display's real format into `gl_config`, so `SDL_GL_GetAttribute` reports
 * what the display is rather than what was requested.
 */
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_PROSPERO

#include "SDL_video.h"
#include "video/SDL_sysvideo.h"
#include "video/SDL_pixels_c.h"
#include "events/SDL_events_c.h"
#include "events/SDL_keyboard_c.h"

#include "SDL_prosperovideo.h"
#include "SDL_prosperoevents_c.h"

#include "oops/dialog.h"
#include "oops/display.h"
#include "oops/keyboard.h"
#include "oops/mouse.h"
#include "oops/system.h"
#include "oops/time.h"

#define PROSPEROVID_DRIVER_NAME "prospero"

/* The one context. Its address is the handle; its value is never read. */
static int prospero_the_context;

/* ---------------------------------------------------------------- lifecycle */

static int PROSPERO_VideoInit(_THIS) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;
    SDL_DisplayMode mode;

    /*
     * `oops_gfx_create` opens the display, creates the GL context and makes it current,
     * dispatching to whichever renderer the title linked, so Mesa keeps its own present
     * path.
     */
    data->gfx =
        oops_gfx_create(&(oops_gfx_desc_t){.width = OOPS_DISPLAY_DEFAULT_WIDTH,
                                           .height = OOPS_DISPLAY_DEFAULT_HEIGHT,
                                           .depth = true,
                                           .vsync = true});
    if (!data->gfx) {
        return SDL_SetError("prospero: the renderer did not come up");
    }
    data->display = oops_gfx_display(data->gfx);

    oops_time_init();

    /* Flags the dashboard's Close, which the event pump turns into SDL_QUIT
     * (oops/system.h). */
    oops_system_install_close_handler();

    /*
     * Keyboard and mouse are optional; their absence is not an error, and the pump
     * skips a device that reported unavailable.
     */
    data->keyboard_ready = (oops_keyboard_init() == 0);
    data->mouse_ready = (oops_mouse_init() == 0);

    /* These flags gate all key and mouse events in `PROSPERO_PumpEvents`, so they are
       logged. */
    oops_log_info("INPUT", "SDL video init: keyboard_ready=%d mouse_ready=%d",
                  data->keyboard_ready, data->mouse_ready);

    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_ARGB8888;
    mode.w = (int)oops_display_get_width(data->display);
    mode.h = (int)oops_display_get_height(data->display);
    mode.refresh_rate = 60;
    mode.driverdata = NULL;

    if (SDL_AddBasicVideoDisplay(&mode) < 0) {
        oops_gfx_destroy(data->gfx);
        data->gfx = NULL;
        data->display = NULL;
        return -1;
    }
    SDL_AddDisplayMode(&_this->displays[0], &mode);

    return 0;
}

static void PROSPERO_VideoQuit(_THIS) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;

    if (data->mouse_ready) {
        oops_mouse_close();
        data->mouse_ready = 0;
    }
    if (data->keyboard_ready) {
        oops_keyboard_close();
        data->keyboard_ready = 0;
    }
    if (data->gfx) {
        oops_gfx_destroy(data->gfx);
        data->gfx = NULL;
        data->display = NULL;
    }
}

/* ------------------------------------------------------------------- window */

static int PROSPERO_CreateSDLWindow(_THIS, SDL_Window *window) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;

    if (data->window) {
        return SDL_SetError("prospero: there is one display, so one window");
    }

    /*
     * The window is the display. Any requested size is replaced with the display's,
     * which SDL reports back to the caller.
     */
    window->x = 0;
    window->y = 0;
    window->w = (int)oops_display_get_width(data->display);
    window->h = (int)oops_display_get_height(data->display);
    window->flags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_SHOWN;
    window->flags &= ~(unsigned int)(SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE |
                                     SDL_WINDOW_MINIMIZED | SDL_WINDOW_BORDERLESS);

    data->window = window;
    window->driverdata = data;

    SDL_SetKeyboardFocus(window);
    SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED, window->w, window->h);
    return 0;
}

static void PROSPERO_DestroyWindow(_THIS, SDL_Window *window) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;

    if (data->window == window) {
        data->window = NULL;
    }
    window->driverdata = NULL;
}

/* There is no title bar; doing nothing is the complete implementation. */
static void PROSPERO_SetWindowTitle(_THIS, SDL_Window *window) {
    (void)_this;
    (void)window;
}

static void PROSPERO_ShowWindow(_THIS, SDL_Window *window) {
    (void)_this;
    (void)window;
}

/* ----------------------------------------------------------------------- GL */

static int PROSPERO_GL_LoadLibrary(_THIS, const char *path) {
    /*
     * oops-gl is linked into the payload. A named library path is refused, since the
     * caller wants a different GL.
     */
    if (path) {
        return SDL_SetError("prospero: GL is linked in, so no library can be loaded");
    }
    _this->gl_config.driver_loaded = 1;
    _this->gl_config.driver_path[0] = '\0';
    _this->gl_config.dll_handle = NULL;
    return 0;
}

static void PROSPERO_GL_UnloadLibrary(_THIS) {
    _this->gl_config.driver_loaded = 0;
}

/*
 * A weak definition, overridden by oops-gl when it is linked, so a title using SDL
 * without GL still links with no undefined symbol. Declared here because <GL/gl.h>
 * conflicts with SDL's own GL declarations.
 */
__attribute__((weak)) void *oops_gl_get_proc_address(const char *name) {
    (void)name;
    return NULL;
}

/* Weak for the same reason; oops-gl's gates its GL 2.0 entry points on this version. */
__attribute__((weak)) unsigned char glContextSetVersion(unsigned int major,
                                                        unsigned int minor) {
    (void)major;
    (void)minor;
    return 0;
}

static void *PROSPERO_GL_GetProcAddress(_THIS, const char *proc) {
    (void)_this;
    /*
     * Desktop-GL titles load post-1.1 entry points by name, so the linker never sees
     * them. oops-gl looks the name up in the payload's dynamic symbol table and
     * answers NULL for a name it does not have.
     */
    return oops_gl_get_proc_address(proc);
}

static SDL_GLContext PROSPERO_GL_CreateContext(_THIS, SDL_Window *window) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;

    if (!data->display) {
        SDL_SetError("prospero: no display, so no context");
        return NULL;
    }
    if (window != data->window) {
        SDL_SetError("prospero: that window is not this display's");
        return NULL;
    }

    /*
     * The display's actual format. Without this, gl_config keeps the requested values
     * and SDL_GL_GetAttribute reports them.
     */
    _this->gl_config.red_size = 8;
    _this->gl_config.green_size = 8;
    _this->gl_config.blue_size = 8;
    _this->gl_config.alpha_size = 8;
    _this->gl_config.buffer_size = 32;
    _this->gl_config.depth_size = 24;
    _this->gl_config.stencil_size = 8;
    _this->gl_config.double_buffer = 1;
    _this->gl_config.stereo = 0;
    _this->gl_config.multisamplebuffers = 0;
    _this->gl_config.multisamplesamples = 0;
    _this->gl_config.accelerated = 1;

    /* An ES 2.0 request is a request for the programmable pipeline, which oops-gl
     * enables at version 2.0. Desktop requests keep oops-gl's default: SDL's own
     * default is 2.1, so honouring it would move every fixed-function title onto
     * the 2.x badge. */
    if (_this->gl_config.profile_mask == SDL_GL_CONTEXT_PROFILE_ES &&
        _this->gl_config.major_version >= 2) {
        if (!glContextSetVersion(2, 0)) {
            SDL_SetError("prospero: oops-gl refused GL 2.0 for an ES 2.0 context");
            return NULL;
        }
    }

    return (SDL_GLContext)&prospero_the_context;
}

static int PROSPERO_GL_MakeCurrent(_THIS, SDL_Window *window, SDL_GLContext context) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;

    /* Both NULL releases the context, which succeeds. */
    if (!context && !window) {
        return 0;
    }
    if (context != (SDL_GLContext)&prospero_the_context) {
        return SDL_SetError("prospero: there is one context and that is not it");
    }
    if (window && window != data->window) {
        return SDL_SetError("prospero: that window is not this display's");
    }
    return 0;
}

static void PROSPERO_GL_DeleteContext(_THIS, SDL_GLContext context) {
    (void)_this;
    (void)context;
    /* The context belongs to the display and VideoQuit closes it. */
}

static void PROSPERO_GL_GetDrawableSize(_THIS, SDL_Window *window, int *w, int *h) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;

    (void)window;
    if (w) {
        *w = (int)oops_display_get_width(data->display);
    }
    if (h) {
        *h = (int)oops_display_get_height(data->display);
    }
}

static int PROSPERO_GL_SetSwapInterval(_THIS, int interval) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;

    /* The flip is always on vsync, so only an interval of 1 is accepted. */
    if (interval != 1) {
        return SDL_SetError(
            "prospero: the flip is on vsync, so the swap interval is 1");
    }
    data->swap_interval = 1;
    return 0;
}

static int PROSPERO_GL_GetSwapInterval(_THIS) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;
    return data->swap_interval;
}

static int PROSPERO_GL_SwapWindow(_THIS, SDL_Window *window) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;

    if (window != data->window) {
        return SDL_SetError("prospero: that window is not this display's");
    }
    /* Present through the renderer, not a bare display flip: the flip belongs to
     * whichever backend drew the frame. oops_gfx_present returns true on
     * success; false means the frame is not on screen. */
    if (!oops_gfx_present(data->gfx)) {
        return SDL_SetError("prospero: the present failed (%d)",
                            oops_display_get_last_error(data->display));
    }
    return 0;
}

/* ---------------------------------------------------------------- bootstrap */

static void PROSPERO_DeleteDevice(SDL_VideoDevice *device) {
    SDL_free(device->driverdata);
    SDL_free(device);
}

static SDL_VideoDevice *PROSPERO_CreateDevice(void) {
    SDL_VideoDevice *device;
    PROSPERO_VideoData *data;

    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return NULL;
    }
    data = (PROSPERO_VideoData *)SDL_calloc(1, sizeof(PROSPERO_VideoData));
    if (!data) {
        SDL_free(device);
        SDL_OutOfMemory();
        return NULL;
    }
    data->swap_interval = 1;
    device->driverdata = data;

    device->VideoInit = PROSPERO_VideoInit;
    device->VideoQuit = PROSPERO_VideoQuit;
    device->PumpEvents = PROSPERO_PumpEvents;

    device->CreateSDLWindow = PROSPERO_CreateSDLWindow;
    device->DestroyWindow = PROSPERO_DestroyWindow;
    device->SetWindowTitle = PROSPERO_SetWindowTitle;
    device->ShowWindow = PROSPERO_ShowWindow;

    device->GL_LoadLibrary = PROSPERO_GL_LoadLibrary;
    device->GL_UnloadLibrary = PROSPERO_GL_UnloadLibrary;
    device->GL_GetProcAddress = PROSPERO_GL_GetProcAddress;
    device->GL_CreateContext = PROSPERO_GL_CreateContext;
    device->GL_MakeCurrent = PROSPERO_GL_MakeCurrent;
    device->GL_GetDrawableSize = PROSPERO_GL_GetDrawableSize;
    device->GL_SetSwapInterval = PROSPERO_GL_SetSwapInterval;
    device->GL_GetSwapInterval = PROSPERO_GL_GetSwapInterval;
    device->GL_SwapWindow = PROSPERO_GL_SwapWindow;
    device->GL_DeleteContext = PROSPERO_GL_DeleteContext;

    device->free = PROSPERO_DeleteDevice;

    return device;
}

/*
 * The system message dialog, when the title has it, and weakly referenced so that it does not put
 * `dialog` on the capability list of every title that links SDL. Absent, the text still reaches the
 * log and the default button answers.
 */
#pragma weak oops_dialog_message_show
#pragma weak oops_dialog_message_poll
#pragma weak oops_dialog_message_close

/* The button the port itself marked as the return-key default, or its first, or -1 for no buttons. */
static int PROSPERO_DefaultButton(const SDL_MessageBoxData *data) {
    for (int i = 0; i < data->numbuttons; i++) {
        if (data->buttons[i].flags & SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT) {
            return data->buttons[i].buttonid;
        }
    }
    return data->numbuttons > 0 ? data->buttons[0].buttonid : -1;
}

/*
 * A message box, on screen where the console can draw one and in the log either way.
 *
 * A port reaches for a message box at the point where it has something the player must read - a
 * missing asset archive, an unsupported ROM - and then acts on the answer. Leaving the hook null
 * makes SDL return an error without touching `buttonid`, so the port reads its own uninitialised
 * stack and exits with nothing said. Both halves of that are worth fixing.
 *
 * A two-button box maps to the system Yes/No dialog and the player's answer is returned. Anything
 * else is shown as OK and answered with the port's own default, so a box with three buttons is
 * reported as such in the log rather than silently reduced.
 */
static int PROSPERO_ShowMessageBox(const SDL_MessageBoxData *data, int *buttonid) {
    if (!data || !buttonid) return SDL_InvalidParamError("messageboxdata");

    oops_log_warn("SDL", "message box: %s", data->title ? data->title : "(no title)");
    if (data->message) oops_log_warn("SDL", "  %s", data->message);
    if (data->numbuttons > 2) {
        oops_log_warn("SDL", "  %d buttons; the console dialog offers two, so this one is OK-only",
                      data->numbuttons);
    }

    *buttonid = PROSPERO_DefaultButton(data);

    if (&oops_dialog_message_show && &oops_dialog_message_poll && &oops_dialog_message_close) {
        const int yesno = (data->numbuttons == 2);
        char text[1024];
        SDL_snprintf(text, sizeof(text), "%s\n\n%s", data->title ? data->title : "",
                     data->message ? data->message : "");
        if (oops_dialog_message_show(text, yesno ? OOPS_MSG_DIALOG_BTN_YESNO
                                                 : OOPS_MSG_DIALOG_BTN_OK) == 0) {
            oops_msg_dialog_result_t result = OOPS_MSG_DIALOG_RES_INVALID;
            int rc;
            while ((rc = oops_dialog_message_poll(&result)) == 0) {
                oops_time_sleep_ms(16);
            }
            oops_dialog_message_close();
            if (rc == 1 && yesno && result != OOPS_MSG_DIALOG_RES_INVALID) {
                /* Yes is the first button, No the second: the order SDL_MessageBoxData lists them. */
                *buttonid = data->buttons[result == OOPS_MSG_DIALOG_RES_OK ? 0 : 1].buttonid;
            }
            oops_log_warn("SDL", "  answered on screen with button id %d", *buttonid);
            return 0;
        }
        oops_log_warn("SDL", "  the system dialog would not open; answering with the default");
    }

    if (data->numbuttons > 0) {
        oops_log_warn("SDL", "  answered with button id %d, the port's own default", *buttonid);
    }
    return 0;
}

VideoBootStrap PROSPERO_bootstrap = {
    PROSPEROVID_DRIVER_NAME, "OOPS console video driver", PROSPERO_CreateDevice,
    PROSPERO_ShowMessageBox
};

#endif /* SDL_VIDEO_DRIVER_PROSPERO */
