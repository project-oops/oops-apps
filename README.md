# oops-apps

**Conforming Homebrew Applications and 3D Graphics Testbed for Prospero.**

`oops-apps` is the collection of native homebrew applications, 3D graphics demos, and system utilities built on [oops-sdk](../oops-sdk/). It serves as the primary testbed for the entire OOPS collection, verifying our compilers, packaging tools, hardware communication, and emulator on known-source applications.

| 📖 **[User & Operator Guide](docs/USER_GUIDE.md)** | ⚙️ **[Technical Reference & Architecture](README.md)** |
| :--- | :--- |
| *App catalog, building demos (gl1-cube), and using tracer.* | *Testbed role in THE LOOP, AGC pipelines, and tracer hooks.* |

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
│ Deploy to PS5 via Prosperous            │   │ Run in Orbistoun Emulator     │
│ - pros restore ... && pros launch ...   │   │ - ./bin/orbistoun run ...     │
│ - Hardware renders on TV via HDMI       │   │ - Verifies Vulkan render loop │
└───────────────────────┬─────────────────┘   └───────────────┬───────────────┘
                        │                                     │
                        └──────────────► Match ◄──────────────┘
```

1. **Unambiguous Ground Truth**: Commercial games are massive, opaque, and fail without clear diagnostics. In `oops-apps`, we know every line of code, every vertex buffer, and every expected return value.
2. **End-to-End Pipeline Verification**: If `gl1-cube` renders a red cube on physical PS5 hardware, but fails in [Orbistoun](../orbistoun/), the gap is isolated immediately without guessing.
3. **Dogfooding First-Party Tooling**: Every application builds with `app.mk`, packages with `selfish`, and deploys with `pros`.

---

## Developer Quickstart

### 1. Build and Run Host Tests
Cross-compilation uses the `oops-builder` WSL distribution on Windows or native `clang`/`lld` on Linux:

```bash
./bin/oops-apps build    # builds all applications
./bin/oops-apps check    # runs host-side self tests
```

### 2. Build a Complete Title Package (`gl1-cube`)
```bash
cd src/oops-gl/gl1-cube
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

Apps are filed by **category**, and the category says which part of the collection an app
exercises rather than what kind of program it is. `oops-gl` and `oops-mesa` are named after the
implementations they test - "opengl" and "mesa" as siblings would be confusing, since Mesa *is*
OpenGL - and the split also matches who works on what.

**An app's name stays bare**: `./bin/oops-apps dist gl1-cube` does not need to know the
category, and `oops-apps` refuses two categories sharing a name rather than resolving it, since
bare names are only usable while they are unique.

| Application | Path | Description |
|---|---|---|
| **`gl1-cube`** | `src/oops-gl/gl1-cube/` | 3D spinning cube demo, and the **pinned hardware oracle**: its frame is measured on a retail console and asserted register by register by oops-sdk. Its run modes and what a console run should measure are in [its README](src/oops-gl/gl1-cube/README.md). |
| **`gl1-probe`** | `src/oops-gl/gl1-probe/` | Thirty-three checks that each drive one GL 1.x feature and read the pixels back to decide. Runs the same suite on the host rasteriser and on the console, so a difference between them is a hardware-path bug. |
| **`gl2-cube`** | `src/oops-gl/gl2-cube/` | The GL 2.0 oracle, once there is a GL 2.0 back end to record. Today it checks its own shaders through the GLSL front end and builds no payload. |
| **`mesa-winsys-probe`** | `src/oops-mesa/mesa-winsys-probe/` | OpenGL-through-Mesa bring-up app (`OOPS_RENDERER = mesa`; hosted rather than freestanding). Calls `radeonsi_screen_create` directly - the winsys path - and stays the control when `mesa-dri-probe` stops somewhere. |
| **`mesa-dri-probe`** | `src/oops-mesa/mesa-dri-probe/` | The same stack through the **Gallium DRI frontend** rather than the winsys directly: screen, drawable, context, a fixed-function triangle, a GLSL 330 one, and a full-frame hash of the result. Unit 6's gate (`0x5188ddb7`, oops-mesa worklog 066). A probe, so it answers and stops. |
| **`mesa-cube`** | `src/oops-mesa/mesa-cube/` | The **example** title for `USE_MESA`, and the one to read before writing your own: a textured, depth-tested, index-drawn cube animated through a matrix pipeline, presenting every frame. Ordinary GL 3.3 throughout - only three lines differ from the same program on a desktop, and each is commented where it appears. An example, **not** conformance. |
| **`seashell`** | `src/oops-utilities/seashell/` | SeaShell unified homebrew shell for Prospero (title launcher, settings, save manager). |
| **`porthole`** | `src/oops-payloads/porthole/` | Remote-play target payload streaming video out and receiving controller input over TCP (host half in Prosperous). |
| **`tracer`** | `src/oops-payloads/tracer/` | In-process passive telemetry recorder and hook engine. Intercepts API calls, out-parameter buffers, AGC PM4 DCB submissions, and bound RDNA2 shader bytecode from running games. |
| **`sandbox-daemon`** | `src/oops-payloads/sandbox-daemon/` | On-demand filesystem-namespace unsandboxing daemon over loopback IPC (`127.0.0.1:9069`). |
| **`pltauth-patch`** | `src/oops-payloads/pltauth-patch/` | Kernel patcher for SceShellCore / platform-authentication entitlement checks. |
| **`injector`** | `src/oops-payloads/injector/` | Standalone process payload injector. |
| **`pad-viz`** | `src/oops-utilities/pad-viz/` | Live DualSense controller telemetry visualizer (analog sticks, triggers, 6-axis IMU, touchpad). |
| **`gallery`** | `src/oops-utilities/gallery/` | Visual showcase of SDK display, audio PCM, and media playback capabilities. |
| **`net-tool`** | `src/oops-utilities/net-tool/` | Network configuration and interface diagnostics (link status, SDK inet helpers, UDP status responder). |

The title targets - which open-source games go in which slot, and what was actually verified
about each - are in [`src/oops-titles/README.md`](src/oops-titles/README.md).

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
