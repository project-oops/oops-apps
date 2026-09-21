# mesa-cube

<p align="center">
  <img src="assets/logo.png" alt="mesa-cube" width="200">
</p>

The example title for oops-mesa: create a GL context, upload geometry and a texture, draw a frame
in a loop.

## About

The three titles beside this one are probes that ask a question and stop. mesa-cube asks nothing —
it is the file a title author reads before writing their own, so it is deliberately ordinary,
plain OpenGL 3.3 core-profile practice.

- **Only three platform-specific lines** — `oops_gl_create` in place of a windowing library,
  `oops_mesa_run_init_array` at the top, and parking instead of returning at the bottom. Each is
  commented where it appears.
- **The broadest exercise of the GL stack that exists here** — a texture and sampler, a depth
  buffer with the test on, an index buffer, a matrix pipeline feeding a uniform, and a continuous
  present loop. This is an example, not a conformance test.
- **A present-cost profile.** Because it presents every frame, `oops_gl_present` reports its cost
  in four parts across the run — the measurement that decides whether the readback detile or the
  display re-tile dominates.

## Screenshot

<p align="center">
  <img src="../../../common/assets/no-screenshot.svg" alt="No screenshot yet" width="600">
</p>

## Docs

- [oops-mesa bring-up docs](../../../../oops-mesa/docs/)
- [oops-apps catalog & guide](../../../docs/USER_GUIDE.md)
