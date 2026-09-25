# Bugdom 2

<p align="center">
  <img src="../../../common/assets/no-logo.svg" alt="No logo yet" width="200">
</p>

Pangea's sequel — queued for the console, behind [Bugdom](../bugdom).

## About

Bugdom 2 is the 2002 sequel, ported by jorio from the same codebase generation: 97 C sources and 38
C++, drawing through immediate-mode OpenGL 1.x. It is tracked against a pinned commit.

- **Pinned by commit hash** — `4050d6f9`, `v4.0.0`, the only tag upstream has. A **lightweight** tag.
- **Fetched, not vendored** — `make` pulls upstream on demand.
- **Brings its own game data** — 180 MB under `Data/`, nearly three times Bugdom's.
- **Submodule** — `extern/Pomme`, the same as Bugdom's.

## What is measured

**`oops-gl` covers all 74 GL entry points it names.** Two that looked missing,
`glLockArraysEXT`/`glUnlockArraysEXT`, are commented out in upstream's own source.

Everything expensive here is shared with Bugdom — SDL3, Pomme, libc++ — so this should follow it
rather than run alongside it. Bugdom is the smaller of the two and the cheaper place to learn the
shared parts. [`docs/PORTING.md`](docs/PORTING.md) says what differs.
