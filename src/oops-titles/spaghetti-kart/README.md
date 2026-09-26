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

Like Ship of Harkinian and unlike [`../sm64`](../sm64/README.md), it builds with no ROM and
converts the player's own copy on the device.
