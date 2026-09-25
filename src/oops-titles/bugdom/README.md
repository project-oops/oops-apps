# Bugdom

<p align="center">
  <img src="../../../common/assets/no-logo.svg" alt="No logo yet" width="200">
</p>

Pangea's garden adventure — queued for the console.

## About

Bugdom is a 1999 Macintosh game, released as freeware and ported to modern systems by jorio: 78 C
sources and 38 C++, drawing through immediate-mode OpenGL 1.x. It is tracked against a pinned commit.

- **Pinned by commit hash** — `18b413f8`, the `1.3.4` release, newest of five. A **lightweight** tag.
- **Fetched, not vendored** — `make` pulls upstream on demand.
- **Brings its own game data** — 66 MB under `Data/`. Nothing is required from the player.
- **Submodule** — `extern/Pomme` is one, so the fetch needs `UPSTREAM_SUBMODULES=1`. Ship of
  Harkinian's lock is the precedent.

## What is measured

**`oops-gl` covers Bugdom's GL surface completely** — all 51 entry points it names. The renderer is
`glBegin`/`glVertex3f` immediate mode, the same shape as Neverball and Extreme Tux Racer, so
`OOPS_RENDERER = gl1` is the target and there is no shader question.

**The cost is SDL3.** Upstream requires it and this collection vendors SDL2. That is a second SDL
backend rather than a version bump, and `sdl2-compat` does not help — it runs SDL2 apps on SDL3, not
the reverse. Whether to pay it here or patch Bugdom back to SDL2 is unsettled;
[`docs/PORTING.md`](docs/PORTING.md) sets out both.

`extern/Pomme`, jorio's reimplementation of the classic Mac Toolbox, turns out to be genuinely
portable: 5 of its 69 sources mention any Apple API. It is C++ on SDL, so it inherits the SDL3
question and adds a libc++ dependency.
