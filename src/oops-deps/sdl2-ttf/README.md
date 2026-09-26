# SDL2_ttf

TrueType text rendered into an `SDL_Surface`, pinned at `release-2.24.0`. Neverball uses it.
Extreme Tux Racer does not; it calls FreeType directly.

It compiles for `x86_64-unknown-freebsd -ffreestanding` against `oops-sdk`'s libc and no host
headers. `patches/` is empty.

## Source list

SDL_ttf is a thin layer: `SDL_ttf.c` over FreeType, which is pinned next door. `glfont.c` and
`showfont.c` beside it are upstream's demos and are not built.

## HarfBuzz

HarfBuzz is off, which is upstream's default. It is a text shaper for scripts whose glyphs depend
on their neighbours; Neverball's menus are Latin. A shaper nothing needs is a large dependency
and a large parser of untrusted input.

## Ordering

A title includes `oops-sdl.mk` and `oops-freetype.mk` first, this after. It needs both on the
include path and includes neither itself - a title wanting text has already chosen its SDL, and
FreeType stands alone for a title that uses it directly.
