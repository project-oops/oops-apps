/*
 * The console video driver for upstream SDL2. Ours, compiled beside upstream's tree rather than
 * patched into it - see ../README.md for why the patch stays at two hunks.
 *
 * This is written against `src/video/SDL_sysvideo.h`, which is SDL's own interface for adding a
 * platform. `src/video/dummy/` is upstream's stated template for exactly this and says so in its
 * header comment; what is taken from it is which function pointers a device has to fill, which
 * is a fact about the interface.
 */
/*
 * **The includes here are not spelled the way upstream's own drivers spell theirs.** A driver
 * under `src/video/<name>/` reaches its neighbours with `"../SDL_sysvideo.h"`; these files live
 * outside that tree, so they go through `-Iupstream/src` and name the path from there. That is
 * the whole price of keeping our code as ordinary source instead of burying it in a patch, and
 * it is worth paying - a `.c` file can be read, reviewed and compiled on its own, and a hunk in
 * a `.patch` cannot.
 */
#ifndef SDL_prosperovideo_h_
#define SDL_prosperovideo_h_

#include "video/SDL_sysvideo.h"

#include "oops/display.h"

/*
 * There is one display and one window, because the console has one of each. `window` is held so
 * a second CreateSDLWindow can be refused rather than quietly handing back a second handle onto
 * the same framebuffer.
 */
typedef struct
{
    oops_display_t *display;
    SDL_Window *window;
    int swap_interval;
    int keyboard_ready;
    int mouse_ready;
} PROSPERO_VideoData;

extern VideoBootStrap PROSPERO_bootstrap;

#endif /* SDL_prosperovideo_h_ */
