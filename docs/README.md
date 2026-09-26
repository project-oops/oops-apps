# oops-apps documentation

The homebrew this collection builds on [oops-sdk](https://github.com/project-oops/oops-sdk):
programs that run on the hardware and use the SDK's subsystems. Not one of the four - a
repository rather than a project, beside oops-libs and oops-sdk.

The [root README](../README.md) has the admission rule, the apps-versus-probes line that keeps
obSCEne separate, and how an app builds.

## The apps

Apps are filed by **category**, named after the part of the collection each one exercises.
`oops-gl` and `oops-mesa` are named after the implementations rather than "opengl" and "mesa",
because Mesa is an OpenGL implementation and the two as siblings would be ambiguous.

### `oops-gl` - what oops-gl can do

- [gl1-cube](../src/oops-gl/gl1-cube/) - clean-room OpenGL 1.x 3D rotating-cube demo, and the
  **pinned hardware oracle**: its frame is measured on the hardware and asserted register by
  register by oops-sdk.
- [gl1-probe](../src/oops-gl/gl1-probe/) - the breadth half: checks that each drive one GL 1.x
  feature and read the pixels back to decide. It runs the same suite on the host software
  rasteriser and on the hardware, so a difference between them is a hardware-path bug.
- [gl2-cube](../src/oops-gl/gl2-cube/) - the programmable-pipeline (OpenGL 2.0 / GLSL) cube,
  the shader counterpart to `gl1-cube`.
- [gl2-probe](../src/oops-gl/gl2-probe/) - the breadth half for the GL 2.0 shader path.

### `oops-mesa` - what oops-mesa can do

- [mesa-winsys-probe](../src/oops-mesa/mesa-winsys-probe/) - OpenGL-through-Mesa bring-up app
  (hosted; `OOPS_RENDERER = mesa`). The winsys path, and the control for `mesa-dri-probe`.
- [mesa-dri-probe](../src/oops-mesa/mesa-dri-probe/) - the same stack through the Gallium DRI
  frontend, ending in a full-frame hash.
- [mesa-cube](../src/oops-mesa/mesa-cube/) - the example title: a textured, depth-tested cube
  presenting every frame.

### `oops-frameworks` - middleware ported to the hardware

Apps that demonstrate a framework running on this target, rather than a renderer directly. They
link a renderer underneath, but the framework is the point.

- [sdl-probe](../src/oops-frameworks/sdl-probe/) - upstream SDL2 through `oops-sdl`: init, a
  window and GL context, the event pump, and the controller.
- [glut-demo](../src/oops-frameworks/glut-demo/) - an ordinary GLUT program built by compiling it
  against the SDK's `<GL/glut.h>`.

### `oops-payloads` - things that run inside another process

- [porthole](../src/oops-payloads/porthole/README.md) - the target half of the
  capture-and-input path; its host half is [Prosperous](../../prosperous).
- [tracer](../src/oops-payloads/tracer/README.md) - in-process API and GPU command/shader
  telemetry tracer for real titles.
- [sandbox-daemon](../src/oops-payloads/sandbox-daemon/README.md) - on-demand
  filesystem-namespace unsandboxing daemon over loopback IPC.
- [pltauth-patch](../src/oops-payloads/pltauth-patch/) - kernel DMAP patcher for
  SceShellCore entitlement checks.
- [injector](../src/oops-payloads/injector/) - standalone process payload injector.

### `oops-utilities` - tools and shells

- [seashell](../src/oops-utilities/seashell/README.md) - SeaShell, the unified Prospero homebrew
  shell: system overlay, title launcher, settings, media player, storage and save manager,
  notifications.
- [gallery](../src/oops-utilities/gallery/README.md) - visual showcase and capability
  inspector across display, draw, input, audio, net, and media decoding.
- [pad-viz](../src/oops-utilities/pad-viz/README.md) - controller telemetry visualizer.
- [net-tool](../src/oops-utilities/net-tool/README.md) - network configuration and interface
  diagnostics.
- [cxx-throw](../src/oops-utilities/cxx-throw/) - C++ exception-handling probe: throw and catch
  across the runtime to prove the unwinder on the target.

### `oops-titles` - real programs, ported

Each port is a directory under [`src/oops-titles/`](../src/oops-titles/).

## Project memory

- [DECISIONS.md](DECISIONS.md) - a generated index over `decisions/`, one file per entry.

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
