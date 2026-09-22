# gl2-probe

<p align="center">
  <img src="assets/logo.png" alt="gl2-probe" width="200">
</p>

The conformance probe for **OpenGL 2.0** on oops-gl, as `gl1-probe` is for 1.0 through 1.5.

## About

gl2-probe is a shared conformance suite that pins down the programmable pipeline — shaders,
GLSL 1.20, varyings, and the per-fragment operations they run alongside. One table of checks
runs twice: against the software reference on a build machine, and against the console's own
GL 2.0 back end. A check that passes on one and fails on the other is a hardware-path bug, and
nothing else in this repository can see one.

- **46 checks, all passing** against the software reference.
- **Written to find the subtle failures** — perspective-correct varyings, floored `mod`,
  short-circuit evaluation, per-face stencil — the ones a flat test quad can't tell apart.
- **Already caught real bugs**: an always-true `gl_FrontFacing` and a sampler reading the wrong
  target, both fixed and both kept fixed by a check.

## Screenshot

<p align="center">
  <img src="../../../common/assets/no-screenshot.svg" alt="No screenshot yet" width="600">
</p>

## Docs

- **[Reference](docs/REFERENCE.md)** — the full check taxonomy, the checks worth naming, and what the two runners each measure.
- [oops-apps catalog & guide](../../../docs/USER_GUIDE.md)
