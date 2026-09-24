# SuperTux

<p align="center">
  <img src="../../../common/assets/no-logo.svg" alt="No logo yet" width="200">
</p>

A side-scrolling platformer about a penguin — ported to the console.

## About

SuperTux is a classic 2D jump-and-run in the Super Mario shape: 476 sources of C++, a Squirrel
scripting layer every level's logic is written in, and a renderer with two OpenGL backends it
chooses between at run time. It is one of the collection's ported upstream titles, tracked
against a pinned commit.

- **Pinned by commit hash** — `c1ddb4f2`, the `v0.6.3` release. `upstream.lock` says why that
  rather than master, and the reason is SDL rather than age: master has moved to **SDL3** and
  `src/oops-deps/` pins SDL2.
- **Fetched, not vendored** — the upstream sources are pulled on demand; only the port's own
  `tools/`, `shim/` and `patches/` live here. There are no patches yet, and one candidate is
  already written down.

## Status

**The graphics are nearly free and the C++ stack is the whole job**, which is the opposite of
what this title was queued under.

```
$ make check
supertux glcheck: upstream/src/video/gl against .../oops-sdk/include/GL/gl.h
  gl20_context.cpp            30 call sites
  gl33core_context.cpp        29 call sites   (GL33Core only)
  gl_framebuffer.cpp           5 call sites
  gl_pixel_request.cpp        13 call sites   (not compiled here)
  ...
  absent from oops-gl, on the GL 2.0 path:
    glBindFramebuffer  glDeleteFramebuffers  glFramebufferTexture2D  glGenFramebuffers

supertux glcheck: 29 of 33 entry points on the GL 2.0 path are in oops-gl
supertux glcheck: 4 known gap(s), all of them the framebuffer object - unchanged
```

**SuperTux's GL 2.0 backend uses no shaders at all.** `GL20Context` is fixed-function GL 1.x —
`glMatrixMode`, `glOrtho`, `glEnableClientState`, `glVertexPointer`, `glColor4f`, `glDrawArrays`
— and it binds one texture, ignoring the displacement texture its own interface hands it. The
four files in `data/shader/` belong to the GLES2 and GL33Core paths and this port never loads
them.

So on this path SuperTux is a **gl1** title, closer to Neverball — which already runs end to end
— than to Craft. Every GLSL question is moot here, and what is left is one feature:
[docs/PORTING.md](docs/PORTING.md) has the framebuffer gap, the cheaper alternative oops-gl
already supports, and the four things nobody has measured yet.

`make` builds and runs that check and nothing else. The Makefile names the pieces and is
deliberately not armed: a title whose default target fails is one CI has to special-case and a
reader learns to ignore. That is the shape `craft/Makefile` and `extreme-tux-racer/Makefile`
already use.

## What is in the way

Four things, none of them OpenGL, and the first is not this title's to pay:

| | |
|---|---|
| **The C++ runtime** | exceptions, RTTI, static-initialisation guards. A shared cost the collection assigns to Extreme Tux Racer first |
| **Six submodules** | `common/upstream-fetch.sh` does not recurse, and four of the six are non-optional. The first origin here that needs them |
| **OpenAL** | `src/oops-deps/` has SDL2\_mixer, libogg and libvorbis, and no OpenAL |
| **Squirrel and PhysFS** | never compiled for this target |

## Docs

- [Porting notes](docs/PORTING.md) — the renderer reading, the framebuffer gap, and what is not measured
- [Shim notes](shim/README.md) — empty, with the one line that is not optional
- [oops-titles overview](../README.md)
- [oops-apps catalog & guide](../../../docs/USER_GUIDE.md)

## Upstream

[SuperTux/supertux](https://github.com/SuperTux/supertux), GPL-3.0 licensed.
