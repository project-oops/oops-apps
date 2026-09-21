/*
 * SDL2_image is **not pinned yet** - see ../../README.md.
 *
 * This forwards to SDL2's own header so the include resolves and the compile gets past it,
 * which means the *link* is what names the gap. That is the failure that gets read: a payload
 * link ignores unresolved symbols, so `common/app.mk`'s undefined-symbol check is the thing
 * that will list `IMG_Load` and friends, loudly, in one place.
 */
#include <SDL.h>
