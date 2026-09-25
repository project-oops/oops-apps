# SuperTux 3D

<p align="center">
  <img src="../../../common/assets/no-logo.svg" alt="No logo yet" width="200">
</p>

A Godot platformer — **scaffolded, not queued.**

## About

SuperTux 3D is a fan-made 3D platformer with gravity-flipping physics. This repository is 22
GDScript files, their scenes and 728 MB of assets, and **no engine source at all**.

- **Pinned by commit hash** — `5d409c08`. **No `UPSTREAM_REF`:** upstream has no tags, and naming a
  branch would read as a pin while moving.
- **Godot 3.x**, not Godot 4 — `project.godot` says `config_version=4`.

## Why it is scaffolded rather than queued

There is nothing here to compile against `oops-sdk`. Porting this title means porting Godot, with
this repository as its first payload — an engine job rather than a title one, and the honest
comparison is with `oops-mesa` rather than with the other titles in this directory.

The version is the interesting part, and it is easy to get backwards. Godot 4 renders through Vulkan
or a GL 3.3-core Compatibility path. **Godot 3.x keeps a `drivers/gles2/` renderer**, and `oops-gl`
already has a GL 2.0 path that Craft and SuperTux's 2D port target. That gap is narrow enough to be
worth an argument — which is why the scaffold exists instead of a deletion.

[`docs/PORTING.md`](docs/PORTING.md) has the split, the renderer comparison, and why Godot's own
`platform/<name>/` layout cuts both ways.
