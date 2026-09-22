# mesa-cube

<p align="center">
  <img src="assets/logo.png" alt="mesa-cube" width="200">
</p>

The example title for oops-mesa: create a GL context, upload geometry and a texture, draw a frame
in a loop, read the pad, and stop cleanly when asked.

## About

The two probes beside this one ask a question and stop. mesa-cube asks nothing — it is the file a
title author reads before writing their own, so it is deliberately ordinary, plain OpenGL 3.3
practice that reaches for the SDK the way any oops title would.

- **Only three platform-specific lines** — `oops_gl_create` in place of a windowing library,
  `oops_mesa_run_init_array` at the top, and parking instead of returning at the bottom. Each is
  commented where it appears. The pad and the stop file are not platform quirks; they are what the
  desktop version would do with a windowing library and a close event.
- **The broadest exercise of the GL stack that exists here** — a texture and sampler, a depth
  buffer with the test on, an index buffer, a matrix pipeline (`oops/math.h`), a continuous present
  loop, and a GPU 2D overlay (`oops/hud.h`) composited over the scene. This is an example, not a
  conformance test.
- **A present-cost profile.** Because it presents every frame, `oops_gl_present` reports its cost
  in four parts across the run — the profile that settled oops-mesa D012: the present scans out
  Mesa's own tiled buffer directly, 59.94 fps vsync-locked with the CPU touching no pixel.

## Controls

The same set gl1-cube has, so the two demos behave alike:

- **Cross** — pause / resume the spin
- **Triangle** — texture on / off
- **Square** — depth test on / off
- **R1** — back-face culling on / off
- **L1** — face shading (lighting) on / off
- **Circle** or **Options** — stop the title
- a `/app0/stop` file (`pros sh touch /app0/stop`) — stop a run with nobody at the pad

The on-screen HUD is the shared dashboard from `common/cube_hud.c` — the same one gl1-cube draws.

## Screenshot

<p align="center">
  <img src="assets/demo.gif" alt="mesa-cube running on a PS5, captured over JetKVM" width="600">
</p>

## Docs

- [oops-mesa bring-up docs](../../../../oops-mesa/docs/)
- [oops-apps catalog & guide](../../../docs/USER_GUIDE.md)
