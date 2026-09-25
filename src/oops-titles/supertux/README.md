# SuperTux

<p align="center">
  <img src="../../../common/assets/no-logo.svg" alt="No logo yet" width="200">
</p>

A side-scrolling platformer about a penguin — ported to the console on oops-gl's GLSL path.

## About

SuperTux is a classic 2D jump-and-run in the Super Mario shape: 468 sources of C++, a Squirrel
scripting layer every level's logic is written in, and a renderer with one GLSL program. It is
the collection's first title on oops-gl's **programmable** path.

- **Pinned by commit hash** — `c1ddb4f2`, the `v0.6.3` release. `upstream.lock` says why that
  rather than master: master has moved to **SDL3**, and `src/oops-deps/` pins SDL2.
- **Upstream's ES 2.0 renderer** — `-DUSE_OPENGLES2`, which loads `data/shader/shader100.*`.
  Its fragment shader samples three textures and generates for the console through oops-gl's
  own compiler; the water and heat-haze effects the fixed-function backend cannot draw are what
  the extra two are for.
- **Fetched, not vendored** — the upstream sources are pulled on demand; only the port's own
  `tools/`, `shim/` and `patches/` live here, and `patches/` is empty.

## Checks

```
$ make check
supertux glcheck: 53 of 53 entry points on the ES 2.0 path are in oops-gl
supertux glcheck: every entry point the ES 2.0 path calls is in oops-gl

$ make shadercheck
GENERATES  shader100.frag
COMPILES   shader100.vert
supertux shadercheck: the ES 2.0 program compiles, and its fragment shader generates
```

Together they answer "can this render": every GL function the renderer calls exists, and the
one program it loads becomes gfx1030 instructions. The first is a link question, the second a
code-generation one, and neither is answered by the other.

## The stack

C++ with exceptions and RTTI — 779 `throw` and 226 `dynamic_cast` — on the libc++abi and
libunwind build `cxx-throw` proved on hardware. PhysFS, Squirrel, sexp-cpp and tinygettext are
upstream's own submodules, fetched with the pinned tree and built with the title; audio is OpenAL
Soft, shared with SuperTuxKart; the small slice of Boost the game uses is answered from the shim.
[docs/PORTING.md](docs/PORTING.md) has each decision and its reason.

## Docs

- [Porting notes](docs/PORTING.md) — the renderer, the stack, the shim
- [Shim notes](shim/README.md)
- [oops-titles overview](../README.md)
- [oops-apps catalog & guide](../../../docs/USER_GUIDE.md)

## Upstream

[SuperTux/supertux](https://github.com/SuperTux/supertux), GPL-3.0 licensed.
