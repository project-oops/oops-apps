# oops-apps

The homebrew this collection builds on its own SDK, in one place.

A target-side app that runs on the hardware is a payload that opens the display, reads the
controller, makes some sound and talks to the network - the same handful of subsystems every
time. [oops-sdk](https://github.com/project-oops/oops-sdk) is the layer that answers those
calls; this is where the programs that use it live.

> **Status: a growing set of apps.** Porthole - the target half of the capture-and-input path
> whose host half lives in [Prosperous](https://github.com/project-oops/Prosperous) - is the
> largest, and the one the repository was made for; beside it are smaller apps that each build
> on the SDK. The point of a shared repository is that the next one starts from a working
> example rather than a blank directory.

## What belongs here

**An app built on the SDK.** Something a person runs on the hardware for its own sake - a
tool, a demo, a service. The admission rule is that narrow on purpose, the same way
[oops-libs](https://github.com/project-oops/oops-libs) is narrow: a repository that will take
anything becomes a place things are dropped rather than a place they belong.

**obSCEne is not here, and that is the line that matters.**
[obSCEne](https://github.com/project-oops/obSCEne) is a conformance *probe*: it exists to
**measure** what the platform does, one call at a time, and report it. An app here **does**
something. If a thing's purpose is to find out whether a function works, it is a probe and it
belongs in obSCEne; if its purpose is to put a picture on the screen or a stream on the wire,
it is an app and it belongs here. Blurring that puts two homes under one roof and is how a
measurement ends up shipped as a feature.

## Applications

- **home**: Unified homebrew shell reimplementation for Prospero mimicking the native system UI (PS button overlay, title launcher, settings, media player, storage/save manager, notifications).
- **wipeout**: Native clean-room WipEout engine port for Prospero / Trinity using AGC hardware compute tiler, PCM audio streaming, and DualSense input.
- **porthole**: Background daemon payload exposing the console's unencrypted remote play stream on TCP port 9805 and accepting controller input injection on TCP port 9806.
- **pltauth-patch**: Kernel DMAP patcher that bypasses SceShellCore entitlement checks for native Big Apps (`category 0` / direct HDMI scanout / LibAgc GPU).
- **gallery**: Visual showcase and capability inspector across display, draw, input, system, audio, net, and media decoding.
- **pad-viz**: DualSense/DualShock controller telemetry visualizer (live analog stick drift bounds, triggers, touch-pad, 6-axis IMU tilt, and rumble/lightbar test).
- **net-tool**: Network configuration, interface diagnostics, and UDP status echo server.
- **injector**: Dedicated process payload injector.

> **Subsystem Unification:** Media viewing, notification overlays, save management, shell skinning, and system diagnostics are unified directly inside **`home`** as a complete Prospero shell reimplementation.

## Layout

```
oops-apps/
├── bin/oops-apps       # build and check any app, the collection's one entry-point shape
├── common/
│   └── app.mk          # shared application Makefile helper with app.env support
├── src/                # each app in its own directory
│   ├── home/           # unified Prospero shell reimplementation
│   ├── wipeout/        # native clean-room 3D WipEout port
│   ├── porthole/       # remote play daemon
│   └── ...             # the other apps, all the same shape
└── docs/
```

**Canonical base layer lives in oops-sdk.** The freestanding runtime (`oops/freestd.h`),
the syscall interface (`oops/syscall.h`) and kernel read/write (`oops/krw.h`) an app needs
are provided by `oops-sdk`, consumed via source inclusion (`oops-sdk.mk`). There is no local
duplicate base layer; see [docs/decisions/D002-the-base-layer-is-on-loan-from-obscene.md](docs/decisions/D002-the-base-layer-is-on-loan-from-obscene.md).

## Building an app

Each app lives under `src/<name>`, carries a minimal `Makefile` and an optional `app.env` configuration file, reaching the shared build helper and SDK by sibling layout:

```bash
./bin/oops-apps build          # every app, or: ./bin/oops-apps build wipeout
./bin/oops-apps list           # what is here
./bin/oops-apps check          # run each app's host selftest
```

An app's `Makefile` defines its host test and target payload sources, then includes the shared helper:

```makefile
OOPS_APPS_ROOT ?= $(abspath ../..)
HOST_TEST_SRCS = myapp_selftest.c
PAYLOAD_SRCS   = myapp.c $(OOPS_SDK_DIR)/src/system/freestd.c
include $(OOPS_APPS_ROOT)/common/app.mk
```

**It cross-compiles for the target, so build under WSL on Windows** - the toolchain targets a
FreeBSD-derived system and the Windows shell has no `clang` on its path. This is the same
constraint oops-sdk and obSCEne build under.

## Shipping an app & `app.env`

An app declares its identity, title metadata, and release formats in an `app.env` file alongside its `Makefile`:

```properties
APP_NAME=wipeout
TITLE_ID=WIPE00001
TITLE_NAME="WipEout"
TITLE_CATEGORY=big-app
TITLE_VERSION=01.00
# Supported formats: elf, eboot, title (or combinations)
FORMATS=title eboot
```

Running `make dist` (or `./bin/oops-apps dist`) builds the requested format(s) under `dist/`, following the four-axis naming conventions in
[CONVENTIONS.md](https://github.com/project-oops/OOPS/blob/main/docs/CONVENTIONS.md#the-four-axes-of-a-build-and-a-run):
- `elf`: stages plain target executable `myapp-<target>.elf`
- `eboot`: invokes `selfish` to produce signed executable container `myapp-eboot-<target>.bin`
- `title`: invokes `selfish` to package a native PS5 title directory into `myapp-title-<target>.zip`

Every artifact encodes the target generation (`$(TARGET)`, default: `prospero`). The shared SDK helper `oops_verify_dist` enforces this at the Makefile level and fails if an un-generation-tagged file or an unzipped folder is staged.

`./bin/oops-apps dist` stages every app that has a `dist` target and says so for every app
that does not - an app still in development ships nothing, out loud, rather than an empty
release. On a push to `main`, CI validates and publishes each app's artifacts to the rolling
**`latest-main`** prerelease. A new app appears there the day its Makefile grows a conforming
`dist` target, with nothing to add to the workflow.

## Downloads

Built artifacts for every app are on the **[latest-main
release](https://github.com/project-oops/oops-apps/releases/tag/latest-main)**, rebuilt on
every push to `main`, each file prefixed with the app it came from. The documentation is at
**[project-oops.github.io/oops-apps](https://project-oops.github.io/oops-apps/)**.

## A nursery, and leaving it

An app that grows into a product of its own should be able to move out to its own repository
without a rewrite. So an app keeps to its own directory - its sources, its `sce_sys/` assets,
its `Makefile` - and links against oops-sdk. Nothing
here is structured so that a subdirectory cannot be lifted out cleanly the day it earns its
own home.

## Licence

Dual-licensed under [MIT](LICENSE-MIT) or [Apache-2.0](LICENSE-APACHE), at your option.

## Where it sits

Not one of the four. **OOPS** is Orbistoun, obSCEne, Prosperous and SELFish - four projects
aimed at one console's operating system. This sits beside
[oops-sdk](https://github.com/project-oops/oops-sdk) as infrastructure underneath them: a
repository rather than a project. Where oops-libs is the Rust the host-side tools share and
oops-sdk is the C the payloads are built from, oops-apps is the payloads themselves.

Shared rules - provenance, naming, decision logs, honest failure, gates - live in
[the OOPS conventions](https://github.com/project-oops/OOPS/blob/main/docs/CONVENTIONS.md) and
are not restated here.
