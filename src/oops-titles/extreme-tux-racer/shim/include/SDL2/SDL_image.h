/*
 * ETR writes <SDL2/SDL_image.h>, which is where a system install puts it. SDL2_image is
 * pinned in `oops-deps/sdl2-image` and its header sits directly on the include path, so
 * this forwards - the same shape as `SDL_mixer.h` beside it.
 *
 * It used to forward to <SDL.h> instead, with a note that the library was not pinned
 * yet and that the *link* would therefore be what named the gap. That was the wrong
 * prediction: SDL.h declares no `IMG_Load`, so the compile stopped first and the
 * undefined-symbol check never got the chance. Reaching the link would have been the
 * better failure, which is what this now is - except that there is nothing left to fail
 * on.
 */
#include <SDL_image.h>
