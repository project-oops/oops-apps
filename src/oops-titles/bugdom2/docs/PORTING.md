# Porting Bugdom 2

## Where it stands

**Nothing is built. The GL surface is measured, and it is covered completely:**

| | |
|---|---|
| GL entry points the game and Pomme name | **74** |
| of those, defined by `oops-gl` | **74** |
| real gaps | **0** |
| SDL | **SDL3** — the same cost Bugdom carries, shared |
| language | 97 C, 38 C++ (the C++ is almost entirely `extern/Pomme`) |
| game data | **ships with upstream** — 180 MB under `Data/` |

Three names came back "missing" and none is real. `glTextureName` is upstream's own identifier.
`glLockArraysEXT` and `glUnlockArraysEXT` are **commented out** — `Source/3D/MetaObjects.c:756` is
`//	glLockArraysEXT(0, data->numPoints);`. Worth stating plainly, because a census that counted them
would have invented a gap out of a line that does not compile.

## Almost everything here is Bugdom's story

Same engine generation, same author, same `extern/Pomme`, same immediate-mode GL 1.x (84
`glVertex3f`, 30 `glBegin`). Read [`../bugdom/docs/PORTING.md`](../bugdom/docs/PORTING.md) first:
the SDL3 question, Pomme's portability, and the submodule handling are identical and are argued
there rather than repeated here.

## What is different

- **Nearly three times the game data**, 180 MB against 66 MB. That matters for deployment rather
  than for the build: `pros restore` costs per *file*, not per byte, so the file count under `Data/`
  is what decides how long a deploy takes. Count it before the first one.
- **One tag upstream, `v4.0.0`.** Bugdom has five releases to choose between; this has no choice to
  make.
- **A slightly larger GL surface**, 74 against 51, and still entirely covered — so the extra calls
  are ones `oops-gl` already has rather than new ground.

## Do this one second

There is no reason to port both at once and one good reason not to: everything expensive here is
shared with Bugdom, so whatever the first port learns about SDL3, Pomme and libc++ is spent once and
reused. Bugdom is the smaller of the two, which makes it the cheaper place to learn it.
