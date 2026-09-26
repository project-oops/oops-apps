/*
 * SDL3's video driver for this console, over `oops/gfx.h` and `oops/display.h`.
 *
 * One display, one window, one GL context. The display is open before a payload runs,
 * so there is one display mode and the window is the display: it cannot be moved,
 * resized, hidden or given a border. `oops-deps/sdl2/backend/SDL_prosperovideo.c` is
 * the same driver for SDL2's interface.
 *
 * `GL_SwapWindow` presents through `oops_gfx_present`, not `oops_display_flip`: a GPU
 * back end has a command buffer to submit before the flip.
 */
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_PRIVATE

#include "video/SDL_sysvideo.h"
#include "video/SDL_pixels_c.h"
#include "events/SDL_events_c.h"

#include "oops/display.h"
#include "oops/gfx.h"
#include "oops/keyboard.h"
#include "oops/mouse.h"
#include "oops/system.h"
#include "oops/time.h"

#define PROSPERO_DRIVER_NAME "prospero"

typedef struct {
    oops_gfx_t *gfx;
    oops_display_t *display;
    SDL_Window *window; /* the one window, or NULL before it is created */
    int swap_interval;
    bool keyboard_ready;
    bool mouse_ready;
} PROSPERO_VideoData;

/*
 * The one GL context is the display's and outlives every create/destroy pair, so its
 * handle is the address of a file-scope object: identity is a pointer comparison and
 * destroying it frees nothing.
 */
static const int prospero_the_context = 0;

/* Bring-up. */

static bool PROSPERO_VideoInit(SDL_VideoDevice *_this) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->internal;
    SDL_DisplayMode mode;

    data->gfx = oops_gfx_create(&(oops_gfx_desc_t){
        .width = OOPS_DISPLAY_DEFAULT_WIDTH, .height = OOPS_DISPLAY_DEFAULT_HEIGHT});
    if (!data->gfx) {
        return SDL_SetError("prospero: oops_gfx_create() failed");
    }
    data->display = oops_gfx_display(data->gfx);

    oops_time_init();

    /* The system's close request reaches SDL as a quit event (see `PumpEvents`). */
    oops_system_install_close_handler();

    /* Neither is fatal: no keyboard or mouse is the normal case. */
    data->keyboard_ready = (oops_keyboard_init() == 0);
    data->mouse_ready = (oops_mouse_init() == 0);

    SDL_zero(mode);
    mode.w = (int)oops_display_get_width(data->display);
    mode.h = (int)oops_display_get_height(data->display);
    /* The scanout is 32 bits per pixel with the top byte unused, not alpha. */
    mode.format = SDL_PIXELFORMAT_XRGB8888;
    mode.refresh_rate = 0.0f; /* unknown here; SDL takes 0 as "do not report one" */

    if (SDL_AddBasicVideoDisplay(&mode) == 0) {
        oops_gfx_destroy(data->gfx);
        data->gfx = NULL;
        data->display = NULL;
        return false; /* SDL_AddBasicVideoDisplay has set the error */
    }
    return true;
}

static void PROSPERO_VideoQuit(SDL_VideoDevice *_this) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->internal;

    if (data->mouse_ready) {
        oops_mouse_close();
        data->mouse_ready = false;
    }
    if (data->keyboard_ready) {
        oops_keyboard_close();
        data->keyboard_ready = false;
    }
    if (data->gfx) {
        oops_gfx_destroy(data->gfx);
        data->gfx = NULL;
        data->display = NULL;
    }
    data->window = NULL;
}

/* The window. */

