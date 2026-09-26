# mesa-demos

Upstream Mesa's own demonstration programs, running on the hardware through oops-mesa.

## About

This is the `bring-up/` slot among the titles, and the only title here whose purpose is
measurement rather than play. The programs under `upstream/src/demos/` were written by the Mesa
project to exercise one OpenGL feature each, which makes them a direct measure of what OpenGL
runs on the hardware.

- **Pinned by tag** - a measuring instrument wants a reproducible origin; `upstream.lock` says
  why that is the opposite of Craft's choice.
- **Fetched, not vendored** - only `shim/`, `tools/`, the Makefile and the lock live here.
- **No patches.** The demos compile unmodified.

## Build and run

```
make title              # gears, the default
make title DEMO=engine  # any other name from upstream/src/demos/
make imports            # after a link, before `make title` - see the Makefile
make stage-data         # after `make title`: copy the demos' data files into it
```

One demo per build: a picker would be our code standing between the hardware and the program,
and the value here is that the program is not ours.

OPTIONS on the pad ends a demo, through `glutLeaveMainLoop`.

Demos that read files (`dissolve`, `fbo_firecube`, `fire`, `geartrain`, `ipers`, `lodbias`,
`reflect`, `teapot`, `terrain`, `textures`, `tunnel`, `tunnel2`) need `make stage-data`, which
copies every asset into `/app0/data/` whichever demo is built.

`fbotexture`'s `Anim` defaults to false, so one `present us:` line in its log is its specified
behaviour; `a` on a keyboard turns it into a stream.

[`tools/sweep.sh`](tools/README.md) builds every demo and records whether each compiles, links and
places every symbol.

## The renderer

**`OOPS_RENDERER = mesa` is the point.** oops-sdk's GLUT (`src/gl/glut.c`) names no OpenGL
implementation - it goes through `oops/gfx.h` and otherwise calls plain GL - so that one line in
the Makefile decides whether these demos run on oops-gl or on upstream Mesa with radeonsi and
ACO.

## The port surface

| file | what it does |
|---|---|
| `shim/include/glad/glad.h`, `shim/glad_from_driver.c` | mesa-demos loads GL through glad, and the loading half is a no-op in a static link. The shim answers the flags the demos read, from the driver |
| `shim/include/glut_proportional_fonts.h` | the proportional GLUT font names, mapped here rather than in oops-sdk for the reason that header gives |
| `shim/mesa_demos_entry.c` | the entry point |

GLUT and GLU come from oops-sdk through `OOPS_FEATURES`, and the build also compiles upstream's
`readtex.c` and `showbuffer.c`. The Makefile explains why oops-sdk's `src/gl/glut.c` drives Mesa
without bringing oops-gl into the link.

## Demos outside the port

| needs | demos | why |
|---|---|---|
| `glutCreateMenu` | `engine`, `gloss`, `isosurf`, `multiarb`, `pointblast`, `projtex`, `renormal`, `spectex`, `spriteblast`, `texcyl` | oops-sdk's `PORTING.md` lists GLUT menus as absent. The menus attach to `GLUT_RIGHT_BUTTON`, and the decision is oops-sdk's |
| `glutSetColor` | `bounce`, `copypix`, `drawpix`, `readpix` | colour-index visuals; `glutGet(GLUT_DISPLAY_MODE_POSSIBLE)` reports 0 for them |
| `GLUtriangulatorObj` | `dinoshade` | the GLU tessellator, which oops-sdk's `PORTING.md` lists as absent |
| extensions this Mesa build does not provide | `fplight` (`glLoadProgramNV` and the NV program family), `paltex` (`glColorTableEXT`), `vao_demo` (`glBindVertexArrayAPPLE`), `winpos` (`glWindowPos2fMESA`) | the symbols are in no archive in the build, and `generate-imports.sh` refuses to bind them to the platform's own GL. A stub would produce a demo that runs and demonstrates nothing |

Results from hardware runs are recorded in `oops-mesa/docs/hardware/`, named by build and
firmware like every other hardware record in the collection.
