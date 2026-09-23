# oops-apps documentation

The homebrew this collection builds on [oops-sdk](https://github.com/project-oops/oops-sdk):
programs that run on the hardware and use the SDK's subsystems. Not one of the four - a
repository rather than a project, beside oops-libs and oops-sdk.

New here? The [root README](../README.md) has the admission rule, the apps-versus-probes line
that keeps obSCEne separate, and how an app builds.

## The apps

Apps are filed by **category**, named after the part of the collection each one exercises.
`oops-gl` and `oops-mesa` are named after the implementations rather than called "opengl" and
"mesa", because Mesa *is* OpenGL and the two as siblings would confuse somebody later.

### `oops-gl` - what oops-gl can do

- **[gl1-cube](../src/oops-gl/gl1-cube/)** - clean-room OpenGL 1.x 3D rotating-cube demo, and
  the **pinned hardware oracle**: its frame is measured on a retail console and asserted
  register by register by oops-sdk.
- **[gl1-probe](../src/oops-gl/gl1-probe/)** - the breadth half: fifteen checks that each drive
  one GL 1.x feature and read the pixels back to decide. Runs the same suite on the host
  software rasteriser and on the console, so a difference between them is a hardware-path bug.
- **[gl2-cube](../src/oops-gl/gl2-cube/)** - the programmable-pipeline (OpenGL 2.0 / GLSL) cube,
  the shader counterpart to `gl1-cube`.
- **[gl2-probe](../src/oops-gl/gl2-probe/)** - the breadth half for the GL 2.0 shader path.

### `oops-mesa` - what oops-mesa can do

- **[mesa-winsys-probe](../src/oops-mesa/mesa-winsys-probe/)** - OpenGL-through-Mesa bring-up app
  (hosted; `OOPS_RENDERER = mesa`). The winsys path, and the control for `mesa-dri-probe`.
- **[mesa-dri-probe](../src/oops-mesa/mesa-dri-probe/)** - the same stack through the Gallium DRI frontend,
  ending in a full-frame hash.
- **[mesa-cube](../src/oops-mesa/mesa-cube/)** - the example title: a textured, depth-tested cube
  presenting every frame.

### `oops-frameworks` - middleware ported to the console

Apps that demonstrate a framework running on this target, rather than a renderer directly. They
link a renderer underneath (oops-gl today) but the framework is the point.

- **[sdl-probe](../src/oops-frameworks/sdl-probe/)** - upstream SDL2 on the console through
  `oops-sdl`: init, a window and GL context, the event pump, and the controller.
- **[glut-demo](../src/oops-frameworks/glut-demo/)** - an ordinary GLUT program built for the
  console by compiling it against the SDK's `<GL/glut.h>`.

### `oops-payloads` - things that run inside another process

- **[porthole](../src/oops-payloads/porthole/README.md)** - the target half of the
  capture-and-input path; its host half is [Prosperous](../../prosperous).
- **[tracer](../src/oops-payloads/tracer/README.md)** - in-process API and GPU command/shader
  telemetry tracer for real titles.
- **[sandbox-daemon](../src/oops-payloads/sandbox-daemon/README.md)** - on-demand
  filesystem-namespace unsandboxing daemon over loopback IPC.
- **[pltauth-patch](../src/oops-payloads/pltauth-patch/)** - kernel DMAP patcher for
  SceShellCore entitlement checks.
- **[injector](../src/oops-payloads/injector/)** - standalone process payload injector.

### `oops-utilities` - tools and shells

- **[seashell](../src/oops-utilities/seashell/README.md)** - SeaShell, the unified Prospero homebrew
  shell, providing an authentic console UI (PS button overlay, title launcher, settings, media
  player, storage/save manager, notifications).
- **[gallery](../src/oops-utilities/gallery/README.md)** - visual showcase and capability
  inspector across display, draw, input, audio, net, and media decoding.
- **[pad-viz](../src/oops-utilities/pad-viz/README.md)** - DualSense/DualShock controller
  telemetry visualizer.
- **[net-tool](../src/oops-utilities/net-tool/README.md)** - network configuration and interface
  diagnostics.
- **[cxx-throw](../src/oops-utilities/cxx-throw/)** - C++ exception-handling probe: throw and catch
  across the runtime, to prove the unwinder works on the target.

### `oops-titles` - real programs, ported

Empty so far. The targets, and what was actually verified about each, are in
[`src/oops-titles/README.md`](../src/oops-titles/README.md).

**Some entries link to a directory rather than a README, because that app has not written one
yet** - the whole of `oops-gl` and `oops-mesa`, plus `injector` and `pltauth-patch`. Writing
them is worth doing; linking to a file that was never written is not the way to record that it
is missing.

## Project memory

- [DECISIONS.md](DECISIONS.md) - a generated index over `decisions/`, one file per entry.
  Why the repository exists, why obSCEne is exempt, and how the base layer was
  promoted from obSCEne into oops-sdk (closing D002).

## The words

Vocabulary is the collection's, not this repository's:

- [the collection's glossary](https://github.com/project-oops/OOPS/blob/main/docs/GLOSSARY.md) - standard ELF, `DT_`/`PT_`, and the cross-repository word collisions
- [oops-sdk](https://github.com/project-oops/oops-sdk) - the subsystems an app calls
- [SELFish](https://github.com/project-oops/SELFish/blob/main/docs/GLOSSARY.md) - payload, title, package, the generation split

**payload** and **target** are defined for all repositories in
[CONVENTIONS.md section 2](https://github.com/project-oops/OOPS/blob/main/docs/CONVENTIONS.md#the-words-for-our-own-layers).
An app here is a **payload** built for the **target**.

Shared rules - provenance, naming, decision logs, honest failure, gates - are in
[the OOPS conventions](https://github.com/project-oops/OOPS/blob/main/docs/CONVENTIONS.md) and
not restated here.
