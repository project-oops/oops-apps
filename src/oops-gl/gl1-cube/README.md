# gl1-cube

<p align="center">
  <img src="assets/logo.png" alt="gl1-cube" width="200">
</p>

A rotating cube, torus and sphere drawn through `oops-gl`'s OpenGL 1.x path.

## About

gl1-cube is the collection's **pinned hardware oracle**. The frame it renders was measured on the
hardware and is checked back, register by register, by the oops-sdk test suite, so a drift in
the GL command stream shows here.

- **Ground truth for the whole pipeline.** One known scene, known vertex for vertex, that renders
  the same on the hardware and in the [Orbistoun](../../../../orbistoun/) emulator.
- **A live HUD** - GPU fence state, shader canaries, pipeline toggles and per-frame timing.
- **Driven over the network** by dropping control files into the title directory: pause, dump the
  oracle record, and more.

## Screenshot

<p align="center">
  <img src="assets/demo.gif" alt="gl1-cube running on the hardware" width="600">
</p>

## Docs

- [Reference](docs/REFERENCE.md) - building, control files, and reading the HUD.
- [oops-apps catalog and guide](../../../docs/USER_GUIDE.md)
