# SDL2_image

Images decoded into an `SDL_Surface`, pinned at `release-2.8.12`. Extreme Tux Racer uses one
function, `IMG_Load`, at one call site in `textures.cpp`, to read its PNGs. Its `.tga` and `.bmp`
paths are its own screenshot writers and never reach this.

It compiles for `x86_64-unknown-freebsd -ffreestanding` against `oops-sdk`'s libc and no host
headers. `patches/` is empty.

## Pinned rather than written

[`../README.md`](../README.md) draws the line: `oops_png_decode` answers "decode this file",
which is what a payload with a texture needs; a program written against SDL_image's API needs
SDL_image. It sits on libpng and zlib, both pinned next door for Neverball.

## Formats

`LOAD_PNG` is the one format switched on - trim for surface, not size, since AVIF, JXL, WEBP,
TIFF, SVG and XCF are all parsers of untrusted input that no title here has a file for.

Every `IMG_<fmt>.c` is still compiled: `IMG.c`'s `supported[]` table names each format's
`is`/`load` pair unconditionally, so the files have to be present to satisfy the link. Without
its own `LOAD_<FMT>`, each compiles to a stub that returns NULL.

`IMG_png.c` chooses its back end at the top - `USE_STBIMAGE` takes the bundled header-only
decoder, and the `#else` selects libpng. Nothing is defined, so it is libpng, the decoder this
collection already ships. `SDL_IMAGE_SAVE_PNG` is 0, because nothing calls `IMG_SavePNG`.

The two platform back ends are not built: `IMG_WIC.c` is Windows Imaging Component and
`IMG_ImageIO.m` is Objective-C.
