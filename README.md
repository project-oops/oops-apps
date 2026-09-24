# oops-apps

**Native homebrew for Orbis and Prospero — demos, ported games, and utilities.**

`oops-apps` is the native homebrew for the platform: 3D graphics demos, ports of upstream games,
and system utilities, all built on [oops-sdk](../oops-sdk/). The demos we write from scratch are
the known-good yardstick the rest of OOPS is measured against — the compilers, the packaging,
the hardware transport and the emulator are all proven here first — and the ported games put the
same toolchain through real, third-party code.

| 📖 **[User & Operator Guide](docs/USER_GUIDE.md)** | 📐 **[Design Decisions](docs/DECISIONS.md)** |
| :--- | :--- |
| *Building and running the demos, and using tracer.* | *Numbered decisions and their reasoning.* |

⬇️ **[Browse and download every app](https://project-oops.github.io/oops-apps/)** — the
oops-apps index, built fresh from `main` with screenshots and one-click downloads.

---

## Role in THE LOOP

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

1. **Unambiguous Ground Truth**: Commercial games are massive, opaque, and fail without clear diagnostics. In `oops-apps`, we know every line of code, every vertex buffer, and every expected return value.
2. **End-to-End Pipeline Verification**: If `gl1-cube` renders on physical hardware but fails in [Orbistoun](../orbistoun/), the gap is isolated immediately without guessing.
3. **Dogfooding First-Party Tooling**: Every application builds with `app.mk`, packages with `selfish`, and deploys with `pros`.

---

## Developer Quickstart

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
Deploy and launch it on hardware with [Prosperous](../prosperous/), or run it in
[Orbistoun](../orbistoun/). The step-by-step — including
[creating your own application](docs/USER_GUIDE.md#4-creating-a-new-application-in-oops-apps) —
is in the **[User & Operator Guide](docs/USER_GUIDE.md)**.

---

## Finding the apps

Apps live under `src/`, grouped by the part of the collection they exercise. List them with
`./bin/oops-apps list`, browse the source, or **[open the oops-apps
index](https://project-oops.github.io/oops-apps/)** to see screenshots and download the latest
build of each one. There is deliberately no catalog table here — the source tree, the `list`
command and the index are always current; a hand-kept list never is.

---

## Cross-Project Links

- **[Master OOPS Front Door](../README.md)** — Collection overview and building instructions.
- **[The OOPS Loop](../docs/THE_LOOP.md)** — Master ecosystem loop specification.
- **[oops-sdk](../oops-sdk/)** — Freestanding C runtime used by all applications.
- **[SELFish](../selfish/)** — Packages applications into `.eboot` and title directories.
- **[Prosperous](../prosperous/)** — Deploys and launches applications on hardware.
- **[Orbistoun](../orbistoun/)** — Clean-room emulator testing these applications.
