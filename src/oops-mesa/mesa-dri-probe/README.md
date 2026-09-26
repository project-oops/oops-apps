# mesa-dri-probe

<p align="center">
  <img src="assets/logo.png" alt="mesa-dri-probe" width="200">
</p>

Brings GL up through the Gallium DRI frontend and reports how far it gets.

## About

mesa-dri-probe runs oops-mesa's platform shim. It creates a GL context through the frontend and
renders only what it can verify, adding one thing at each step.

- **Fixed function** - a clear and a triangle, read back with `glReadPixels` from the drawable's
  own colour buffer.
- **The programmable pipeline** - a GLSL 330 shader drawing a colour-interpolated triangle from a
  vertex buffer: the compiler, program linking and vertex attributes.
- **The frame hash** - FNV-1a over every pixel of the finished frame, an acceptance gate that a
  single sampled pixel cannot stand in for.

It is the frontend counterpart to `mesa-winsys-probe`, which walks the winsys directly; separate
titles give a failure one screen and one attribution. A frame that does not retire is a failure,
never a fallback to software - the readback is the check.

## Screenshot

<p align="center">
  <img src="../../../common/assets/no-screenshot.svg" alt="No screenshot yet" width="600">
</p>

## Docs

- [oops-mesa bring-up docs](../../../../oops-mesa/docs/)
- [oops-apps catalog and guide](../../../docs/USER_GUIDE.md)
