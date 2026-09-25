# Super Mario 64

The PC port of the `n64decomp/sm64` decompilation —
[upstream](https://github.com/sm64-port/sm64-port), pinned by commit.

## Why this fork and not the decompilation

`n64decomp/sm64` builds an N64 ROM. It has no `src/pc`, no graphics backend and no platform
layer — which is true of every matching decompilation, including `zeldaret/oot`, `zeldaret/mm`
and `n64decomp/mk64`. Porting *from* one means writing the platform layer yourself.
`sm64-port` is the fork that already did.

## It is the cleanest GL fit surveyed

Measured on 2026-09-25 against what `oops-gl` defines, counting *definitions* in
`oops-sdk/src/gl/*.c` rather than header declarations:

| | GL calls needed | missing from oops-gl |
|---|---|---|
| **`src/pc/gfx/gfx_opengl.c`** | **34** | **0** |
| the whole of `src/pc` | 49 | 13, all `glX*` |

The thirteen are the X11 windowing path — `glXMakeCurrent`, `glXSwapBuffers` and friends —
which SDL2 replaces and which the collection already vendors. The renderer proper asks for
nothing this GL lacks: shaders, buffers, vertex attributes, textures, and `glDrawArrays`. No
vertex array objects, no framebuffer objects, no core profile.

Its shaders are `#version 110`, generated at run time by `gfx_opengl.c` from the F3D combiner
state. That is the dialect `oops-gl`'s GLSL front end was written for — and unlike
`../ship-of-harkinian`, there is no version number to patch.

**C and a Makefile**, which is the other reason this is the easy one. No CMake, no C++20, no
twelve third-party libraries.

## The ROM, which is the actual problem

`sm64-port` extracts its assets **at build time**. `extract_assets.py` reads
`baserom.<version>.z64` and writes them into the tree:

| kind | count |
|---|---|
| `.png` textures | 1707 |
| `.aiff` samples | 223 |
| `.m64` sequences | 135 |
| `.bin` | 8 |

Those are then compiled in, so **the build fails without a ROM on the build machine** and the
resulting payload has the assets inside it. That is a different shape from
`../ship-of-harkinian`, which builds clean with no ROM anywhere and converts the player's own
copy on the console at first run.

Two ways forward, and the choice has not been made:

1. **Build-time, as upstream does.** Simplest, and it means this title cannot be built by CI or
   shipped as a download — only built locally by someone who owns the game.
2. **Patch it to read the ROM at run time**, the way Ship of Harkinian does. The work is real:
   the extraction currently produces a PNG and AIFF tree that the *build* consumes, so moving it
   to the console means an on-device extractor. That is exactly what `ZAPDLib` is for Ocarina of
   Time, which is an argument for doing Ship of Harkinian first and learning the shape there.

## State

Pin recorded, nothing built. The GL side is the part that is already answered; the asset
pipeline is the part that is not.
