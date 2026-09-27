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
#include "events/SDL_mouse_c.h" /* SDL_GetMouse, for the warp hook below */

#include "SDL_prosperovideo.h"
#include "SDL_prosperoevents_c.h"

#include "oops/dialog.h"
#include "oops/display.h"
#include "oops/draw.h"  /* the software pointer in PROSPERO_DrawCursor */
#include "oops/input.h" /* a button closes the drawn message box */
#include "oops/keyboard.h"
#include "oops/mouse.h"
#include "oops/system.h"
#include "oops/time.h"

#define PROSPEROVID_DRIVER_NAME "prospero"

/* The one context. Its address is the handle; its value is never read. */
static int prospero_the_context;

/* ---------------------------------------------------------------- lifecycle */

/*
 * `SDL_WarpMouseInWindow`, which does nothing at all when a driver leaves this hook null.
 *
 * The pump reports the mouse as *relative* motion and SDL keeps the absolute position itself, so a
 * warp is that position being set: an absolute motion event says where the pointer now is, and
 * `SDL_GetMouseState` agrees from the next call on.
 *
 * A port that steers an on-screen cursor with a thumbstick needs this and looks like a controller
 * fault without it. Bugdom's menu moves its cursor by the stick and then warps the mouse to follow;
 * with the warp doing nothing, the next frame reads the mouse still at the old place, decides the
 * mouse has moved instead, and snaps the cursor back - every frame, however well the stick reads.
 */
static void PROSPERO_WarpMouse(SDL_Window *window, int x, int y) {
    SDL_SendMouseMotion(window, 0, 0 /* absolute */, x, y);
}

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

    /* Installed whether or not a mouse is present: a title that warps the pointer is steering its
     * own cursor, and that has to work on a console where nobody has plugged a mouse in. */
    SDL_GetMouse()->WarpMouse = PROSPERO_WarpMouse;

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

/*
 * The pointer, drawn by us because nothing else will.
 *
 * `SDL_ShowCursor(1)` asks the operating system to show a pointer, and on a desktop that is the end
 * of it. This console has no such pointer and no cursor plane, so a port that steers one - Bugdom's
 * menu picks its icons by cursor position, and never draws one itself - shows the player nothing at
 * all, whatever the pad is doing. Moving and not moving look identical.
 *
 * Drawn straight onto the buffer the next flip shows, after the frame's own rendering and before the
 * present, so it lands on top without touching the GL state the title left behind.
 *
 * Weakly referenced: the drawing calls belong to the SDK's `draw` capability, and linking SDL should
 * not put that on the capability list of every title. Without it there is no pointer, exactly as
 * before.
 */
#pragma weak oops_display_get_surface
#pragma weak oops_draw_rect
#pragma weak oops_draw_line

/* The arrow, as a 1-bit mask: 1 is the white body, 2 the black outline, 0 transparent. Twelve by
 * nineteen, the proportions of the pointer everything else uses. */
