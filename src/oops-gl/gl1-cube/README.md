# gl1-cube

A rotating cube, torus and sphere drawn through `oops-gl`'s OpenGL 1.x path, and **the pinned
hardware oracle**: the frame it emits was measured on a retail console and is asserted register
by register by `test_pm4_gl_honours_the_gl_cube_oracle_record` in oops-sdk. The record lives at
`oops-sdk/docs/hardware/agc-gl-cube-oracle-fw1240.md`.

The distinction from its neighbour matters. `gl1-probe` checks that GL *features* produce the
right pixels - it is the coverage test, and it is where a new feature earns its place. gl1-cube
checks that the *command stream* is still the one a console was proven to accept. Widening
gl1-cube into a feature test would duplicate the probe; what gl1-cube owes is that its own narrow
job is done honestly and that it runs fast enough to watch.

The title id stays `GLCB00001` although the app was renamed from `gl-cube` on 2026-09-17: the
record names that id, and changing it would leave the record describing a title that no longer
exists. See `app.env`.

## Building

```
make check      # host self-test - the scene, lit, through the real software rasteriser
make elf        # the plain-ELF payload a homebrew loader runs
make title      # complete title package (.zip)
```

`gl1_cube_scene.h` holds the geometry and the procedural texture, and **both the payload and the
self-test include it**. They used to hold a copy each, and the copies drifted by one vertex
colour, so every run of the host gate was rasterising a cube the console never drew.

## Control files

Pushed to the title directory (`/data/homebrew/GLCB00001/`) before launch, where the payload sees
them as `/app0/<name>`. Existence is the whole signal; nothing is read from them.

| File | What the run does |
|---|---|
| `stop.<pid>` | ends the session cleanly - the only way out for a target driven over the network |
| `pause` | no animation, one fixed orientation, and the depth test toggles at frames 60 and 90 so the frames either side compare |
| `dump` | frame 30 logs the oracle record: command stream, shader words, descriptors, fence, GPU clock and the frame's pixel hash |
| `nodepth` | start with the depth test off |
| `tex` | start with the texture on |
| `preamble-a` | open every frame with Mesa's preamble, variant A (with `CLEAR_STATE`) |
| `preamble-b` | the same, variant B (no `CLEAR_STATE`) |
| `gbaddr` | read `GB_ADDR_CONFIG` off the GPU with a `COPY_DATA` packet and log it at frame 3 |
| `gputile` | move display tiling from the CPU to the GPU's compute tiler for this run |

`pause` and `dump` are also what switch the frame measurement from sampled to full: those runs
hash every one of the 2,073,600 words of the frame, and every other run samples one word in 64.
The HUD labels the sampled figures `Hash(1:64)` and `Mod Pix(1:64)` so the two are never confused
for one another.

## Reading the HUD

* **GPU badge** - green when a submission has been confirmed by its end-of-pipe fence, with the
  start-up clear test's match count, the last fence and the GPU clock.
* **VS Can / PS Can** - the shader canaries; green means the vertex shader ran.
* **Hash / Mod Pix / Center** - the render target's own numbers, read back from the command
  processor's copy rather than from the uncached target.
* **Cull / Light / Tex / Depth** - pipeline state, each on its own button.
* **Tile: CPU|GPU** - which tiler is converting the linear render target into the display-tiled
  scanout surface. `CPU` unless this run was given `gputile`.
* **Frame: N (M ms/frame)** - wall-clock for the whole frame, top to swap.

## What a run should measure

Frame time is logged in phases at frame 5 and every 60 frames: `t-draw-finish-us` (drawing and
the fence), `t-hash-us` (the frame measurement), `t-hud-us` (the 2D overlay) and `t-swap-us`
(tiling, the vsync wait and the flip). Two numbers are open questions that one deploy answers:

1. **What the CPU tiler costs.** Every flip reads the whole frame out of write-combined video
   memory and writes it back scattered, and CPU reads of that memory are the slowest thing the
   app touches - the code's own note records that a full-frame read drops the demo to two frames
   a second. Run once with `gputile` and once without: the difference in `t-swap-us` is the
   answer, and `Tile:` on screen says which path each run took.
2. **What the HUD costs.** Its two translucent panels blend 268,200 pixels per frame, and a blend
   reads the destination - so those are 268,200 uncached reads. If `t-hud-us` says that is
   material, the fix is to blend against the readback copy, which is in cached memory, rather
   than against the render target.
