# glut-demo

A GLUT program, ported to this console by compiling it.

This app exists to be **evidence**, not a demonstration. oops-sdk grew `<GL/glut.h>` and the rest
of GLU on 2026-09-20, and the claim that made was that a program written against GLUT builds
against this SDK. The way to know is to write one the way that code is actually written and build
it for the target.

## What is portable, and what is not

Everything in `glut_demo_main.c` above the last twenty lines is ordinary GLUT: `main`,
`glutInit`, `glutCreateWindow`, callbacks, `glutMainLoop`, `gluPerspective`, `gluLookAt`,
`gluBuild2DMipmaps`, `glutSolidSphere`, `glutSolidTorus`, `glutSolidCube`,
`glutSolidDodecahedron`, `glutSetWindowTitle`, `glutFullScreen`, `glutSetCursor`,
`glutBitmapString`. There is no SDK call in it, no header of this SDK's, and nothing that names
the platform. Copy it into a desktop GLUT project and it compiles there.

The last twenty lines are the part that cannot be portable: a homebrew payload is entered **by
name**, not by the C runtime, so `glut_demo_start` initialises the syscall table and calls
`main`. That is the whole of the platform-specific work in a port of this shape.

## What the build proves, and what it does not

```
make elf
```

builds the payload under `-Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion`.

**The link alone proves less than it looks.** App links pass `--unresolved-symbols=ignore-all`,
because the platform's own modules resolve `sce*` imports at load - so a missing GLUT function
would link silently and fail on the console. The check that means something is the symbol table:

```
nm -u build/glut-demo.elf | grep -v sce   # only sysctlbyname, a platform import
nm build/glut-demo.elf | grep -c ' [TtWw] \(glut\|glu\)[A-Z]'   # 68 defined
```

Every `glut*` and `glu*` name this program calls is **defined in the payload**; nothing is
missing and waiting to fail later.

What it does not prove is what the frame looks like. Nothing here has run on a console: this app
has never been deployed or launched, and the GL it drives is the same GL the other apps drive, on
a path whose remaining gaps are recorded in `oops-sdk/docs/GL_ROADMAP.md`. The claim is about
building and linking, which is the part a port stumbles over first.

## Controlling it

There is no keyboard on most consoles, so the pad stands in - the SDK's GLUT maps the d-pad to
the arrow specials, cross and circle to Return and Escape, and the **option button leaves the
main loop**. Escape or `q` on a real keyboard does the same. `glutOopsPadKeys(0)` turns that off
for a program that reads the pad itself.
