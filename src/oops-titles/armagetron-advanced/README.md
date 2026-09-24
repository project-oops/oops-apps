# Armagetron Advanced

<p align="center">
  <img src="../../../common/assets/no-logo.svg" alt="No logo yet" width="200">
</p>

Lightcycle arena combat — queued for the console.

## About

Armagetron Advanced is a 3D lightcycle game in the Tron shape: 189 sources of C++, drawing
through immediate-mode OpenGL. It is one of the collection's queued upstream titles, tracked
against a pinned commit.

- **Pinned by commit hash** — `036daaf3`, the `v0.2.9.3.0` release. `upstream.lock` records that
  this is an **annotated** tag, so the hash is the commit rather than the tag object — the trap
  Neverball's lock warns about, which caught this one too.
- **Fetched, not vendored** — `make` pulls upstream on demand; only the port's own `shim/`,
  `patches/` and notes live here.

## Status

**A scaffold. Nothing has been measured.** The lock is verified and the layout is in place; every
claim about this title is still inherited from the survey in [`../README.md`](../README.md).

`make` fetches upstream and does nothing else. There is no source list, because nobody has read
upstream's build and a list written from a glob would be a guess wearing the shape of a fact.

## Why it is last rather than second

Not the graphics — 23 `glBegin` call sites and no shaders puts it in the `gl1/` slot beside
Neverball, which already runs end to end. The cost is **exceptions, RTTI and part of boost**, and
the collection's plan is that Extreme Tux Racer pays the shared C++ runtime bill first precisely
because it needs none of them.

Worth re-checking before that ordering is taken as settled: C++ exceptions are measured working
on this target, 5 of 5 on firmware 12.40. Whether that covers what Armagetron asks for has never
been measured in either direction.

## Docs

- [Porting notes](docs/PORTING.md) — what is inherited, what is unknown, and the first job
- [oops-titles overview](../README.md)
- [oops-apps catalog & guide](../../../docs/USER_GUIDE.md)

## Upstream

[ArmagetronAd/armagetronad](https://github.com/ArmagetronAd/armagetronad), GPL-2.0 licensed.
