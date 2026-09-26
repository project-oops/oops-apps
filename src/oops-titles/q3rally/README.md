# Q3Rally

<p align="center">
  <img src="../../../common/assets/no-logo.svg" alt="No logo yet" width="200">
</p>

Arcade rally combat on ioquake3.

## About

Q3Rally is a standalone racing game built on the ioquake3 engine: C sources, no C++, drawing
through ioquake3's `renderergl1` fixed-function renderer, on the SDL2 already vendored in
`src/oops-deps/sdl2`. It is tracked against a pinned commit.

- **Pinned by commit hash** to the `v0.7d` release. It is a lightweight tag, so the ref and the
  commit are the same object; `upstream.lock` records that.
- **Fetched, not vendored** - `make` pulls upstream on demand; only the port's own `shim/`,
  `patches/` and notes live here.
- **Brings its own game data** under `baseq3r/`. Nothing has to be supplied by the player.

[`docs/PORTING.md`](docs/PORTING.md) covers the source list, the POSIX surface, the by-name GL
surface, the bytecode VM and the data package.
