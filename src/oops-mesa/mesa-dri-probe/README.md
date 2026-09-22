# mesa-dri-probe

<p align="center">
  <img src="assets/logo.png" alt="mesa-dri-probe" width="200">
</p>

Brings GL up through the Gallium DRI frontend and reports how far it gets.

## About

mesa-dri-probe is the title that first *runs* oops-mesa's platform shim rather than only compiling it.
It creates a GL context through the frontend and renders only what it can verify, adding one thing
at each step.

- **Fixed function** — a clear and a triangle, read back with `glReadPixels` from the drawable's
  own colour buffer.
- **The programmable pipeline** — a GLSL 330 shader drawing a colour-interpolated triangle from a
  vertex buffer: the compiler, program linking and real vertex attributes.
- **The frame hash** — FNV-1a over every pixel of the finished frame, the roadmap's acceptance
  gate that a single sampled pixel can't stand in for.

It is the frontend counterpart to `mesa-probe`, which walks the winsys directly; keeping them
separate titles gives a failure one screen and one clean attribution. A frame that doesn't retire
is a failure, never a fallback to software — the readback is the check.

## Screenshot

<p align="center">
  <img src="../../../common/assets/no-screenshot.svg" alt="No screenshot yet" width="600">
</p>

## Docs

- [oops-mesa bring-up docs](../../../../oops-mesa/docs/)
- [oops-apps catalog & guide](../../../docs/USER_GUIDE.md)
