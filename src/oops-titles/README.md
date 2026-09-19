# Titles

Real programs, ported to run on the console through `oops-sdk`. A probe shows a thing can be
done; a title is the reason for doing it.

**Nothing here may need the user to supply assets or ROMs to build and boot.** An emulator is
the one exception and only a partial one: it builds, boots and runs without any proprietary
code, and that a user must bring their own content afterwards does not stop it being a proof.

## The targets, and what was actually checked

Each entry below was verified by cloning it and reading the source - grepping for `glBegin` and
`glVertexPointer` against `glCreateShader` and `glUseProgram`, checking the shading language
version, and looking for the asset licence in the repository. **Nothing here is from memory**:
GL versions asserted from recollection have already been wrong twice in this project.

| Slot | Target | Verified | Language |
|---|---|---|---|
| `bring-up/` | mesa-demos | 53 programs under `src/demos/`, purpose-built per GL feature, no assets | C |
| `gl1/` | **Neverball** | fixed-function (`glEnableClientState`, `<GLES/gl.h>`), 150 MB free assets in-repo | **C**, 106 files |
| `gl1/` | Armagetron Advanced | 23 `glBegin`, 3 `glVertexPointer`, no shader calls | C++, 189 files |
| `gl1/` | Extreme Tux Racer | upstream is Subversion; a git port is the way in | - |
| `gl2/` | **Craft** | `#version 120` - GLSL 1.20 is OpenGL 2.1 - `glCreateShader`/`glUseProgram`, 14 MB with textures | **C** |
| `gl2/` | SuperTux 2 | ships `#version 100` *and* `#version 330`, so it belongs to both slots | C++, 476 files |
| `gl3/` | SuperTuxKart | its README: "OpenGL >= 3.3 or OpenGL ES >= 3.0" | C++ |
| `gl3/` | SuperTux 2 | the `#version 330` half of the same engine | C++ |
| `multi/` | **RetroArch** | ships `gl1.c`, `gl2.c` **and** `gl3.c` as separate drivers - one app across all three | C |

## Two things the checking changed

**AssaultCube was dropped.** Its licence permits redistribution only of *unmodified* packages
and states "You MAY NOT use AssaultCube for ANY commercial purposes". A console port is a
modified redistribution, so it fails the rule at the top of this file outright.

**Tesseract was demoted.** It mixes 39 `glBegin` calls with `glGenVertexArrays` and
`#version 140` shaders, so it needs fixed-function and modern GL at once - it straddles oops-gl
and oops-mesa rather than exercising either.

## The real sorting axis is C versus C++

More than the GL version. Neverball and Craft are C; everything else is substantial C++, which
on a freestanding target needs `-fno-exceptions -fno-rtti` and runtime stubs for `new`, `delete`
and static-initialisation guards. That is a known pattern and a real project, and it is a
**shared** cost - whichever C++ title lands first pays it and the rest follow cheaply.

So the order that gets a title running soonest is: **Neverball** (gl1, C), then **Craft** (gl2,
C), and then the C++ runtime once, for Armagetron, SuperTux 2 and RetroArch together.
