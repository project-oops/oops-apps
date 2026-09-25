/*
 * The console video driver for upstream SDL2: the display, the one window, and the GL context.
 *
 * # What a context is here
 *
 * oops-gl has one implicit context, created with the display and never switched. SDL's model is
 * a handle you create, make current and delete. The two meet by handing back a sentinel: there
 * is exactly one context, `MakeCurrent` on it succeeds, and `MakeCurrent` on anything else
 * fails rather than pretending. A caller that believes a second context exists would draw into
 * the first one and see no error, which is the failure D009 exists to prevent.
 *
 * # Where the attributes go
 *
 * `SDL_GL_SetAttribute` writes into `_this->gl_config` before the context is created, and a real
 * driver honours what it can and reports back what it got. This display is 32-bit RGBA with a
 * depth buffer and no stereo or multisample, and `GL_CreateContext` writes that back so
 * `SDL_GL_GetAttribute` answers with the truth rather than the request.
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

#include "oops/display.h"
#include "oops/keyboard.h"
#include "oops/mouse.h"
#include "oops/system.h"
#include "oops/time.h"

#define PROSPEROVID_DRIVER_NAME "prospero"

/* The one context. Its address is the handle; its value is never read. */
static int prospero_the_context;

/* ---------------------------------------------------------------- lifecycle */

