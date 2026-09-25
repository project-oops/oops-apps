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

So there is nothing here to compile against `oops-sdk`. A port of this title is a port of Godot,
with this repository as its first payload.

## That is a different kind of job from every other title here

Every other port in `src/oops-titles` is a program that draws through GL and reads files. This one
is a *project file* for an engine. The work splits in two and only the second half is small:

1. **Godot 3.x on this target.** An engine of roughly two million lines of C++ including its
   bundled third-party code, with its own platform abstraction.
2. **This title on that engine.** Once the engine runs, a GDScript project is data - the engine
   loads it. There is nothing to port in step 2.

## Why Godot 3 is a more interesting question than Godot 4 would be

The version matters and it is easy to get backwards. **Godot 4's** renderers are Forward+ and Mobile
(both Vulkan) and Compatibility (OpenGL ES 3.0 / GL 3.3 core). **Godot 3.x** keeps a `drivers/gles2/`
renderer — GL ES 2.0, shader-based but ES2-level.

`oops-gl` has a GL 2.0 path (`OOPS_RENDERER = gl2`) that SuperTux's 2D port and Craft already
target. GLES2 and desktop GL 2.0 are close relatives, not the same thing - ES2 has no fixed
function, different precision qualifiers, a narrower texture-format set - but that gap is the kind
`oops-gl` has closed before, and it is far narrower than GL 3.3 core would be.

That is the case for looking at Godot 3 rather than dismissing the engine outright. It is not a
case for doing it: see [`../README.md`](../README.md) and the survey notes, and the argument written
up alongside this scaffold.

## Godot is designed to be ported, which cuts both ways

`platform/<name>/` is the engine's own extension point: an OS implementation, a display/window
driver, an audio driver and a main entry, per platform. Console ports exist and are built exactly
this way. So the shape of the work is known and the engine does not have to be fought.

Against that: it is still an engine port. Everything this collection has learned about one title -
a shim, a patch, a source list - does not transfer, because the thing being ported is not a title.
The honest comparison is not "Godot versus Bugdom" but "Godot versus `oops-mesa`", which is the
other large engine-shaped investment already underway.

## Status

**Scaffolded, not queued.** The lock and this note exist so the decision is recorded next to the
other candidates from the same survey rather than living in a chat log. Nothing should be spent here
until the Godot question is settled on its own terms.
