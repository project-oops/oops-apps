# glut-demo - reference

Design notes for the GLUT port. The [README](../README.md) is the overview; this is what the
build checks and how to drive the program.

## Portable and platform code

Everything in `glut_demo_main.c` above the last twenty lines is ordinary GLUT: `main`,
`glutInit`, `glutCreateWindow`, callbacks, `glutMainLoop`, `gluPerspective`, `gluLookAt`,
`gluBuild2DMipmaps`, `glutSolidSphere`, `glutSolidTorus`, `glutSolidCube`,
`glutSolidDodecahedron`, `glutSolidTeapot`, `glutSetWindowTitle`, `glutFullScreen`,
`glutSetCursor`, `glutBitmapString`. There is no SDK call in it, no header of this SDK's, and
nothing that names the platform.

The last twenty lines are platform-specific: a homebrew payload is entered by name, not by the C
runtime, so `glut_demo_start` initialises the syscall table and calls `main`. That is the whole
of the platform-specific work in a port of this shape.

## The symbol check

```
make elf
```

builds the payload under `-Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion`.

App links pass `--unresolved-symbols=ignore-all`, because the platform's own modules resolve
`sce*` imports at load, so a missing GLUT function would link silently and fail on the hardware.
The symbol table is the check:

```
make elf                                  # not `make dist` - see below
nm -u build/glut-demo.elf | grep -v sce   # only sysctlbyname, a platform import
nm build/glut-demo.elf | grep -c ' [TtWw] \(glut\|glu\)[A-Z]'   # every glut*/glu* name defined
```

Every `glut*` and `glu*` name this program calls is defined in the payload.

Run those on a freshly linked ELF. `mkmodule` rewrites `build/glut-demo.elf` in place during
`make dist`, setting the `e_type` the loader wants, and `nm` then answers `file format not
recognized`. `make elf` leaves an ordinary `ET_DYN` behind. `app.mk` runs the same check as part
of the link, before the tag step, and fails the build and deletes the ELF if anything is
undefined; the by-hand form is for inspecting what it found.

## The teapot

The teapot is the one GLUT solid that is not a formula: every other shape here comes from a
quadric or the vertices of a platonic solid, while `glutSolidTeapot` is measured Bézier-patch
data run through `glMap2f`/`glEvalMesh2`. Its silhouette is wrong if the evaluator maths is.
`glutSolidTeapot(0.5)` is about 1.6 units across, not 0.5 - GLUT's size argument is a scale on
the source data, not a diameter.

## Controls

The pad stands in for a keyboard: the SDK's GLUT maps the d-pad to the arrow specials, cross and
circle to Return and Escape, and the option button leaves the main loop. Escape or `q` on a
keyboard does the same. `glutOopsPadKeys(0)` turns the mapping off for a program that reads the
pad itself.
