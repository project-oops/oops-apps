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
| `glu_absent_on_mesa.c` | GLU's six quadric entry points as reporting no-ops |

oops-sdk's GLU (`src/gl/gl_glu.c`) reaches into oops-gl for its maths and pixel helpers, and
those live in files that between them define forty OpenGL entry points — so linking it beside
Mesa would put a second `glMatrixMode` in the binary. GLUT's solid shapes are built on GLU
quadrics, so they are unavailable here until that is decoupled.

A demo that calls one prints a line naming it and draws nothing. **Deliberately not a stub that
draws something else**: a substituted shape would make a demo *look* like it ran, and this
title's entire job is measuring what works.

Filed as `REQ-20260922T0940Z-5c17` against oops-sdk. `gears`, the default, calls none of them.

## Status

`gears` builds, links, passes the import check with **0 unknown symbols**, and packages as
`MDEM00001`. It deployed and launched on hardware on 2026-09-22.

The demo-by-demo table this title exists to produce does not exist yet. It goes in
`oops-mesa/docs/hardware/` when the runs are done, named by build and firmware like every other
hardware record in the collection.
