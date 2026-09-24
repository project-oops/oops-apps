# Porting RetroArch

Pinned at `v1.22.2` (`69a4f0ea`), the revision `upstream.lock` names.

## Where it stands

**A scaffold. Nothing here has been measured.** The lock is verified and the layout is in place;
every claim below is inherited from the survey in [`../README.md`](../README.md) and is waiting
for somebody to read the source.

## Why this title is interesting, and it is not the games

RetroArch is the only program in the survey that ships **`gl1.c`, `gl2.c` and `gl3.c` as separate
drivers**. Every other title picks a renderer; this one carries three and selects between them.

That makes it the collection's natural end-to-end exercise of oops-gl and oops-mesa together -
one program whose own driver selection covers all three slots - and it is why the survey files it
under `multi/` rather than a GL version.

It is also why **the emulation cores are not the point here and are not pinned**. A core is what
makes RetroArch useful; the renderers are what make it worth porting early. Those are different
projects and conflating them is how this title would become open-ended.

## What is known, and how well

| | confidence |
|---|---|
| Ships three GL drivers as separate files | certain - it is why the title is on the list |
| C | certain |
| Which driver is selected, and how | **unknown.** Nobody has read the selection logic |
| Platform layer surface | **unknown, and it is the whole job** |

## The platform layer is the job

RetroArch is a front end. It wants a window, input, audio, a filesystem, threads, a clock,
dynamic loading for cores, and a configuration directory - and it has its own abstraction for
each, with a backend per platform. A port is mostly **writing one more backend**, not patching
the program.

That is a different shape from every other title in this directory. Craft and Neverball are
programs with dependencies; RetroArch is a framework with ports, and it has a documented place
for a new one. That is likely to make it easier than its size suggests and should be checked
before its 476-times-larger source tree is taken as a measure of the work.

**Dynamic loading is the part with no obvious answer here.** Cores are shared libraries opened at
run time. RetroArch can also be built with a core linked in statically - which is almost
certainly what this target wants - and whether that path is still maintained upstream is a
question for the first reading.

## The first job

1. **Find the platform-layer interface and count it.** How many functions, and how many of them
   oops-sdk already answers. That number decides whether this is a week or a quarter.
2. **Read the GL driver selection.** Which of the three runs by default, and what it keys on -
   the same question SuperTux's scaffold exists to answer, and the same reason.
3. **Check the static-core build path.** If it is gone, that changes the shape of everything
   above it.
