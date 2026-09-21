# glut-demo — reference

Design notes for the GLUT port. The [README](../README.md) is the overview; this is what the
build proves and how to drive the program.

## What is portable, and what is not

Everything in `glut_demo_main.c` above the last twenty lines is ordinary GLUT: `main`,
`glutInit`, `glutCreateWindow`, callbacks, `glutMainLoop`, `gluPerspective`, `gluLookAt`,
`gluBuild2DMipmaps`, `glutSolidSphere`, `glutSolidTorus`, `glutSolidCube`,
`glutSolidDodecahedron`, `glutSolidTeapot`, `glutSetWindowTitle`, `glutFullScreen`,
`glutSetCursor`, `glutBitmapString`. There is no SDK call in it, no header of this SDK's, and
nothing that names the platform. Copy it into a desktop GLUT project and it compiles there.

The last twenty lines are the part that cannot be portable: a homebrew payload is entered **by
name**, not by the C runtime, so `glut_demo_start` initialises the syscall table and calls
`main`. That is the whole of the platform-specific work in a port of this shape.

## What the build proves, and what it does not

```
make elf
```

builds the payload under `-Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion`.

**The link alone proves less than it looks.** App links pass `--unresolved-symbols=ignore-all`,
because the platform's own modules resolve `sce*` imports at load — so a missing GLUT function
would link silently and fail on the console. The check that means something is the symbol table:

```
make elf                                  # not `make dist` — see below
nm -u build/glut-demo.elf | grep -v sce   # only sysctlbyname, a platform import
nm build/glut-demo.elf | grep -c ' [TtWw] \(glut\|glu\)[A-Z]'   # every glut*/glu* name defined
```

Every `glut*` and `glu*` name this program calls is **defined in the payload**; nothing is
missing and waiting to fail later.

**Run those on a freshly linked ELF.** `mkmodule` rewrites `build/glut-demo.elf` **in place**
during `make dist`, setting the `e_type` the loader wants — and `nm` then answers `file format
not recognized`, which reads like a broken payload and is not one. `make elf` leaves an ordinary
`ET_DYN` behind. `app.mk` runs this same check automatically as part of the link, before the tag
step, and fails the build and deletes the ELF if anything is undefined; the by-hand form is for
looking at what it found.

## The teapot

The teapot is the one GLUT solid that is not a formula: every other shape here comes out of a
quadric or the vertices of a platonic solid, while `glutSolidTeapot` is measured Bézier-patch
data run through `glMap2f`/`glEvalMesh2`. Its silhouette would be wrong if the evaluator maths
were, which is why it earns its place in a demo that exists to be evidence. `glutSolidTeapot(0.5)`
is about 1.6 units across, not 0.5 — GLUT's size argument is a scale on the source data, not a
diameter.

## Controlling it

There is no keyboard on most consoles, so the pad stands in — the SDK's GLUT maps the d-pad to
the arrow specials, cross and circle to Return and Escape, and the **option button leaves the
main loop**. Escape or `q` on a real keyboard does the same. `glutOopsPadKeys(0)` turns that off
for a program that reads the pad itself.
