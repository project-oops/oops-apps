# gl2-cube

<p align="center">
  <img src="assets/logo.png" alt="gl2-cube" width="200">
</p>

A cube drawn with a GLSL vertex and fragment shader through oops-gl's OpenGL 2.0 path.

## About

gl2-cube is the smallest end-to-end exercise of the programmable pipeline: compile, link, bind
generic attribute arrays, run the vertex shader per vertex, interpolate the varying, run the
fragment shader per fragment — the whole path, checked against oops-gl's software reference.

- **Measurements chosen to separate the stages** — the front face's colour proves the varying
  arrived, its neighbours catch wrong winding, and a rotation through the `mvp` uniform proves the
  matrix is read and applied.
- **The front end still refuses bad GLSL**, so a compiler that accepted everything can't pass by
  drawing something that happens to look right.
- **Host only, by design.** There is no hardware GL 2.0 back end yet, and the draw path refuses a
  program rather than faking one — so nothing here can quietly differ from a console answer that
  doesn't exist.

## Screenshot

<p align="center">
  <img src="../../../common/assets/no-screenshot.svg" alt="No screenshot yet" width="600">
</p>

## Docs

- [oops-apps catalog & guide](../../../docs/USER_GUIDE.md)
