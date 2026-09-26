/*
 * ETR writes <SDL2/SDL_image.h>, which is where a system install puts it. SDL2_image is
 * pinned in `oops-deps/sdl2-image` and its header sits directly on the include path, so
 * this forwards.
 */
#include <SDL_image.h>
