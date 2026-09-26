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

The ROM is a run-time input for every title here: a build carries no game assets, and a title that
finds no ROM says so on screen.

`sm64-port` decides the other way. `extract_assets.py` reads `baserom.<version>.z64` and writes
`.png` textures, `.aiff` samples, `.m64` sequences and `.bin` files into the tree, which the build
compiles into `sound_data.o`, `leveldata.o` and the skybox objects. A payload built from it embeds
the game's assets, so it cannot be published, and this pin therefore has no payload target.

[`../ship-of-harkinian`](../ship-of-harkinian/README.md) is the shape a port of this game needs:
the build ships only the port's own archive, and the title converts the player's copy on the
hardware at first run. Reaching that here means an upstream that reads the ROM at run time.

What remains useful is the platform layer, which `make census` measures against oops-sdk, and the
entry point in `shim/`, which is upstream-independent.
