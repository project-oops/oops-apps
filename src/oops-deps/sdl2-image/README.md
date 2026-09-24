# SDL2_image

Images decoded into an `SDL_Surface`, pinned at `release-2.8.12`. Extreme Tux Racer wants it, and
wants one function: `IMG_Load`, at one call site in `textures.cpp`, reading all 149 of its PNGs.

Its `.tga` and `.bmp` paths are its own screenshot *writers* and never reach this.

## Where it stands

**Its nineteen sources compile**, first attempt, for `x86_64-unknown-freebsd -ffreestanding`,
against `oops-sdk`'s libc and no host headers. **`patches/` is empty.** `IMG_Load` links, and
ETR's last compile error went with it.

## One function, and why it is pinned rather than written

One function is exactly the size at which writing it yourself looks reasonable, and
[`../README.md`](../README.md) answers that before it is asked: `oops_png_decode` answers "decode
this file", which is what a payload with a texture needs; a program written **against SDL_image's
API** needs SDL_image. An `IMG_Load` of our own would have been a second answer to a question
upstream already answers - and then ours to carry, bump and debug forever.

It cost almost nothing for the reason SDL_ttf did: **the expensive half was paid first.** "A title
reads its own artwork" means libpng and zlib, both pinned next door for Neverball and both already
compiling. Wiring this into ETR is three `include` lines and no new code anywhere.

## PNG only, and nineteen files anyway

`LOAD_PNG` is the one format switched on - that is "trim for surface, not size", since AVIF, JXL,
WEBP, TIFF, SVG and XCF are all parsers of untrusted input that no title here has a file for.

**Every `IMG_<fmt>.c` is still compiled**, which is not a contradiction: `IMG.c`'s `supported[]`
table names each format's `is`/`load` pair unconditionally, so the files have to be present to
satisfy the link. Without its own `LOAD_<FMT>`, each compiles to a stub that returns NULL.
Dropping the files would not drop the references.

`IMG_png.c` chooses its back end at the top - `USE_STBIMAGE` takes the bundled header-only
decoder, and the `#else` selects libpng. Nothing is defined, so it is libpng: the decoder this
collection already ships. `SDL_IMAGE_SAVE_PNG` is 0, because nothing calls `IMG_SavePNG`.

The two platform back ends are not built: `IMG_WIC.c` is Windows Imaging Component and
`IMG_ImageIO.m` is Objective-C.
