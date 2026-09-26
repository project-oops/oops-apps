/*
 * `<SDL2/SDL.h>`, which `supertux/screen_manager.hpp` spells with the directory every
 * other file in the game leaves off. oops-sdl2's include path is SDL's own `include/`,
 * so the prefixed form needs forwarding - the same one-line header Extreme Tux Racer's
 * shim carries.
 */
#include <SDL.h>
