# Craft

<p align="center">
  <img src="../../../common/assets/no-logo.svg" alt="No logo yet" width="200">
</p>

A voxel world to dig, build and fly through, ported to the hardware.

## About

Craft is a small Minecraft-like game in C: an infinite procedural world, a handful of block
types, and a renderer that draws all of it through four shader pairs. It is one of the
collection's ported upstream titles, tracked against a pinned commit.

- **Pinned by commit hash** to upstream's master. `upstream.lock` says why master rather than the
  `v1.0` tag.
- **Fetched, not vendored** - the upstream sources are pulled on demand; only the port's own
  `shim/`, `tools/` and `patches/` live here. There are no patches: everything upstream needs is
  answered from outside its tree.

## Building

`make census` compiles each source under the real target flags and reports what it stops on.
`make check` runs the census and the shader check, which compiles Craft's own fragment shaders
from `upstream/shaders/` for the hardware and reports each one's words, registers, varyings,
uniforms and texture sets against their limits. `make` runs the shader check alone. The
payload's source list is in the Makefile. [docs/PORTING.md](docs/PORTING.md) covers what the
shim replaces and how each platform dependency is answered.

## Controls

The shim reads a USB keyboard and mouse when one is attached and the pad always, every frame,
and whichever moved wins - there is no mode to pick.

| Pad | Does |
|---|---|
| Left stick | Walk |
| Right stick | Look |
| Cross | Jump |
| Square / Circle | Break / place |
| L1 / R1 | Cycle the held block |
| Options | Chat line - needs a keyboard to type into |

## Docs

- [Porting notes](docs/PORTING.md) - what the shim replaces and how each dependency is answered
- [oops-titles overview](../README.md)
- [oops-apps catalog and guide](../../../docs/USER_GUIDE.md)

## Upstream

[fogleman/Craft](https://github.com/fogleman/Craft), MIT licensed.
