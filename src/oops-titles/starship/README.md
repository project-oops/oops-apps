# Starship

<p align="center">
  <img src="assets/icon0.png" alt="Starship" width="200">
</p>

Star Fox 64 as a native port, playing from your own copy of the game.

## About

Starship is Harbour Masters' port of Star Fox 64, built from the decompilation onto
`libultraship` - the runtime [Spaghetti Kart](../spaghetti-kart/) runs on, with the same ROM
converter, Torch.

- **Pinned to a finished release** - `v2.0.0`, by commit hash, with `libultraship` and Torch at
  the revisions that commit records.
- **Fetched, not vendored** - the upstream sources are pulled on demand via `upstream.lock`;
  only the port's own `shim/` and `patches/` live here.

## Your ROM

The build carries no game assets. Copy a Star Fox 64 ROM (US 1.0 or 1.1, `.z64`) beside
`eboot.bin`:

```
/data/homebrew/STRF00001/sf64.z64
```

The first start converts it to `sf64.o2r` on the console, with Torch. Without the ROM the title
says exactly this on screen and stops.

## Building

`make` prints the survey: the pin, the dependencies still to vendor, and what the player
supplies. `make STRF_ARMED=1 title` builds the payload.

## Docs

- [oops-titles overview](../README.md)
- [oops-apps catalog and guide](../../../docs/USER_GUIDE.md)
