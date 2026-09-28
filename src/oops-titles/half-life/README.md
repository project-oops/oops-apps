# Half-Life

<p align="center">
  <img src="assets/icon0.png" alt="Xash3D FWGS" width="200">
</p>

Half-Life on Xash3D FWGS, playing the data from your own copy of the game.

## About

Xash3D FWGS is a GoldSrc-compatible engine in C; hlsdk-portable is the Half-Life SDK's client
and server made portable. Both are linked into one payload under the engine's own
`XASH_STATIC_LIBS` mode - the one its Vita and Switch builds use - so nothing is loaded from a
library at run time.

- **Two origins** - the engine in `upstream.lock`, the game code in `upstream-hlsdk.lock`, each
  pinned by commit hash. Neither project cuts releases, so each pin is a dated commit on its
  default branch.
- **Fetched, not vendored** - only the port's own `shim/` and `patches/` live here.
- **One library per component, as upstream's static build makes them** - the filesystem, the
  renderer, the menu, the client and the server are each linked into a relocatable object whose
  only global symbol is its export table, so the client and the server can each carry the
  shared player-movement code without colliding. The client's and server's tables list every
  function they define, which is how save games find entity callbacks by name.

## Your game data

The build carries no game data. Copy the `valve` folder from your Half-Life install beside
`eboot.bin`:

```
/data/homebrew/HALF00001/valve/
```

Without it the title says exactly this on screen and stops.

## Building

`make title` builds the payload. `make survey` prints the pins, the dependencies and what the
player supplies; `make census` compiles every component's sources and names any that fail.

## Docs

- [oops-titles overview](../README.md)
- [oops-apps catalog and guide](../../../docs/USER_GUIDE.md)
