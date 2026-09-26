# gl1-cube - reference

Operator and design notes for `gl1-cube`. The [README](../README.md) is the overview; this is
how it runs.

## The oracle, and the line to gl1-probe

gl1-cube is the **pinned hardware oracle**: the frame it emits was measured on the hardware and
is asserted register by register by `test_pm4_gl_honours_the_gl_cube_oracle_record` in oops-sdk.
The record lives at `oops-sdk/docs/hardware/agc-gl-cube-oracle-fw1240.md`.

`gl1-probe` checks that GL features produce the right pixels; it is the coverage test, and new
features are checked there. gl1-cube checks that the command stream is the one the hardware
accepts. Its job stays narrow, and it runs fast enough to watch.

The title id is `GLCB00001`, the id the oracle record names. See `app.env`.

## Building

```
make check      # host self-test - the scene, lit, through the real software rasteriser
make elf        # the plain-ELF payload a homebrew loader runs
make title      # complete title package (.zip)
```

`gl1_cube_scene.h` holds the geometry and the procedural texture, and both the payload and the
self-test include it, so the host gate rasterises the same cube the hardware draws.

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

`pause` and `dump` also switch the frame measurement from sampled to full: those runs hash every
word of the frame, and every other run samples one word in 64. The HUD labels the sampled figures
`Hash(1:64)` and `Mod Pix(1:64)`.

## Reading the HUD

* **GPU badge** - green when a submission has been confirmed by its end-of-pipe fence, with the
  start-up clear test's match count, the last fence and the GPU clock.
* **VS Can / PS Can** - the shader canaries; green means the vertex shader ran.
* **Hash / Mod Pix / Center** - the render target's own numbers, read back from the command
  processor's copy rather than from the uncached target.
* **Cull / Light / Tex / Depth** - pipeline state, each on its own button.
* **Tile** - which tiler converts the linear render target into the display-tiled scanout
  surface.
* **Frame: N (M ms/frame)** - wall-clock for the whole frame, top to swap.

Frame time is logged in phases at frame 5 and every 60 frames: `t-draw-finish-us` (drawing and
the fence), `t-hash-us` (the frame measurement), `t-hud-us` (the 2D overlay) and `t-swap-us`
(tiling, the vsync wait and the flip).
