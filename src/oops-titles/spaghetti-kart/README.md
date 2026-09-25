# Spaghetti Kart

Harbour Masters' native port of *Mario Kart 64* —
[upstream](https://github.com/HarbourMasters/SpaghettiKart), pinned at `1.0.0` (2026-02-25).

**This is the second title out of the `libultraship` family and it exists to measure how much
cheaper the second one is.** Read [`../ship-of-harkinian/README.md`](../ship-of-harkinian/README.md)
first — the renderer, the shader dialect, the dependency stack and the run-time ROM property are
all the same, and the reasoning is written out there rather than repeated here.

## Why a second one now

`libultraship` is not this port's renderer or Ship of Harkinian's. It is Harbour Masters' shared
runtime, and on 2026-09-25 every port built on it measured **zero direct GL calls** — the
graphics are entirely inside the library:

| port | game |
|---|---|
| Shipwright | Ocarina of Time |
| 2ship2harkinian | Majora's Mask |
| **SpaghettiKart** | **Mario Kart 64** |
| Starship | Star Fox 64 |
| PaperBoat | Paper Mario 64 |
| Ghostship | — |

So the platform work is done once. If that is true, this title should cost roughly what Neverputt
cost after Neverball — a Makefile, an app.env, an icon and a shim — and if it does not, the
difference is the thing worth writing down.

Its submodules are `libultraship`, `torch` and a documentation theme. Note that it takes `torch`
where Ship of Harkinian at `9.2.3` still takes the older `ZAPDTR` + `OTRExporter` pair; the asset
pipeline moved between those two points, so this is also the first look at the newer one.

## Run time, not build time

Like Ship of Harkinian and unlike [`../sm64`](../sm64/README.md), it builds with no ROM anywhere
and converts the player's own copy on the device. That is the property that put both of these
ahead of the other candidates surveyed.

## State

Pin recorded, nothing fetched or built. It is deliberately behind Ship of Harkinian: the nine
dependencies and the `#version 120` patch are shared work, and doing them once in one title
before starting the second is the whole argument for the family being cheap.
