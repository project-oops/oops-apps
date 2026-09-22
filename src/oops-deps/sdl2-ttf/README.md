# SDL2_ttf

TrueType text rendered into an `SDL_Surface`, pinned at `release-2.24.0`. Neverball wants it -
13 of its 86 sources fail without `SDL_ttf.h`.

Extreme Tux Racer does **not** use this; it calls FreeType directly.

## Where it stands

**Its one source compiles**, first attempt, for `x86_64-unknown-freebsd -ffreestanding`, against
`oops-sdk`'s libc and no host headers. **`patches/` is empty.**

## One file, and why that is the point

SDL_ttf is a thin layer: `SDL_ttf.c` over FreeType. `glfont.c` and `showfont.c` beside it are
upstream's demos and are not built.

That it cost almost nothing is not luck - **the expensive half was paid first.** "A title wants
text" means FreeType, which is pinned next door and took a new `<setjmp.h>` and a `getenv` in the
SDK to build. SDL_ttf arriving as one compile is what a dependency graph looks like when the
thing underneath it is already right, and it is the reason this was done after FreeType rather
than attempted before.

## HarfBuzz is off

That is upstream's default and it stays. HarfBuzz is a text shaper for scripts whose glyphs
depend on their neighbours; Neverball's menus are Latin. A shaper nothing needs is a large
dependency and a large parser of untrusted input.

## Ordering

A title includes `oops-sdl.mk` and `oops-freetype.mk` **first**, this after. It needs both on the
include path and includes neither itself - a title wanting text has already chosen its SDL, and
FreeType stands alone for a title that uses it directly.