static bool PROSPERO_CreateSDLWindow(SDL_VideoDevice *_this, SDL_Window *window,
                                     SDL_PropertiesID create_props) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->internal;

    (void)create_props;
    if (!data->display) {
        return SDL_SetError("prospero: no display, so no window");
    }
    if (data->window) {
        /* A second window would share the one display. */
        return SDL_SetError("prospero: this display already has a window");
    }

    /* The window is the display, whatever size was asked for. */
    window->w = (int)oops_display_get_width(data->display);
    window->h = (int)oops_display_get_height(data->display);
    window->flags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS;
    window->flags &= ~(SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_MINIMIZED |
                                        SDL_WINDOW_HIDDEN);

    data->window = window;
    SDL_SetKeyboardFocus(window);
    return true;
}

static void PROSPERO_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->internal;

    if (data->window == window) {
        data->window = NULL;
    }
}

/*
 * Both succeed without moving anything: the event reports the geometry the window
 * actually has, as SDL's contract asks, and programs that set a size at start-up run.
 */
static bool PROSPERO_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window *window) {
    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_MOVED, window->x, window->y);
    return true;
}

static void PROSPERO_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window) {
    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_RESIZED, window->w, window->h);
}

/* Events. */

static void PROSPERO_PumpEvents(SDL_VideoDevice *_this) {
    /*
     * The close request is the only event this driver sources; input arrives through
     * its own subsystems. `oops_system_close_requested()` stays set, so the quit is
     * sent once.
     */
    static bool quit_sent = false;

    (void)_this;
    if (!quit_sent && oops_system_close_requested()) {
        quit_sent = true;
        SDL_SendQuit();
    }
}

/* OpenGL. */

static bool PROSPERO_GL_LoadLibrary(SDL_VideoDevice *_this, const char *path) {
    /* oops-gl is linked in; a caller naming a library would not get it. */
    if (path) {
        return SDL_SetError(
            "prospero: GL is linked in; there is no library to load from '%s'", path);
    }
    _this->gl_config.driver_loaded = 1;
    return true;
}

static void PROSPERO_GL_UnloadLibrary(SDL_VideoDevice *_this) {
    _this->gl_config.driver_loaded = 0;
}

/*
 * A title written against desktop GL fills function pointers from names rather than
 * linking the entry points. `oops_gl_get_proc_address` looks each name up in oops-gl's
 * table of core and extension entry points (`oops-sdk/src/gl/gl_procs_core.h`).
 *
 * Weak, so a title linking this driver without oops-gl gets NULL rather than a link
 * error.
 */
__attribute__((weak)) void *oops_gl_get_proc_address(const char *name) {
    (void)name;
    return NULL;
}

static SDL_FunctionPointer PROSPERO_GL_GetProcAddress(SDL_VideoDevice *_this,
                                                      const char *proc) {
    (void)_this;
    return (SDL_FunctionPointer)oops_gl_get_proc_address(proc);
}

static SDL_GLContext PROSPERO_GL_CreateContext(SDL_VideoDevice *_this,
                                               SDL_Window *window) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->internal;

    if (!data->display) {
        SDL_SetError("prospero: no display, so no context");
        return NULL;
    }
    if (window != data->window) {
        SDL_SetError("prospero: that window is not this display's");
        return NULL;
    }

    /* The configuration the display has: 32-bit XRGB, 24-bit depth, 8-bit stencil,
     * no multisampling. */
    _this->gl_config.red_size = 8;
    _this->gl_config.green_size = 8;
    _this->gl_config.blue_size = 8;
    _this->gl_config.alpha_size = 0; /* the top byte is unused, not alpha */
    _this->gl_config.buffer_size = 32;
    _this->gl_config.depth_size = 24;
    _this->gl_config.stencil_size = 8;
    _this->gl_config.double_buffer = 1;
    _this->gl_config.stereo = 0;
    _this->gl_config.multisamplebuffers = 0;
    _this->gl_config.multisamplesamples = 0;
    _this->gl_config.accelerated = 1;

    return (SDL_GLContext)&prospero_the_context;
}

