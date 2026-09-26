/*
 * ETR includes <SDL2/SDL.h>, which is where a system install puts it. Our SDL2 is
 * pinned in oops-deps and its headers sit directly on the include path, so this
 * forwards.
 *
 * A forwarding header rather than a patch (`src/oops-titles/README.md`): it survives an
 * upstream bump.
 */
#include <SDL.h>