static const unsigned char prospero_cursor_mask[19][12] = {
    {2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {2, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {2, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0},
    {2, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0}, {2, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0},
    {2, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0}, {2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0},
    {2, 1, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0}, {2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0, 0},
    {2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0}, {2, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2},
    {2, 1, 1, 1, 2, 1, 1, 2, 0, 0, 0, 0}, {2, 1, 1, 2, 2, 1, 1, 2, 0, 0, 0, 0},
    {2, 1, 2, 0, 2, 1, 1, 2, 0, 0, 0, 0}, {2, 2, 0, 0, 0, 2, 1, 1, 2, 0, 0, 0},
    {2, 0, 0, 0, 0, 2, 1, 1, 2, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 2, 1, 2, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0},
};

static void PROSPERO_DrawCursor(PROSPERO_VideoData *data) {
    if (!&oops_display_get_surface || !&oops_draw_rect) {
        return; /* The title did not name the `draw` capability. */
    }
    if (SDL_ShowCursor(-1) != 1) {
        return; /* SDL_QUERY: the title has the pointer hidden. */
    }

    {
        SDL_Mouse *mouse = SDL_GetMouse();
        oops_surface_t surf = oops_display_get_surface(data->display);
        const int mx = mouse->x;
        const int my = mouse->y;
        int row, col;

        if (!surf.pixels) {
            return;
        }
        /* A pixel at a time rather than a blit: the shape is 228 pixels, and a sprite blit onto a
         * scanout buffer in the GPU's swizzle would read the destination back to blend, out of
         * memory that is write-combined and slow to read. Opaque writes do not read at all. */
        for (row = 0; row < 19; row++) {
            for (col = 0; col < 12; col++) {
                const unsigned char m = prospero_cursor_mask[row][col];
                if (m == 0) {
                    continue;
                }
                oops_draw_rect(&surf, mx + col, my + row, 1, 1,
                               m == 1 ? OOPS_RGBA(255, 255, 255, 255)
                                      : OOPS_RGBA(0, 0, 0, 255));
            }
        }
    }
}

static int PROSPERO_GL_SwapWindow(_THIS, SDL_Window *window) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;

    if (window != data->window) {
        return SDL_SetError("prospero: that window is not this display's");
    }

    PROSPERO_DrawCursor(data);

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
 * The system message dialog, when the title has it, and weakly referenced so that it
 * does not put `dialog` on the capability list of every title that links SDL. Absent,
 * the text still reaches the log and the default button answers.
 */
#pragma weak oops_dialog_message_show
#pragma weak oops_dialog_message_poll
#pragma weak oops_dialog_message_close

/* The pieces the drawn fallback below needs, weak for the same reason. */
#pragma weak oops_display_any_open
#pragma weak oops_display_open
#pragma weak oops_display_close
#pragma weak oops_display_flip
#pragma weak oops_draw_clear
#pragma weak oops_draw_text
#pragma weak oops_draw_text_width
#pragma weak oops_input_init
#pragma weak oops_input_poll

/*
 * Break `text` into lines of at most `columns` characters at word boundaries, writing
 * the result into `out` with newlines between them. `oops_draw_text` already honours
 * newlines; what it will not do is decide where they go, and a message box's text is one
 * long line that would otherwise run off the side of the screen.
 */
static void PROSPERO_WrapText(const char *text, int columns, char *out,
                              size_t out_size) {
    size_t w = 0;
    int column = 0;

    if (!out || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (!text || columns < 8) {
        return;
    }

    while (*text && w + 2 < out_size) {
        size_t word;

        if (*text == '\n') {
            out[w++] = '\n';
            column = 0;
            text++;
            continue;
        }
        if (*text == ' ' || *text == '\t') {
            /* A run of spaces at a line break is the break itself; keep one otherwise. */
            if (column > 0 && column < columns) {
                out[w++] = ' ';
                column++;
            }
            text++;
            continue;
        }

        for (word = 0; text[word] && text[word] != ' ' && text[word] != '\t' &&
                       text[word] != '\n';
             word++) {
        }

        /* A word that will not fit on what is left of this line starts a new one; a word
         * longer than the whole line is broken rather than dropped. */
        if (column > 0 && column + (int)word > columns) {
            out[w++] = '\n';
            column = 0;
        }
        while (word > 0 && *text && w + 2 < out_size) {
            if (column >= columns) {
                out[w++] = '\n';
                column = 0;
            }
            out[w++] = *text++;
            column++;
            word--;
        }
    }
    out[w] = '\0';
}

/*
 * The message on the screen, without the system dialog.
 *
 * A port asks for a message box at the point where something is already wrong, and the
 * two cases that matter most - a missing asset archive, a ROM the player has to supply -
 * are found *before* the first frame. There is no video-out session then, so the system
 * dialog cannot be used at all: it composites against one, and asked without it the
 * platform's own library faults. That left the player with a black screen and a crash
 * where the whole point was to tell them something.
 *
 * So this opens a display of its own, writes the text, and waits for a button. It runs
 * only when nothing else has a display open - if a port has one, the system dialog works
 * and is the better answer - and it closes the display again, because the port may go on
 * to open its own.
 *
 * Returns 0 if the message reached the screen, negative if it could not.
 */
static int PROSPERO_DrawMessageBox(const SDL_MessageBoxData *data) {
    static const unsigned int width = 1920, height = 1080;
    static const int scale = 2, glyph = 8;
    const int line_height = glyph * scale + 6;
    char wrapped[2048];
    oops_display_t *disp;
    oops_surface_t surf;
    int y, polls;

    if (!&oops_display_open || !&oops_display_close || !&oops_display_get_surface ||
        !&oops_display_flip || !&oops_draw_clear || !&oops_draw_text ||
        !&oops_draw_text_width) {
        return -1;
    }
    /* Only when there is no video-out at all: see the note above. */
    if (&oops_display_any_open && oops_display_any_open()) {
        return -1;
    }

    disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, width, height);
    if (!disp) {
        oops_log_warn("SDL", "  no display could be opened to show the message on");
        return -1;
    }

    surf = oops_display_get_surface(disp);
    if (!surf.pixels) {
        oops_log_warn("SDL", "  the display gave no surface to draw the message on");
        oops_display_close(disp);
        return -1;
    }

    oops_draw_clear(&surf, OOPS_RGBA(16, 18, 24, 255));

    y = 180;
    if (data->title) {
        const int w = oops_draw_text_width(data->title, scale + 1);
        oops_draw_text(&surf, ((int)width - w) / 2, y, data->title,
                       OOPS_RGBA(255, 214, 92, 255), scale + 1);
        y += (glyph * (scale + 1)) + 40;
    }

    PROSPERO_WrapText(data->message, ((int)width - 240) / (glyph * scale), wrapped,
                      sizeof(wrapped));
    for (const char *line = wrapped; *line;) {
        char one[256];
        size_t n = 0;
        while (line[n] && line[n] != '\n' && n + 1 < sizeof(one)) {
            one[n] = line[n];
            n++;
        }
        one[n] = '\0';
        oops_draw_text(&surf, 120, y, one, OOPS_RGBA(232, 232, 238, 255), scale);
        y += line_height;
        line += n;
        if (*line == '\n') {
            line++;
        }
    }

    {
        static const char *const hint = "Press any button to close";
        const int w = oops_draw_text_width(hint, scale);
        oops_draw_text(&surf, ((int)width - w) / 2, (int)height - 160, hint,
                       OOPS_RGBA(140, 146, 160, 255), scale);
    }

    oops_display_flip(disp);
    oops_log_warn("SDL", "  the message is on screen; waiting for a button");

    /*
     * Bounded for the same reason the dialog wait is: a title that waits for ever on a
     * screen nobody is watching is killed by the system with nothing explained. A minute
     * is long enough to read a sentence and short enough not to look like a hang.
     */
    if (&oops_input_init && &oops_input_poll) {
        (void)oops_input_init();
        for (polls = 0; polls < 60 * 1000 / 16; polls++) {
            oops_pad_state_t pad;
            if (oops_input_poll(0, &pad) == 0 && pad.buttons != 0 && polls > 30) {
                break;
            }
            oops_time_sleep_ms(16);
        }
    } else {
        oops_time_sleep_ms(10000);
    }

    oops_display_close(disp);
    return 0;
}

/* The button the port itself marked as the return-key default, or its first, or -1 for
 * no buttons. */
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
 * A port reaches for a message box at the point where it has something the player must
 * read - a missing asset archive, an unsupported ROM - and then acts on the answer.
 * Leaving the hook null makes SDL return an error without touching `buttonid`, so the
 * port reads its own uninitialised stack and exits with nothing said. Both halves of
 * that are worth fixing.
 *
 * A two-button box maps to the system Yes/No dialog and the player's answer is
 * returned. Anything else is shown as OK and answered with the port's own default, so a
 * box with three buttons is reported as such in the log rather than silently reduced.
 */
static int PROSPERO_ShowMessageBox(const SDL_MessageBoxData *data, int *buttonid) {
    if (!data || !buttonid)
        return SDL_InvalidParamError("messageboxdata");

    oops_log_warn("SDL", "message box: %s", data->title ? data->title : "(no title)");
    if (data->message)
        oops_log_warn("SDL", "  %s", data->message);
    if (data->numbuttons > 2) {
        oops_log_warn(
            "SDL",
            "  %d buttons; the console dialog offers two, so this one is OK-only",
            data->numbuttons);
    }

    *buttonid = PROSPERO_DefaultButton(data);

    if (&oops_dialog_message_show && &oops_dialog_message_poll &&
        &oops_dialog_message_close) {
        const int yesno = (data->numbuttons == 2);
        char text[1024];
        SDL_snprintf(text, sizeof(text), "%s\n\n%s", data->title ? data->title : "",
                     data->message ? data->message : "");
        /* Logged either side of the call, because a port reaches a message box at the
         * point where something has already gone wrong: if the dialog is what kills it,
         * the log has to say which side of this line it stopped on. */
        oops_log_warn("SDL", "  opening the system dialog");
        if (oops_dialog_message_show(text, yesno ? OOPS_MSG_DIALOG_BTN_YESNO
                                                 : OOPS_MSG_DIALOG_BTN_OK) == 0) {
            oops_msg_dialog_result_t result = OOPS_MSG_DIALOG_RES_INVALID;
            int rc;
            /*
             * Bounded, and that bound is the point. The dialog only reaches FINISHED
             * when the player answers it, and the player can only answer one that
             * reached the screen - which wants a video-out session the port may not
             * have opened yet, since a missing asset is found before the first frame.
             * An unbounded wait there is a title that never draws and never exits, and
             * the system kills it without either of us learning why. Thirty seconds at
             * 16ms, then the default answer the caller would have had anyway.
             */
            const int max_polls = 30 * 1000 / 16;
            int polls = 0;
            while ((rc = oops_dialog_message_poll(&result)) == 0 && polls < max_polls) {
                oops_time_sleep_ms(16);
                polls++;
            }
            oops_dialog_message_close();
            if (rc == 0) {
                oops_log_warn("SDL",
                              "  the dialog did not finish in 30s - it may never have "
                              "reached the screen; answering with the default");
            } else if (rc == 1 && yesno && result != OOPS_MSG_DIALOG_RES_INVALID) {
                /* Yes is the first button, No the second: the order SDL_MessageBoxData
                 * lists them. */
                *buttonid =
                    data->buttons[result == OOPS_MSG_DIALOG_RES_OK ? 0 : 1].buttonid;
            }
            oops_log_warn("SDL", "  answered with button id %d", *buttonid);
            return 0;
        }
        oops_log_warn("SDL",
                      "  the system dialog would not open; answering with the default");
    }

    /* No system dialog, for want of a video-out session or of the capability. The
     * message still has to reach the player, so draw it. */
    (void)PROSPERO_DrawMessageBox(data);

    if (data->numbuttons > 0) {
        oops_log_warn("SDL", "  answered with button id %d, the port's own default",
                      *buttonid);
    }
    return 0;
}

VideoBootStrap PROSPERO_bootstrap = {PROSPEROVID_DRIVER_NAME,
                                     "OOPS console video driver", PROSPERO_CreateDevice,
                                     PROSPERO_ShowMessageBox};

#endif /* SDL_VIDEO_DRIVER_PROSPERO */