static int PROSPERO_VideoInit(_THIS)
{
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;
    SDL_DisplayMode mode;

    /*
     * The renderer, brought up through oops/gfx.h rather than by opening the display alone. This
     * is the fix for the one thing this backend used to get wrong: it opened a display but never
     * created a GL context, so a title's GL calls had none. `oops_gfx_create` opens the display
     * *and* creates the context and makes it current - and it dispatches to whichever renderer the
     * title linked, so building SDL against Mesa gets Mesa's present path (its flip owns the
     * scanout), not this backend flipping a buffer Mesa never drew into.
     */
    data->gfx = oops_gfx_create(&(oops_gfx_desc_t){ .width = OOPS_DISPLAY_DEFAULT_WIDTH,
                                                    .height = OOPS_DISPLAY_DEFAULT_HEIGHT,
                                                    .depth = true, .vsync = true });
    if (!data->gfx) {
        return SDL_SetError("prospero: the renderer did not come up");
    }
    data->display = oops_gfx_display(data->gfx);

    oops_time_init();

    /* The dashboard's Close becomes an SDL_QUIT in the event pump; install the handler that flags
     * it (oops/system.h). Every SDL program on this backend then closes cleanly. */
    oops_system_install_close_handler();

    /*
     * The keyboard and mouse are optional hardware and their absence is not an error - a pad is
     * the usual input. Remembered so the pump does not poll a device that reported unavailable,
     * and so a title can still start when neither is plugged in.
     */
    data->keyboard_ready = (oops_keyboard_init() == 0);
    data->mouse_ready = (oops_mouse_init() == 0);

    /* These two flags gate the pump in `PROSPERO_PumpEvents`, so they decide whether a title sees
       any key or mouse event at all - and until they were printed, a title that received nothing
       looked identical to a title whose events were being dropped somewhere above. */
    oops_log_info("INPUT", "SDL video init: keyboard_ready=%d mouse_ready=%d", data->keyboard_ready,
                  data->mouse_ready);

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

static void PROSPERO_VideoQuit(_THIS)
{
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

static int PROSPERO_CreateSDLWindow(_THIS, SDL_Window *window)
{
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;

    if (data->window) {
        return SDL_SetError("prospero: there is one display, so one window");
    }

    /*
     * The window is the display. A request for any other size is answered with the size it
     * actually got - SDL carries `w`/`h` back to the caller, so a title that asks for 800x600
     * and reads the window size back learns the truth instead of scaling to a number nobody
     * honoured.
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

static void PROSPERO_DestroyWindow(_THIS, SDL_Window *window)
{
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;

    if (data->window == window) {
        data->window = NULL;
    }
    window->driverdata = NULL;
}

/*
 * Deliberately nothing, and this is the honest implementation rather than a stub. There is no
 * title bar to set, and a caller that believes the call worked draws exactly the same picture -
 * which is the test D009 gives for which of the two this is.
 */
static void PROSPERO_SetWindowTitle(_THIS, SDL_Window *window)
{
    (void)_this;
    (void)window;
}

static void PROSPERO_ShowWindow(_THIS, SDL_Window *window)
{
    (void)_this;
    (void)window;
}

/* ----------------------------------------------------------------------- GL */

static int PROSPERO_GL_LoadLibrary(_THIS, const char *path)
{
    /*
     * There is no library and no loader: oops-gl is linked into the payload. A path is refused
     * rather than ignored, because a caller naming one wants a *different* GL and will not get
     * it.
     */
    if (path) {
        return SDL_SetError("prospero: GL is linked in, so no library can be loaded");
    }
    _this->gl_config.driver_loaded = 1;
    _this->gl_config.driver_path[0] = '\0';
    _this->gl_config.dll_handle = NULL;
    return 0;
}

static void PROSPERO_GL_UnloadLibrary(_THIS)
{
    _this->gl_config.driver_loaded = 0;
}

/*
 * **The GL that answers this is oops-gl, and it is reached weakly.**
 *
 * This backend creates no GL of its own - the context is the display's - so nothing else here
 * needs a GL to be linked, and a title that uses SDL for events and audio should not be made to
 * carry one. A weak *definition* rather than a weak reference is what keeps that true both ways:
 * oops-gl's own definition overrides it when there is one, and when there is not, the payload
 * still links with no undefined symbol for `common/app.mk`'s check to trip over.
 *
 * It is declared here rather than by including <GL/gl.h>, which would put a whole GL API beside
 * SDL's own and conflict with it.
 */
__attribute__((weak)) void *oops_gl_get_proc_address(const char *name)
{
    (void)name;
    return NULL;
}

static void *PROSPERO_GL_GetProcAddress(_THIS, const char *proc)
{
    (void)_this;
    /*
     * **This returned NULL for everything until 2026-09-22**, reasoning that a statically linked
     * payload has every entry point already bound, so a name reaching here must be one the
     * linker could not resolve - that is, absent.
     *
     * The reasoning was wrong and the console showed it. A title written against desktop GL does
     * not name the post-1.1 entry points as symbols at all: it holds function pointers and fills
     * them from strings, because on a desktop the driver is behind a loader. The linker never saw
     * `glGenBuffersARB` because nothing referenced it. Neverball took the NULL, stored it, and
     * called it - `sol_load_full` jumped to address zero on the first mesh it loaded.
     *
     * So the name has to be looked up, and oops-gl looks it up in the payload's own dynamic
     * symbol table. A name it does not have still answers NULL, which is what a caller probing
     * for an extension is asking.
     */
    return oops_gl_get_proc_address(proc);
}

static SDL_GLContext PROSPERO_GL_CreateContext(_THIS, SDL_Window *window)
{
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
     * Report what the display is, not what was asked for. SDL leaves the requested values in
     * gl_config when a driver does not write them back, and then SDL_GL_GetAttribute returns the
     * request - a title checking whether it got a stencil buffer would be told yes.
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

    return (SDL_GLContext)&prospero_the_context;
}

static int PROSPERO_GL_MakeCurrent(_THIS, SDL_Window *window, SDL_GLContext context)
{
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;

    /* Releasing the context is the one case where both arguments are NULL, and it succeeds. */
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

static void PROSPERO_GL_DeleteContext(_THIS, SDL_GLContext context)
{
    (void)_this;
    (void)context;
    /* The context is the display's and outlives this call; closing it here would take the
       display down under a title that is merely switching contexts. VideoQuit owns it. */
}

static void PROSPERO_GL_GetDrawableSize(_THIS, SDL_Window *window, int *w, int *h)
{
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;

    (void)window;
    if (w) {
        *w = (int)oops_display_get_width(data->display);
    }
    if (h) {
        *h = (int)oops_display_get_height(data->display);
    }
}

static int PROSPERO_GL_SetSwapInterval(_THIS, int interval)
{
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;

    /*
     * The flip is on vsync and there is no way to ask for anything else, so 1 is accepted and
     * everything else is refused. Accepting 0 and flipping on vsync anyway would make
     * SDL_GL_GetSwapInterval report a tear-free zero.
     */
    if (interval != 1) {
        return SDL_SetError("prospero: the flip is on vsync, so the swap interval is 1");
    }
    data->swap_interval = 1;
    return 0;
}

static int PROSPERO_GL_GetSwapInterval(_THIS)
{
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;
    return data->swap_interval;
}

static int PROSPERO_GL_SwapWindow(_THIS, SDL_Window *window)
{
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;

    if (window != data->window) {
        return SDL_SetError("prospero: that window is not this display's");
    }
    /* Present through the renderer, not a bare display flip: the flip belongs to whichever backend
     * drew the frame (D012). oops_gfx_present returns true on success; false means the frame is not
     * on screen. */
    if (!oops_gfx_present(data->gfx)) {
        return SDL_SetError("prospero: the present failed (%d)",
                            oops_display_get_last_error(data->display));
    }
    return 0;
}

/* ---------------------------------------------------------------- bootstrap */

static void PROSPERO_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device->driverdata);
    SDL_free(device);
}

static SDL_VideoDevice *PROSPERO_CreateDevice(void)
{
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

VideoBootStrap PROSPERO_bootstrap = {
    PROSPEROVID_DRIVER_NAME, "OOPS console video driver",
    PROSPERO_CreateDevice,
    NULL /* no ShowMessageBox yet - oops/dialog.h is the place it will come from */
};

#endif /* SDL_VIDEO_DRIVER_PROSPERO */
