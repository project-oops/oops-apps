# gl2-probe

<p align="center">
  <img src="assets/logo.png" alt="gl2-probe" width="200">
</p>

The conformance probe for **OpenGL 2.0** on oops-gl, as `gl1-probe` is for 1.0 through 1.5.

## About

gl2-probe is a shared conformance suite for the programmable pipeline - shaders, GLSL 1.20,
varyings, and the per-fragment operations they run alongside. One table of checks runs twice:
against the software reference on a build machine, and against the hardware's GL 2.0 back end. A
check that passes on one and fails on the other is a hardware-path bug.

- **Written to find the subtle failures** - perspective-correct varyings, floored `mod`,
  short-circuit evaluation, per-face stencil - the ones a flat test quad cannot tell apart.
- **Regression checks** for `gl_FrontFacing` and texture sampler targets.

## Screenshot

<p align="center">
  <img src="../../../common/assets/no-screenshot.svg" alt="No screenshot yet" width="600">
</p>

## Docs

- [Reference](docs/REFERENCE.md) - the check taxonomy, the checks worth naming, and the two runners.
- [oops-apps catalog and guide](../../../docs/USER_GUIDE.md)
