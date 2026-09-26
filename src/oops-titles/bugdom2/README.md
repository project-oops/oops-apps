# Bugdom 2

<p align="center">
  <img src="assets/icon0.png" alt="Bugdom 2" width="200">
</p>

Pangea's sequel, ported through oops-sdk.

## About

Bugdom 2 is a 2002 Macintosh game, released as freeware and ported to modern systems by jorio. It
draws through immediate-mode OpenGL on SDL2, and rests on jorio's Pomme for its Mac OS toolbox layer.

- Pinned by commit hash to upstream's only release tag, a lightweight tag.
- Fetched rather than vendored: `make` pulls upstream on demand.
- Brings its own game data under `Data/`, so nothing is required from the player.
- `extern/Pomme` is a submodule, so the lock carries `UPSTREAM_SUBMODULES=1`.

## Building

```
make            # the payload
make package    # the title package with the game data, which is what runs
make census     # compile every source for the target and report, without linking
make glsurface  # whether oops-gl answers the entry points this game loads by name
```

## Notes

This port shares its shape with [Bugdom](../bugdom): same author, same engine lineage, the same
Pomme, the same SDL2, the same freeware data arrangement.
[`../bugdom/docs/PORTING.md`](../bugdom/docs/PORTING.md) holds the reasoning common to both.

Where it differs: `POMME_NO_QD3D` is set, because this game brought its own `Source/3D`; the camera
is `gluLookAt`/`gluPerspective`, so `OOPS_FEATURES` names `glu`; and two GL entry points are resolved
by name through `SDL_GL_GetProcAddress`, which `make glsurface` checks.

[`docs/PORTING.md`](docs/PORTING.md) covers those differences and the entry point.
