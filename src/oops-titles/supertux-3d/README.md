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

There is nothing here to compile against `oops-sdk` — this title is data for an engine. Godot on this
target is the work, and it is **a platform directory, not an engine rewrite**: Godot keeps everything
platform-specific in `platform/<name>/`, which is 10,609 lines for the whole Linux/BSD layer and 586
for the headless one, against roughly 470,000 lines of engine reused untouched.

Godot does not use SDL for windowing, so a `platform/oops` binds straight to `oops_display_*`,
`oops_input_*` and `oops_audio_*` — one layer fewer than the SDL titles here need, and no SDL3
problem at all.

**Sequenced behind `oops-mesa` rather than declined.** Godot 4's Compatibility renderer wants GL 3.3
core, which is inside `oops-mesa`'s destination of 4.6 — so waiting costs nothing and buys the current
Godot catalogue instead of the legacy one.

[`docs/PORTING.md`](docs/PORTING.md) has the measurements, the GLES2 census for the legacy path, and
the one unknown left: whether Godot's SCons build cross-compiles to this target.
