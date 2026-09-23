# mesa-demos

Upstream Mesa's own demonstration programs, running on the console through oops-mesa.

## About

This is the `bring-up/` slot from [the titles README](../README.md), and it is the only title
here whose purpose is measurement rather than play. The 56 programs under `upstream/src/demos/`
were written by the Mesa project to exercise one OpenGL feature each and show it working, which
makes them the cheapest honest answer to *what of OpenGL actually runs on this console*.

- **Pinned by tag** — `mesa-demos-9.0.0`, peeled commit `661681767bfb`. A measuring instrument
  wants a reproducible origin; `upstream.lock` says why that is the opposite of Craft's choice.
- **Fetched, not vendored** — only `shim/`, the Makefile and the lock live here.
- **No patches.** The demos compile unmodified.

## Build and run

```
make title              # gears, the default
make title DEMO=engine  # any other name from upstream/src/demos/
make imports            # after a link, before `make title` - see the Makefile
```

One demo per build, and the reason is in the Makefile: a picker would be *our* code standing
between the console and the program, and the value here is that the program is not ours.

OPTIONS on the pad ends a demo, through `glutLeaveMainLoop`.

## What this title demonstrates, which is not a picture

**`OOPS_RENDERER = mesa` is the whole point.** oops-sdk's GLUT (`src/gl/glut.c`) names no OpenGL
implementation — it goes through `oops/gfx.h` and otherwise calls plain GL — so that one line in
the Makefile decides whether these demos run on oops-gl or on upstream Mesa with radeonsi and
ACO. This is the first thing in the collection that is somebody else's GL program running on
oops-mesa; everything before it was our own probe or our own cube.

## The two shim files, and when they go

Neither is part of the port. Both work around one thing, and both delete together.

| file | what it does |
|---|---|
| `glu_maths_from_libm.c` | `gl_sin`, `gl_cos`, `gl_sqrt` over libm |
| `glu_quadrics_on_mesa.c` | GLU's quadrics, **drawn** — `gluSphere`, `gluCylinder`, `gluDisk` |
| `glu_matrix_on_mesa.c` | the pure-maths half: `gluPerspective`, `gluLookAt`, mipmap building |

oops-sdk's GLU (`src/gl/gl_glu.c`) reaches into oops-gl for its maths and pixel helpers, and
those live in files that between them define forty OpenGL entry points — so linking it beside
Mesa would put a second `glMatrixMode` in the binary. GLUT's solid shapes are built on GLU
quadrics, so they are unavailable here until that is decoupled.

The quadrics began as no-ops that logged their own absence, on the reasoning that a shape drawn
wrong is worse than a shape not drawn. That was right about the principle and wrong about the
cost: `cubemap` reflection-maps a `glutSolidSphere`, so it ran on hardware and drew a correct room
around **nothing**, and twelve more demos call quadrics directly. A quadric is a parametric
surface with a closed form, so writing one is transcription rather than interpretation — which is
what makes it safe here where a mipmap filter would not have been. `gluPartialDisk` is still a
reporting no-op: nothing in the set calls it, so nothing would check it.

Filed as `REQ-20260922T0940Z-5c17` against oops-sdk. `gears`, the default, calls none of them.

## The census

**37 of 56 demos build, link and place every symbol.** Up from 7 when the set was first swept on
2026-09-22. `tools/sweep.sh` produces this and `build/sweep.tsv` holds the rows; run it again
after any change that could move one.

| | baseline | now |
|---|---|---|
| **OK** — compiles, links, every symbol placed | 7 | **37** |
| **BUILD-FAIL** — does not compile | 46 | 15 |
| **IMPORTS-FAIL** — compiles, but a symbol is undefined | 3 | 4 |

**None of that means a demo draws anything.** The three outcomes are about the port surface, and
the whole point of this title is the measurement that comes *after* they are all green — six have
had that measurement so far, and they are listed below.

### What got them there

Nothing in the first sweep's 46 failures was about OpenGL. Every one was the port surface:

- **glad** — 27 demos include it. mesa-demos 9.0.0 loads GL through it, and the loading half is a
  no-op in a static link. `shim/include/glad/glad.h` answers the eleven flags they read, from the
  driver rather than by assertion.
- **The proportional GLUT font names** — 20 demos. Mapped in `shim/include/glut_proportional_fonts.h`
  rather than in oops-sdk, for the reason that header gives.
- **GLU** — `shim/glu_matrix_on_mesa.c` carries the half that is pure maths, written from the
  specification. oops-sdk's own GLU cannot be linked beside Mesa (`REQ-20260922T0940Z-5c17`).
