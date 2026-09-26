/*
 * The console video driver for upstream SDL2, compiled beside upstream's tree rather
 * than patched into it. Written against `src/video/SDL_sysvideo.h`, with
 * `src/video/dummy/` as the template for which device function pointers to fill.
 *
 * Includes go through `-Iupstream/src` (`"video/SDL_sysvideo.h"`) because these files
 * live outside `src/video/`.
 */
#ifndef SDL_prosperovideo_h_
#define SDL_prosperovideo_h_

#include "video/SDL_sysvideo.h"

#include "oops/gfx.h"
#include "oops/display.h"

/*
 * One display and one window. `window` is held so a second CreateSDLWindow is refused
 * rather than handed a second handle onto the same framebuffer.
 */
typedef struct {
    oops_gfx_t *gfx; /* owns the display and the GL context (oops/gfx.h) */
    oops_display_t
        *display; /* == oops_gfx_display(gfx), cached for the calls that need it */
    SDL_Window *window;
    int swap_interval;
    int keyboard_ready;
    int mouse_ready;
} PROSPERO_VideoData;

extern VideoBootStrap PROSPERO_bootstrap;

#endif /* SDL_prosperovideo_h_ */
