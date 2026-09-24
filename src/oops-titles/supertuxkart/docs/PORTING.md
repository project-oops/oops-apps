# Porting SuperTuxKart

Pinned at `1.5` (`1fb491f5`), the revision `upstream.lock` names.

## Where it stands

**A scaffold. Nothing here has been measured.** The lock is verified and the layout is in place;
every claim below is inherited from the survey in [`../README.md`](../README.md) and is waiting
for somebody to read the source.

That distinction matters more than usual here, because **the survey's row for this title is not
a measurement at all**. It reads, in full, *its README: "OpenGL >= 3.3 or OpenGL ES >= 3.0"* -
which is upstream's own statement of requirements, not a reading of upstream's renderer. Every
other row in that table was taken by grepping the source. This one was taken from a sentence.

A README's stated requirement and what the code does are different facts, and the collection has
already been caught by the gap between them once: SuperTux was sorted by the shaders it ships
into a slot its fixed-function backend does not use.

## What is known, and how well

| | confidence |
|---|---|
| GL 3.3 core or GLES 3.0 | upstream's README says so. Nobody has checked whether a lower path exists |
| C++ | certain |
| Needs `stk-assets`, a second repository of about a gigabyte | certain, and see below |

## The assets

`stk-code` is the program and `stk-assets` is the karts, tracks and audio. The game does not
start without both, and only the first is pinned by `upstream.lock`.

Three mechanisms exist in this collection and none has been chosen:

1. **A second lock in this directory.** Closest to how `upstream.lock` already works, and keeps
   the revision under this repository's control.
2. **An `src/oops-deps/` entry**, as SDL2, FreeType and the rest are. That directory is for
   libraries rather than content, so this would stretch it.
3. **Fetched by the package step**, the way Neverball's `make package` copies `upstream/data`
   into the title directory after `make title` builds it.

Whichever it is, it is a gigabyte that must not arrive as a surprise during an ordinary `make`.
This title's `Makefile` deliberately fetches only `stk-code` and says so.

## Why this is late in the queue

If the README is right, SuperTuxKart needs GL 3.3 core - which is **oops-mesa's** territory, not
oops-gl's. oops-gl is a hand-written GL 1.x/2.0 back end; the GL 3.3 path in this collection goes
through Mesa, which is separately tracked and whose entry-point coverage is its own open
question.

So this title is not blocked on the same things Craft and SuperTux are. It is blocked on a
different renderer being ready, plus the shared C++ runtime cost, plus the assets question above.

## The first job

**Read the renderer before anything else, and specifically look for a lower path.** SuperTux's
lesson is that a title can ship a modern renderer and a legacy one and choose at run time. If
SuperTuxKart has no such arm, that is worth recording as a measured fact rather than an inherited
sentence - it is the difference between "waits for oops-mesa" and "waits for nothing".