- **Three upstream util sources** — `readtex.c`, `showbuffer.c`, and oops-sdk's `glut_font.c`,
  each named by the import check rather than guessed at.

### The 19 that do not, and why

| blocked on | demos | disposition |
|---|---|---|
| `glutCreateMenu` | 10 — `engine`, `gloss`, `isosurf`, `multiarb`, `pointblast`, `projtex`, `renormal`, `spectex`, `spriteblast`, `texcyl` | **oops-sdk's call.** `PORTING.md` lists GLUT menus as deliberately absent. Filed with the argument that all of them attach to `GLUT_RIGHT_BUTTON` and this console has no mouse, so the menu is unreachable anyway and a recording no-op would cost nothing that works today. Not implemented here, because routing around a documented decision quietly is worse than losing ten demos |
| `glutSetColor` | 4 — `bounce`, `copypix`, `drawpix`, `readpix` | **Out of scope, and correctly so.** These want colour-index visuals, and `PORTING.md` already answers that: `glutGet(GLUT_DISPLAY_MODE_POSSIBLE)` reports 0 for them |
| `GLUtriangulatorObj` | 1 — `dinoshade` | The GLU tessellator, which `PORTING.md` also lists as absent. A real one is a large piece of work for one demo |
| **extensions this Mesa build does not provide** | 4 — `fplight` (`glLoadProgramNV` and the NV program family), `paltex` (`glColorTableEXT`), `vao_demo` (`glBindVertexArrayAPPLE`), `winpos` (`glWindowPos2fMESA`) | **Out of scope for this configuration.** The symbols are in no archive in the build, and `generate-imports.sh` refuses to bind them to the *platform's own* GL — which is the check working. A stub would produce a demo that runs and demonstrates nothing, which is the one outcome this title must not produce |

### Which of the 37 are worth a hardware run

All of them, but not all at once and not all equally.

**Twelve need `make stage-data` first** and will find nothing without it: `dissolve`,
`fbo_firecube`, `fire`, `geartrain`, `ipers`, `lodbias`, `reflect`, `teapot`, `terrain`,
`textures`, `tunnel`, `tunnel2`. Compiling is not finding.

**The other twenty-five need nothing but a slot.** `clearspd`, `trispd` and `gltestperf` are
throughput measurements rather than pictures and would say something about the present path
rather than about GL. `cubemap`, `fbotexture`, `shadowtex` and `stex3d` each exercise a feature
no probe here had touched - cube maps, framebuffer objects, shadow comparison, 3D textures - and
are the most informative per run.

Six have now run on this console:

| demo | what it settled |
|---|---|
| `gears` | the title arrangement works at all |
| `glinfo` | [the advertised surface, 322 extensions](../../../../oops-mesa/docs/hardware/the-advertised-surface-measured-fw1240.md) — and three bugs in the log path, which is why a program that only prints was worth a slot |
| `cubemap` | [cube mapping and reflection texgen draw](../../../../oops-mesa/docs/hardware/cube-mapping-draws-fw1240.md), on a sphere this title's own shim tessellates |
| `fbotexture` | [render to texture with depth and stencil](../../../../oops-mesa/docs/hardware/render-to-texture-with-depth-and-stencil-fw1240.md) — a full off-screen pass, sampled back, correct in every part. Then [it animated](../../../../oops-mesa/docs/hardware/third-frame-with-an-fbo-faults-the-gpu-fw1240.md) once the winsys learned to walk radeonsi's command-buffer chain, which it needs ~13 times a frame |
| `shadowtex` | [shadow mapping through `GL_ARB_fragment_program`](../../../../oops-mesa/docs/hardware/shadow-mapping-and-arb-fragment-program-fw1240.md) — depth-comparison sampling and the assembly shader path, both new here |
| `stex3d` | [solid texturing from a 3D texture](../../../../oops-mesa/docs/hardware/solid-texturing-a-3d-texture-on-hardware-fw1240.md) — a torus carved out of a noise volume, no seam and no collapsed slice |

`fbotexture` is also the one to reach for when a demo's log looks stalled: its `Anim` defaults to
false, so **one** `present us:` line is its specified behaviour. And `a` is the cheapest keyboard
test in the set — one press turns that single line into a stream.

The demo-by-demo *result* table goes in `oops-mesa/docs/hardware/` when those runs happen, named
by build and firmware like every other hardware record in the collection. This section is the
build census and is not that table.
