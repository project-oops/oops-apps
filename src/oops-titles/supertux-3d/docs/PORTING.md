# Porting SuperTux 3D

## What this repository is

22 GDScript files, their scenes, and 728 MB of assets. **No engine source at all.**

| | |
|---|---|
| engine | Godot, `config_version=4` in `project.godot` → **Godot 3.x**, not Godot 4 |
| game code | 22 `.gd` scripts — GDScript, interpreted by the engine |
| C / C++ in repo | **none** |
| upstream tags | none; the newest commit is 2020-04-27 |
| platforms upstream ships | Linux only |

So this title is not the work. Godot on this target is the work, and this repository is then data the
engine loads.

## Godot support is a platform directory, not an engine rewrite

This document said the opposite in its first version, and the measurement says otherwise. Godot
isolates everything platform-specific into `platform/<name>/`, and that is the only part a new
target replaces:

| Godot 3.7 | lines |
|---|---|
| `core` | 110,083 |
| `scene` | 208,996 |
| `servers` | 83,635 |
| `drivers` | 65,453 |
| **reused untouched** | **~470,000** |
| `platform/x11` — the whole Linux/BSD layer | **10,609** |
| `platform/server` — the headless layer, i.e. the floor | **586** |

A few thousand lines against most of half a million. That is the order of a large title port —
`libultraship` is 138 sources — rather than the order of `oops-mesa`.

## Why `scons platform=x11` is not the answer, and why that is fine

The x11 layer binds straight to the host: `X11/Xlib.h`, `GL/glx.h`, `alsa/asoundlib.h`,
`pulse/pulseaudio.h`, `libudev.h`, `dlfcn.h`. None of those exist here.

But **Godot does not use SDL for windowing** — it implements display, input and audio per platform.
So a `platform/oops` binds directly to `oops_display_*`, `oops_input_*` and `oops_audio_*`, which is
one layer *fewer* than every SDL title in this directory needs, and sidesteps the SDL3 problem that
blocks Bugdom entirely. `platform/server` at 586 lines is the skeleton to start from: add display,
input and audio to a headless target.

## Target Godot 4, not 3.x

The renderer question mostly dissolves once `oops-mesa` is accounted for. Godot 4's Compatibility
renderer wants **GL 3.3 core**, comfortably inside `oops-mesa`'s destination of 4.6. So the engine
work reaches the *current* Godot catalogue rather than the legacy one, and there is no reason to aim
at 3.x.

For the record, the legacy path is also close. Censused against `oops-gl`, Godot 3.7's `drivers/gles2`
names 126 GL entry points and 93 are already defined. Of the 33 gaps, **27 are header-only
declarations** in Godot's own GL glue that nothing calls. Of the 6 real call sites, three are
`ARB_debug_output` (guarded by `GLAD_GL_ARB_debug_output` and a verbose-stdout check), one is
Apple-only, one is multisample FBO. **The single real gap is `glBindVertexArray`** — the same vertex
array object work ImGui already wants for Ship of Harkinian.

## What has not been measured

**Whether Godot's SCons build cross-compiles to `x86_64-unknown-freebsd`** with this collection's
clang and `oops-deps/libcxx`. That is the one remaining unknown that could be expensive, and it is
cheap to answer. Godot disables exceptions and RTTI by default, which is favourable.

## Status

**Scaffolded, and sequenced behind `oops-mesa` rather than declined.** Waiting costs nothing and buys
the better target: when `oops-mesa` serves GL 3.3, Godot 4's Compatibility renderer is in range and
this becomes a platform directory plus a build. Nothing should be spent here before then.
