# Spaghetti Kart

Harbour Masters' native port of *Mario Kart 64* -
[upstream](https://github.com/HarbourMasters/SpaghettiKart), pinned at `1.0.0`.

It is the second title from the `libultraship` family. Read
[`../ship-of-harkinian/README.md`](../ship-of-harkinian/README.md) first - the renderer, the
shader dialect, the dependency stack and the run-time ROM property are the same, and the
reasoning is written out there.

## libultraship

`libultraship` is Harbour Masters' shared runtime, and the ports built on it make no direct GL
calls - the graphics are entirely inside the library:

| port | game |
|---|---|
| Shipwright | Ocarina of Time |
| 2ship2harkinian | Majora's Mask |
| **SpaghettiKart** | **Mario Kart 64** |
| Starship | Star Fox 64 |
| PaperBoat | Paper Mario 64 |
| Ghostship | - |

So the platform work is shared, and a second port is a Makefile, an `app.env`, an icon and a
shim.

Its submodules are `libultraship`, `torch` and a documentation theme. It takes `torch` where Ship
of Harkinian at `9.2.3` takes the older `ZAPDTR` + `OTRExporter` pair.

## Run time, not build time

Like Ship of Harkinian and unlike [`../sm64`](../sm64/README.md), it builds with no ROM.

The game reads its assets from two archives beside the payload: `spaghetti.o2r`, the port's own,
which ships with the build, and `mk64.o2r`, converted from a Mario Kart 64 ROM. The conversion is
`torch`, a host tool with its own dependency set - yaml-cpp, GSL, its own N64 graphics and binary
readers - and it runs once, on a desktop, to produce a file. Nothing in a frame reaches it, so it
is not in the payload: `shim/GameExtractor.cpp` replaces upstream's `src/port/GameExtractor.cpp`
and names the archive and where it goes, on screen and in the log, when it is absent.

`shim/include/Companion.h` is what lets that work without `torch` on the include path at all -
`src/port/GameExtractor.h` opens with it for one namespace alias.
