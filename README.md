# oops-apps

**Conforming Homebrew Applications and 3D Graphics Testbed for Prospero.**

`oops-apps` is the collection of native homebrew applications, 3D graphics demos, and system utilities built on [oops-sdk](../oops-sdk/). It serves as the primary testbed for the entire OOPS collection, verifying our compilers, packaging tools, hardware communication, and emulator on known-source applications.

| 📖 **[User & Operator Guide](docs/USER_GUIDE.md)** | ⚙️ **[Technical Reference & Architecture](README.md)** |
| :--- | :--- |
| *App catalog, building demos (gl-cube), and using tracer.* | *Testbed role in THE LOOP, AGC pipelines, and tracer hooks.* |

---

## Role in THE LOOP

Within the [OOPS ecosystem](../docs/THE_LOOP.md), `oops-apps` is the **Known Ground-Truth Testbed**:

```
[Write Application in oops-apps (e.g. gl-cube)]
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
│ Deploy to PS5 via Prosperous            │   │ Run in Orbistoun Emulator     │
│ - pros restore ... && pros launch ...   │   │ - ./bin/orbistoun run ...     │
│ - Hardware renders on TV via HDMI       │   │ - Verifies Vulkan render loop │
└───────────────────────┬─────────────────┘   └───────────────┬───────────────┘
                        │                                     │
                        └──────────────► Match ◄──────────────┘
```

1. **Unambiguous Ground Truth**: Commercial games are massive, opaque, and fail without clear diagnostics. In `oops-apps`, we know every line of code, every vertex buffer, and every expected return value.
2. **End-to-End Pipeline Verification**: If `gl-cube` renders a red cube on physical PS5 hardware, but fails in [Orbistoun](../orbistoun/), the gap is isolated immediately without guessing.
3. **Dogfooding First-Party Tooling**: Every application builds with `app.mk`, packages with `selfish`, and deploys with `pros`.

---

## Developer Quickstart

### 1. Build and Run Host Tests
Cross-compilation uses the `oops-builder` WSL distribution on Windows or native `clang`/`lld` on Linux:

```bash
./bin/oops-apps build    # builds all applications
./bin/oops-apps check    # runs host-side self tests
```

### 2. Build a Complete Title Package (`gl-cube`)
```bash
cd src/gl-cube
make title
```
This invokes `selfish` to generate a fully conforming title directory:
- `build/title/GLCB00001/eboot.bin`
- `build/title/GLCB00001/sce_sys/param.json`
- `build/title/GLCB00001/sce_sys/keystone`
- `build/title/GLCB00001/sce_module/libc.prx`

### 3. Deploy and Launch on Real Hardware
```bash
pros.exe restore build\title\GLCB00001 /data/homebrew/GLCB00001
pros.exe launch GLCB00001
pros.exe logs --seconds 15
```

---

## Application Catalog

| Application | Path | Description |
|---|---|---|
| **`gl-cube`** | `src/gl-cube/` | 3D spinning cube graphics demo exercising RDNA2 AGC universal queues, PM4 direct register packets, and 64 KB tile swizzling. |
| **`wipeout`** | `src/wipeout/` | Native clean-room 3D WipEout engine port for Prospero. |
| **`seashell`** | `src/seashell/` | SeaShell unified homebrew shell for Prospero (title launcher, settings, save manager). |
| **`porthole`** | `src/porthole/` | Background remote play daemon streaming unencrypted video and receiving controller input over TCP. |
| **`pad-viz`** | `src/pad-viz/` | Live DualSense controller telemetry visualizer (analog stick drift, triggers, 6-axis IMU). |
| **`tracer`** | `src/tracer/` | In-process passive telemetry recorder and hook engine. Intercepts API calls, out-parameter buffers, AGC PM4 DCB submissions, and bound RDNA2 shader bytecode from running games to feed `obscene-tool` and `orbistoun`. |
| **`gallery`** | `src/gallery/` | Visual showcase of SDK display, audio PCM, and media playback capabilities. |

---

## Creating a New Application

Create a directory under `src/<app-name>` with an `app.env` file and a `Makefile`:

```properties
# src/myapp/app.env
APP_NAME=myapp
TITLE_ID=MYAP00001
TITLE_NAME="My Application"
TITLE_CATEGORY=big-app
FORMATS=title eboot
```

```makefile
# src/myapp/Makefile
OOPS_APPS_ROOT ?= $(abspath ../..)
PAYLOAD_SRCS = myapp.c
include $(OOPS_APPS_ROOT)/common/app.mk
```

---

## Cross-Project Links

- **[Master OOPS Front Door](../README.md)** — Collection overview and building instructions.
- **[The OOPS Loop](../docs/THE_LOOP.md)** — Master ecosystem loop specification.
- **[oops-sdk](../oops-sdk/)** — Freestanding C runtime used by all applications.
- **[SELFish](../selfish/)** — Packages applications into `.eboot` and title directories.
- **[Prosperous](../prosperous/)** — Deploys and launches applications on hardware.
- **[Orbistoun](../orbistoun/)** — Clean-room emulator testing these applications.
