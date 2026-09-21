/*
 * ETR writes <SDL2/SDL_mixer.h>, which is where a system install puts it. SDL2_mixer is pinned
 * in `oops-deps/sdl2-mixer` and its header sits directly on the include path, so this forwards.
 */
#include <SDL_mixer.h>
