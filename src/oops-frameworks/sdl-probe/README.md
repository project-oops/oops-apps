# sdl-probe

<p align="center">
  <img src="assets/logo.png" alt="sdl-probe" width="200">
</p>

Upstream SDL2 on the hardware - the smallest app that integrates the dependency.

## About

sdl-probe brings up four SDL subsystems at once (video, joystick, game controller, timer),
creates a GL window and context, pumps events, opens the pad and swaps buffers - the whole
minimal SDL2 program, built through [`oops-deps/sdl2`](../../oops-deps/sdl2/).

- **The link is half the job.** Payload links ignore unresolved symbols, so a missing SDL
  function would link silently and fault on the hardware; this app is where the undefined-symbol
  check runs against SDL.
- **It ends by itself** after 600 frames.

## Screenshot

<p align="center">
  <img src="../../../common/assets/no-screenshot.svg" alt="No screenshot yet" width="600">
</p>

## Docs

- [Reference](docs/REFERENCE.md) - what it exercises, the symbol check, the pad mapping, and the per-flip cost.
- [oops-apps catalog and guide](../../../docs/USER_GUIDE.md)
