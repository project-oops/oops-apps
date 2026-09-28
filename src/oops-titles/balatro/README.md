# Balatro

<p align="center">
  <img src="assets/icon0.png" alt="LÖVE" width="200">
</p>

Balatro on the LÖVE engine, playing the game from your own copy.

## About

Balatro is written for LÖVE, an open-source Lua game framework, and this title is LÖVE 11.5
ported to the hardware. Nothing of Balatro is fetched or built: the engine is the port, so every
other LÖVE 11 game runs on the same payload given a different archive.

- **Pinned to a finished release** - `11.5`, the LÖVE version Balatro ships with, by commit
  hash.
- **Fetched, not vendored** - the upstream sources are pulled on demand via `upstream.lock`;
  only the port's own `shim/` and `patches/` live here.
- **LuaJIT without the JIT** - the interpreter needs no executable memory, and the games are
  written against LuaJIT's libraries.

## Your game

The build carries no game. Copy `Balatro.exe` from your Balatro install beside `eboot.bin`:

```
/data/homebrew/BLTR00001/Balatro.exe
```

It is read, not run: a LÖVE executable carries its game as a zip archive at the end, and LÖVE
mounts that. Without it the title says exactly this on screen and stops.

## Building

`make` prints the survey: the pin, the dependencies still to vendor, and what the player
supplies. `make BLTR_ARMED=1 title` builds the payload.

## Docs

- [oops-titles overview](../README.md)
- [oops-apps catalog and guide](../../../docs/USER_GUIDE.md)
