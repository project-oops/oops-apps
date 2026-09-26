/*
 * SDL3's video driver for this console, over `oops/gfx.h` and `oops/display.h`.
 *
 * # One display, one window, one context
 *
 * A desktop's video driver abstracts a window system. This console has a single display
 * that is already open by the time a payload runs, so most of the interface has one
 * honest answer: there is one display mode, `CreateSDLWindow` hands back the display it
 * already had, and the window cannot be moved, resized, hidden or given a border.
 * Saying so plainly is better than a driver that accepts a resize and quietly ignores
 * it.
 *
 * `oops-deps/sdl2/backend/SDL_prosperovideo.c` is the same driver for SDL2, and the
 * shape carries over even though the interface does not: SDL3 renamed `driverdata` to
 * `internal`, made every entry point return `bool` instead of `int`, replaced
 * `GL_DeleteContext` with `GL_DestroyContext` and dropped `GL_GetDrawableSize` in
 * favour of the window's own size.
 *
 * # The present goes through the renderer
 *
 * `GL_SwapWindow` calls `oops_gfx_present`, not `oops_display_flip`. The flip belongs
 * to whichever back end `oops_gfx` is driving - a GPU-accelerated one has a command
 * buffer to submit first, and flipping underneath it shows the previous frame. The SDL2
 * driver records the same.
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
 * **The context is a fixed address, not an allocation.** There is exactly one GL
 * context - the display's - and it outlives every
 * `SDL_GL_CreateContext`/`DestroyContext` pair. Handing back the address of a
 * file-scope object makes "is this our context?" a pointer comparison, and makes
 * destroying it a no-op that cannot accidentally free something the display still owns.
 */
static const int prospero_the_context = 0;

/* ---- bring-up ----------------------------------------------------------- */

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

    /* The system's close request - the PS button's "Close Application" - has to reach
     * SDL as a quit event rather than killing the process from under it. */
    oops_system_install_close_handler();

    /* Neither is fatal: a console with no keyboard or mouse attached is the normal
     * case, and the pad is a joystick rather than either of these. */
    data->keyboard_ready = (oops_keyboard_init() == 0);
    data->mouse_ready = (oops_mouse_init() == 0);

    SDL_zero(mode);
    mode.w = (int)oops_display_get_width(data->display);
    mode.h = (int)oops_display_get_height(data->display);
    /* **`SDL_PIXELFORMAT_XRGB8888`, and the X matters.** The scanout is 32 bits per
     * pixel with the top byte unused rather than an alpha channel - a mode claiming
     * ARGB would have SDL's own software renderer blending against a byte the display
     * ignores. */
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

/* ---- the one window ----------------------------------------------------- */

static bool PROSPERO_CreateSDLWindow(SDL_VideoDevice *_this, SDL_Window *window,
                                     SDL_PropertiesID create_props) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->internal;

    (void)create_props;
    if (!data->display) {
        return SDL_SetError("prospero: no display, so no window");
    }
    if (data->window) {
        /* **Refused rather than silently sharing.** A program that opens a second
         * window and is handed the first would draw both into the same display and see
         * neither. */
        return SDL_SetError("prospero: this display already has a window");
    }

    /* The window is the display, whatever size was asked for. SDL wants to be told the
     * size it actually got, which these two assignments are. */
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
 * **Both answer success without moving anything, and that is the truthful answer here
 * rather than a shortcut.** SDL's contract is that a driver reports the geometry the
 * window *actually* has by sending the event; a fullscreen window on a fixed display
 * has the geometry it always had, so the event carries the real numbers and the caller
 * is not misled. A driver that failed instead would stop programs that set a window
 * size on the way up, which is most of them.
 */
static bool PROSPERO_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window *window) {
    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_MOVED, window->x, window->y);
    return true;
}

static void PROSPERO_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window) {
    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_RESIZED, window->w, window->h);
}

/* ---- events -------------------------------------------------------------- */

