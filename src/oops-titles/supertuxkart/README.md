# SuperTuxKart

<p align="center">
  <img src="../../../common/assets/no-logo.svg" alt="No logo yet" width="200">
</p>

Kart racing — queued for the console.

## About

SuperTuxKart is a 3D kart racer with a modern renderer, a large C++ codebase and about a gigabyte
of content in a second repository. It is one of the collection's queued upstream titles, tracked
against a pinned commit.

- **Pinned by commit hash** — `1fb491f5`, the `1.5` release. `upstream.lock` records why the tag
  has no `v`: SuperTuxKart changed tag shape at 1.0, and a `v`-prefixed search stops at `v0.9`,
  which looks like the newest release and is six years older than it.
- **Fetched, not vendored** — `make` pulls `stk-code` on demand. **It does not pull
  `stk-assets`**, which the game needs and which is the gigabyte; see the notes.

## Status

**A scaffold. Nothing has been measured**, and for this title that is worth stating twice.

Its row in [`../README.md`](../README.md)'s survey reads, in full, *its README: "OpenGL >= 3.3 or
OpenGL ES >= 3.0"*. That is upstream's own statement of requirements, not a reading of upstream's
renderer — every other row in that table was taken by grepping the source. A stated requirement
and what the code does are different facts, and this collection has already been caught by the
gap between them: SuperTux was sorted by the shaders it ships into a slot its fixed-function
backend does not use.

So the GL 3.3 claim here is a **citation, not a measurement**, and the first job is to replace it
with one.

## What is in the way

| | |
|---|---|
| **A renderer that may not exist yet** | if the README is right, this needs GL 3.3 core — which is oops-mesa's territory, not oops-gl's |
| **The assets** | `stk-assets` is a second repository of about a gigabyte, and the mechanism to bring it is undecided |
| **The C++ runtime** | shared cost, assigned to Extreme Tux Racer first |

## Docs

- [Porting notes](docs/PORTING.md) — the assets question, and why the first job is reading the renderer
- [oops-titles overview](../README.md)
- [oops-apps catalog & guide](../../../docs/USER_GUIDE.md)

## Upstream

[supertuxkart/stk-code](https://github.com/supertuxkart/stk-code), GPL-3.0 licensed.
Assets: [supertuxkart/stk-assets](https://github.com/supertuxkart/stk-assets), CC-BY-SA.
