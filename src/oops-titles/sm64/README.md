# Super Mario 64

The PC port of the `n64decomp/sm64` decompilation -
[upstream](https://github.com/sm64-port/sm64-port), pinned by commit.

## The fork

`n64decomp/sm64` builds an N64 ROM. It has no `src/pc`, no graphics backend and no platform
layer - true of every matching decompilation, including `zeldaret/oot`, `zeldaret/mm` and
`n64decomp/mk64`. `sm64-port` is the fork that adds the platform layer.

## The GL surface

Counted against the definitions in `oops-sdk/src/gl/*.c`, not header declarations:

- `src/pc/gfx/gfx_opengl.c` asks for nothing oops-gl lacks: shaders, buffers, vertex attributes,
  textures, and `glDrawArrays`. No vertex array objects, no framebuffer objects, no core profile.
- The rest of `src/pc` adds the X11 windowing path - `glXMakeCurrent`, `glXSwapBuffers` and
  relatives - which SDL2 replaces.

Its shaders are `#version 110`, generated at run time by `gfx_opengl.c` from the F3D combiner
state. That is the dialect oops-gl's GLSL front end is written for.

The build is C and a Makefile: no CMake, no C++20, no third-party library stack.

## The ROM

`sm64-port` extracts its assets at build time. `extract_assets.py` reads
`baserom.<version>.z64` and writes `.png` textures, `.aiff` samples, `.m64` sequences and `.bin`
files into the tree, which are then compiled in. The build needs a ROM on the build machine and
the payload carries the assets, so this title is built locally by someone who owns the game and
not by CI or as a download.

[`../ship-of-harkinian`](../ship-of-harkinian/README.md) builds with no ROM and converts the
player's copy on the hardware at first run; reading the ROM at run time here would need an
on-device extractor of the same kind.
