# oops-apps

Native homebrew for Orbis and Prospero: demos, ported games, and utilities.

`oops-apps` holds the native homebrew for the platform: 3D graphics demos, ports of upstream
games, and system utilities, all built on [oops-sdk](../oops-sdk/). The demos written from
scratch are the known-good yardstick the rest of OOPS is measured against - the compilers, the
packaging, the hardware transport and the emulator are all exercised here first - and the ported
games put the same toolchain through third-party code.

- [User and Operator Guide](docs/USER_GUIDE.md) - building and running the demos, and using tracer.
- [Design Decisions](docs/DECISIONS.md) - numbered decisions and their reasoning.
- [The oops-apps index](https://project-oops.github.io/oops-apps/) - every app, built from
  `main`, with screenshots and downloads.

## Role in the loop

Within the [OOPS ecosystem](../docs/THE_LOOP.md), `oops-apps` is the **Known Ground-Truth Testbed**:

```
[Write Application in oops-apps (e.g. gl1-cube)]
                        │
                        ▼
┌───────────────────────────────────────────────┐
│ 1. Compile with oops-sdk (freestanding clang) │
│ 2. Package with SELFish (make title)          │
└───────────────────────┬───────────────────────┘
                        │
                        ├───────────────────────────────┐
                        ▼                               ▼
┌─────────────────────────────────────────┐   ┌───────────────────────────────┐
│ Deploy to Prospero via Prosperous       │   │ Run in Orbistoun Emulator     │
│ - pros restore ... && pros launch ...   │   │ - ./bin/orbistoun run ...     │
│ - Hardware renders on TV via HDMI       │   │ - Verifies Vulkan render loop │
└───────────────────────┬─────────────────┘   └───────────────┬───────────────┘
                        │                                     │
                        └──────────────► Match ◄──────────────┘
```

1. **Known ground truth.** Commercial games are large, opaque, and fail without clear
   diagnostics. In `oops-apps` every line of code, every vertex buffer and every expected return
   value is known.
2. **End-to-end pipeline verification.** If `gl1-cube` renders on the hardware but fails in
   [Orbistoun](../orbistoun/), the gap is isolated to the emulator.
3. **First-party tooling.** Every application builds with `app.mk`, packages with `selfish`, and
   deploys with `pros`.

## Quickstart

### Build and test

```bash
./bin/oops-apps build    # build the applications
./bin/oops-apps check    # run the host-side self-tests
./bin/oops-apps list     # list every app
```

### Build a title and run it

```bash
cd src/oops-gl/gl1-cube
make title               # package a runnable title with SELFish
```

Deploy and launch it on the hardware with [Prosperous](../prosperous/), or run it in
[Orbistoun](../orbistoun/). The steps, including
[creating an application](docs/USER_GUIDE.md#4-creating-a-new-application-in-oops-apps), are in
the [User and Operator Guide](docs/USER_GUIDE.md).

## Finding the apps

Apps live under `src/`, grouped by the part of the collection they exercise. List them with
`./bin/oops-apps list`, browse the source, or open [the oops-apps
index](https://project-oops.github.io/oops-apps/) for screenshots and the latest build of each
one. There is no catalog table here: the source tree, the `list` command and the index are the
catalog.

## Cross-project links

- [OOPS front door](../README.md) - collection overview and build instructions.
- [The OOPS Loop](../docs/THE_LOOP.md) - the ecosystem loop specification.
- [oops-sdk](../oops-sdk/) - freestanding C runtime used by all applications.
- [SELFish](../selfish/) - packages applications into `.eboot` and title directories.
- [Prosperous](../prosperous/) - deploys and launches applications on the hardware.
- [Orbistoun](../orbistoun/) - clean-room emulator testing these applications.
