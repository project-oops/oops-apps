# Mineclonia

<p align="center">
  <img src="assets/icon0.png" alt="Mineclonia" width="200">
</p>

A survival voxel game - gather, build, explore - on the Luanti engine, ported to the hardware.

## About

Luanti (formerly Minetest) is a voxel engine in C++ scripted in Lua; Mineclonia is a game
written for it. Both are free, so the package is complete and the player supplies nothing. A
separate title from [Craft](../craft/), which is its own small engine.

- **Two origins** - the engine in `upstream.lock` (`5.17.0`) and the game in
  `upstream-game.lock` (`0.123.1`), each a release pinned by commit hash.
- **Fetched, not vendored** - only the port's own `shim/` and `patches/` live here.
- **Lua 5.1, bundled** - the engine carries its own Lua and `bitop`, so there is no LuaJIT and
  nothing needs executable memory.

## Building

`make` prints the survey: the pins and the dependencies still to vendor.
`make MNCL_ARMED=1 title` builds the payload.

## Docs

- [oops-titles overview](../README.md)
- [oops-apps catalog and guide](../../../docs/USER_GUIDE.md)
