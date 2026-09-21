/*
 * ETR includes <SDL2/SDL.h>, which is where a system install puts it. Our SDL2 is pinned in
 * oops-deps and its headers sit directly on the include path, so this forwards.
 *
 * A forwarding header rather than a patch, because `src/oops-titles/README.md` says so: this can
 * live in the shim, so it must. Five one-line headers survive an upstream bump; five hunks
 * rewriting include lines in five files do not.
 */
#include <SDL.h>