static bool PROSPERO_GL_MakeCurrent(SDL_VideoDevice *_this, SDL_Window *window,
                                    SDL_GLContext context) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->internal;

    /* Both NULL releases the context. */
    if (!context && !window) {
        return true;
    }
    if (context != (SDL_GLContext)&prospero_the_context) {
        return SDL_SetError("prospero: there is one context and that is not it");
    }
    if (window && window != data->window) {
        return SDL_SetError("prospero: that window is not this display's");
    }
    return true;
}

static bool PROSPERO_GL_DestroyContext(SDL_VideoDevice *_this, SDL_GLContext context) {
    (void)_this;
    (void)context;
    /* The context is the display's; `VideoQuit` owns it. */
    return true;
}

/* The flip is always on vsync, so any interval but 1 is refused. */
static bool PROSPERO_GL_SetSwapInterval(SDL_VideoDevice *_this, int interval) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->internal;

    if (interval != 1) {
        return SDL_SetError(
            "prospero: the flip is on vsync, so the swap interval is 1");
    }
    data->swap_interval = 1;
    return true;
}

static bool PROSPERO_GL_GetSwapInterval(SDL_VideoDevice *_this, int *interval) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->internal;

    if (interval) {
        *interval = data->swap_interval;
    }
    return true;
}

static bool PROSPERO_GL_SwapWindow(SDL_VideoDevice *_this, SDL_Window *window) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->internal;

    if (window != data->window) {
        return SDL_SetError("prospero: that window is not this display's");
    }
    /* Through the renderer, which submits a GPU back end's commands before the flip. */
    if (oops_gfx_present(data->gfx) != 0) {
        return SDL_SetError("prospero: oops_gfx_present() failed");
    }
    return true;
}

/* The device. */

static void PROSPERO_DeleteDevice(SDL_VideoDevice *device) {
    SDL_free(device->internal);
    SDL_free(device);
}

static SDL_VideoDevice *PROSPERO_CreateDevice(void) {
    SDL_VideoDevice *device;
    PROSPERO_VideoData *data;

    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        return NULL;
    }
    data = (PROSPERO_VideoData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        SDL_free(device);
        return NULL;
    }
    data->swap_interval = 1; /* always vsync */
    device->internal = data;

    device->VideoInit = PROSPERO_VideoInit;
    device->VideoQuit = PROSPERO_VideoQuit;
    device->PumpEvents = PROSPERO_PumpEvents;
    device->CreateSDLWindow = PROSPERO_CreateSDLWindow;
    device->DestroyWindow = PROSPERO_DestroyWindow;
    device->SetWindowPosition = PROSPERO_SetWindowPosition;
    device->SetWindowSize = PROSPERO_SetWindowSize;

    device->GL_LoadLibrary = PROSPERO_GL_LoadLibrary;
    device->GL_UnloadLibrary = PROSPERO_GL_UnloadLibrary;
    device->GL_GetProcAddress = PROSPERO_GL_GetProcAddress;
    device->GL_CreateContext = PROSPERO_GL_CreateContext;
    device->GL_MakeCurrent = PROSPERO_GL_MakeCurrent;
    device->GL_DestroyContext = PROSPERO_GL_DestroyContext;
    device->GL_SetSwapInterval = PROSPERO_GL_SetSwapInterval;
    device->GL_GetSwapInterval = PROSPERO_GL_GetSwapInterval;
    device->GL_SwapWindow = PROSPERO_GL_SwapWindow;

    device->free = PROSPERO_DeleteDevice;

    /* No `CreateWindowFramebuffer`: nothing would present SDL's software surface, so
     * `SDL_GetWindowSurface` fails and a program uses the renderer. */

    return device;
}

VideoBootStrap PRIVATE_bootstrap = {
    PROSPERO_DRIVER_NAME, "OOPS Prospero video driver", PROSPERO_CreateDevice,
    NULL, /* no ShowMessageBox: `oops/dialog.h` has one, but it is modal and
           * system-owned, and SDL calls this before the video subsystem is up */
    true  /* preferred: the only driver that can show anything */
};

#endif /* SDL_VIDEO_DRIVER_PRIVATE */
