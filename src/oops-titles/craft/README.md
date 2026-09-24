# Craft

<p align="center">
  <img src="../../../common/assets/no-logo.svg" alt="No logo yet" width="200">
</p>

A voxel world you can dig, build and fly through — ported to the console.

## About

Craft is a small Minecraft-like game in about 5,000 lines of C: an infinite procedural world, a
handful of block types, and a renderer that draws all of it through four shader pairs. It is one
of the collection's ported upstream titles, tracked against a pinned commit.

- **Pinned by commit hash** — `d6888a6e`, upstream's master. `upstream.lock` says why master
  rather than the `v1.0` tag, which is the opposite of what Neverball chose.
- **Fetched, not vendored** — the upstream sources are pulled on demand; only the port's own
  `shim/`, `tools/` and `patches/` live here. There are no patches: everything upstream needs is
  answered from outside its tree.

## Status

**The graphics are done and the storage is not.** Of Craft's twelve sources, four vendored
libraries and two shim sources, **14 compile and 4 do not** - and one of the four, `auth.c`, is
excluded on purpose rather than failed. The other three name a header each, and
[docs/PORTING.md](docs/PORTING.md) has what is behind every one.

Those numbers come from `make census`, which compiles each source under the real target flags and
reports what it died on, because the ones written here by hand had drifted.

What is settled is the part that was actually in question:

```
$ make check
craft shadercheck: upstream/shaders
  block    pass  229 words   56/136 regs   7/16 varying  25/32 uniform  2/2 tex
  line     pass   27 words   20/136 regs   0/16 varying  16/32 uniform  0/2 tex
  sky      pass   41 words   21/136 regs   2/16 varying  18/32 uniform  1/2 tex
  text     pass   76 words   29/136 regs   2/16 varying  18/32 uniform  1/2 tex
craft shadercheck: 4/4 shader pairs compile for the console
```

oops-gl compiles a GL 2.0 fragment shader to gfx1030 instructions or **refuses it by name**, and
a refusal means the draw fails rather than falling back — so whether Craft's own shaders compile
decided whether this title could render at all. They do, with room on every limit except texture
sets, where `block` uses both.

`make check` runs that and the source census beside it, and `make` runs the shader check alone.
The payload's source list is in the Makefile, named and deliberately not armed: a title whose
default target fails is one CI has to special-case and a reader learns to ignore.

## Controls

The shim reads a USB keyboard and mouse when one is attached and the pad always, every frame,
and whichever moved wins — there is no mode to pick.

| Pad | Does |
|---|---|
| Left stick | Walk |
| Right stick | Look |
| Cross | Jump |
| Square / Circle | Break / place |
| L1 / R1 | Cycle the held block |
| Options | Chat line — needs a keyboard to type into |

## Docs

- [Porting notes](docs/PORTING.md) — what the shim replaces, what is left, and the numbers
- [oops-titles overview](../README.md)
- [oops-apps catalog & guide](../../../docs/USER_GUIDE.md)

## Upstream

[fogleman/Craft](https://github.com/fogleman/Craft), MIT licensed.
