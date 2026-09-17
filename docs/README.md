# oops-apps documentation

The homebrew this collection builds on [oops-sdk](https://github.com/project-oops/oops-sdk):
programs that run on the hardware and use the SDK's subsystems. Not one of the four - a
repository rather than a project, beside oops-libs and oops-sdk.

New here? The [root README](../README.md) has the admission rule, the apps-versus-probes line
that keeps obSCEne separate, and how an app builds.

## The apps

- **[seashell](../src/seashell/README.md)** - SeaShell, the unified PS5 homebrew shell, providing an authentic console UI (PS button overlay, title launcher, settings, media player, storage/save manager, notifications).
- **[wipeout](../src/wipeout/README.md)** - native clean-room WipEout racing-game engine port for Prospero / Trinity using AGC GPU compute tiler.
- **[porthole](../src/porthole/README.md)** - the target half of the capture-and-input path; its host half is [Prosperous](../../prosperous).
- **[gallery](../src/gallery/README.md)** - visual showcase and capability inspector across display, draw, input, audio, net, and media decoding.
- **[pad-viz](../src/pad-viz/README.md)** - DualSense/DualShock controller telemetry visualizer.
- **[net-tool](../src/net-tool/README.md)** - network configuration and interface diagnostics.
- **[tracer](../src/tracer/README.md)** - in-process API and GPU command/shader telemetry tracer for real titles.
- **[sandbox-daemon](../src/sandbox-daemon/README.md)** - on-demand filesystem-namespace unsandboxing daemon over loopback IPC.
- **[gl-cube](../src/gl-cube/)** - clean-room OpenGL 3D rotating-cube demo for Prospero / Trinity.
- **[mesa-probe](../src/mesa-probe/)** - OpenGL-through-Mesa bring-up app (hosted; `USE_MESA`).
- **[pltauth-patch](../src/pltauth-patch/)** - kernel DMAP patcher for SceShellCore entitlement checks.
- **[injector](../src/injector/)** - standalone process payload injector.

The last four link to their directories because none of them has a README yet - the rows above
them all point at one. Writing those is worth doing; linking to a file that was never written is
not the way to record that it is missing.

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
