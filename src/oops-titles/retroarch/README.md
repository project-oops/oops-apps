# RetroArch

<p align="center">
  <img src="../../../common/assets/no-logo.svg" alt="No logo yet" width="200">
</p>

The libretro front end — queued for the console.

## About

RetroArch is a front end that loads emulation cores as libretro modules. It is one of the
collection's queued upstream titles, tracked against a pinned commit.

- **Pinned by commit hash** — `69a4f0ea`, the `v1.22.2` release. `upstream.lock` records that
  this one is a lightweight tag, so the hash is the commit directly — and that the tag list has
  to be sorted by version, since a lexical sort puts `v1.9.9` last, twelve releases short.
- **Fetched, not vendored** — `make` pulls the front end on demand. **No cores are pinned**; see
  below.

## Status

**A scaffold. Nothing has been measured.** The lock is verified and the layout is in place; every
claim about this title is still inherited from the survey in [`../README.md`](../README.md).

## Why it is on the list, and it is not the games

RetroArch is the only program in the survey that ships **`gl1.c`, `gl2.c` and `gl3.c` as separate
drivers**. Every other title picks a renderer; this one carries three and selects between them,
which makes it the collection's natural end-to-end exercise of oops-gl and oops-mesa together —
one program covering all three slots.

That is also why the cores are not pinned here. A core is what makes RetroArch *useful*; the
renderers are what make it worth porting early. Conflating the two is how this title becomes
open-ended.

## The shape of the work is different from the others

Craft and Neverball are programs with dependencies. RetroArch is a **framework with ports**: it
wants a window, input, audio, a filesystem, threads, a clock and dynamic loading, and it has its
own abstraction and a documented place for a new backend behind each one. A port is mostly
writing one more backend rather than patching the program — which may make it easier than its
size suggests, and is the first thing to check rather than the last.

The part with no obvious answer is **dynamic loading**: cores are shared libraries opened at run
time, and this target wants the statically-linked-core build instead. Whether that path is still
maintained upstream is a question for the first reading.

## Docs

- [Porting notes](docs/PORTING.md) — the platform layer, the driver selection, and the first job
- [oops-titles overview](../README.md)
- [oops-apps catalog & guide](../../../docs/USER_GUIDE.md)

## Upstream

[libretro/RetroArch](https://github.com/libretro/RetroArch), GPL-3.0 licensed.