static void PROSPERO_PumpEvents(SDL_VideoDevice *_this) {
    /*
     * **The close request is the only event this driver sources.** The pad arrives
     * through the joystick driver and the keyboard and mouse through their own
     * subsystems; there is no window system to poll. `oops_system_close_requested()` is
     * set by the handler installed above and stays set, so the quit is **latched** -
     * sending one on every pump would fill the queue with duplicates and a program that
     * ignores the first would never get anything else.
     */
    static bool quit_sent = false;

    (void)_this;
    if (!quit_sent && oops_system_close_requested()) {
        quit_sent = true;
        SDL_SendQuit();
    }
}

/* ---- OpenGL --------------------------------------------------------------- */

static bool PROSPERO_GL_LoadLibrary(SDL_VideoDevice *_this, const char *path) {
    /* There is no library to load: oops-gl is linked into the payload. A path is
     * refused rather than ignored, because a caller naming one wants *that* library and
     * would not get it. */
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
 * **This returned NULL for everything in the SDL2 driver until 2026-09-22**, on the
 * reasoning that a statically linked payload has every entry point already bound. The
 * reasoning was wrong and the console showed it: a title written against desktop GL
 * does not *name* the post-1.1 entry points as symbols at all - it holds function
 * pointers and fills them from strings, because on a desktop the driver is behind a
 * loader. Neverball took the NULL, stored it, and called it.
 *
 * `oops_gl_get_proc_address` looks the name up in oops-gl's own table. That table
 * carries the core entry points as well as the extensions since 2026-09-26, which is
 * what a port binding *every* name by string needs - see
 * `oops-sdk/src/gl/gl_procs_core.h`.
 *
 * Weak, so that a title linking this driver without oops-gl gets NULL rather than a
 * link error.
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

    /* **What the caller is told it got.** The display is 32-bit XRGB with a 24-bit
     * depth buffer and 8 bits of stencil; reporting anything else would have a program
     * pick a configuration the display cannot show. Multisampling is 0 rather than 1:
     * there is none, and a program that checks before enabling anti-aliasing should be
     * told so. */
    _this->gl_config.red_size = 8;
    _this->gl_config.green_size = 8;
    _this->gl_config.blue_size = 8;
    _this->gl_config.alpha_size =
        0; /* the top byte is unused, not alpha - see the mode above */
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

    /* Releasing the context is the one case where both arguments are NULL, and it
     * succeeds. */
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
    /* The context is the display's and outlives this call; tearing it down here would
     * take the display with it under a program that is merely switching contexts.
     * `VideoQuit` owns it. */
    return true;
}

/*
 * **The flip is on vsync and cannot be turned off**, so anything but 1 is refused
 * rather than accepted and ignored. A program asking for 0 wants to run unlocked and
 * needs to know it cannot; one asking for -1 wants adaptive sync, which is the same
 * answer.
 */
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
    /* Through the renderer rather than a bare `oops_display_flip`: the flip belongs to
     * whichever back end `oops_gfx` is driving, and a GPU one has a command buffer to
     * submit first. */
    if (oops_gfx_present(data->gfx) != 0) {
        return SDL_SetError("prospero: oops_gfx_present() failed");
    }
    return true;
}

/* ---- the device --------------------------------------------------------- */

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
    data->swap_interval = 1; /* vsync, and it cannot be anything else */
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

    /* **No `CreateWindowFramebuffer`.** That is SDL's software surface path, and
     * offering it would let `SDL_GetWindowSurface` succeed with a buffer nothing
     * presents. A program that wants pixels without GL should be told no and reach for
     * the renderer instead. */

    return device;
}

VideoBootStrap PRIVATE_bootstrap = {
    PROSPERO_DRIVER_NAME, "OOPS Prospero video driver", PROSPERO_CreateDevice,
    NULL, /* no ShowMessageBox: `oops/dialog.h` has one, but it is modal and
           * system-owned, and SDL calls this before the video subsystem is up */
    true  /* preferred: it is the only one that can show anything here */
};

#endif /* SDL_VIDEO_DRIVER_PRIVATE */
