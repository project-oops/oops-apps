# gl2-cube

<p align="center">
  <img src="assets/logo.png" alt="gl2-cube" width="200">
</p>

A cube drawn with a GLSL vertex and fragment shader through oops-gl's OpenGL 2.0 path.

## About

gl2-cube is the smallest end-to-end exercise of the programmable pipeline: compile, link, bind
generic attribute arrays, run the vertex shader per vertex, interpolate the varying, run the
fragment shader per fragment - the whole path, checked against oops-gl's software reference.

- **Measurements that separate the stages** - the front face's colour shows the varying arrived,
  its neighbours catch wrong winding, and a rotation through the `mvp` uniform shows the matrix is
  read and applied.
- **The front end refuses bad GLSL**, so a compiler that accepted everything cannot pass by
  drawing something that happens to look right.
- **It runs on the hardware** through the GL 2.0 back end (`FORMATS=eboot title`).

## On the hardware

<p align="center">
  <img src="assets/demo.gif" alt="gl2-cube running on the hardware" width="800">
</p>

The HUD identifies the path the frame took:

| | |
|---|---|
| `OOPS-GL 2.0: SHADER CUBE (RDNA2)` … `GPU` | the hardware path, not the software reference |
| `GLSL compiled to gfx1030 by glsl_ps.c` | the shader was compiled to RDNA2 instructions |
| `Compiled PS: N words, M regs` | the size of the compiled pixel shader |
| `Program: 3 │ Tris: 12 │ Verts: 36` | a linked program object, drawn through generic attributes |

Each visible face is a different colour carried by the varying, so a wrong interpolation or a
wrong winding changes the picture rather than dimming it. The title hashes its framebuffer every
frame for the self-check, and that cost is in the frame time the HUD shows.

## Docs

- [oops-apps catalog and guide](../../../docs/USER_GUIDE.md)
