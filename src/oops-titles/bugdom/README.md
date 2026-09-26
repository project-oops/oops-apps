# Bugdom

<p align="center">
  <img src="assets/icon0.png" alt="Bugdom" width="200">
</p>

Pangea's garden adventure, ported through oops-sdk.

## About

Bugdom is a 1999 Macintosh game, released as freeware and ported to modern systems by jorio. It
draws through immediate-mode OpenGL on SDL2, and rests on jorio's Pomme for its Mac OS toolbox
layer.

- Pinned by commit hash to the newest release tag, a lightweight tag.
- Fetched rather than vendored: `make` pulls upstream on demand.
- Brings its own game data under `Data/`, so nothing is required from the player.
- `extern/Pomme` is a submodule, so the lock carries `UPSTREAM_SUBMODULES=1`.

## Building

```
make            # the payload
make package    # the title package with the game data, which is what runs
make census     # compile every source for the target and report, without linking
```

`make census` compiles with `-fsyntax-only` and does not link. A link is where a missing GL entry
point or an unresolved symbol appears, so a clean census is not a working payload.

## Notes

`OOPS_RENDERER` is `gl1`, and the port calls GL by symbol rather than through a loader, so a missing
entry point is a link error.

Pomme uses libc++'s `<filesystem>` rather than the `ghc::filesystem` it bundles, which costs one
patch and removes a dependency on POSIX facilities this platform does not have.

[`docs/PORTING.md`](docs/PORTING.md) covers the structure, the SDL2 pin, the four C++ constraints a
title on this platform has to meet, and how the entry point finds the data.
