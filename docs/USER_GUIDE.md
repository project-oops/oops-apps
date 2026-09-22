# oops-apps User & Operator Guide

Welcome to the **oops-apps** catalog and operator guide.

This guide provides instructions for **building, testing, and running our homebrew demo titles**, as well as using the **`tracer`** tool to passively record hardware telemetry and graphics command streams.

If you are an AI coding agent or graphics systems architect seeking the internal AGC shader pipelines or hook trampoline disassembly, see the **[Technical Reference](README.md)** and **[src/oops-payloads/tracer/README.md](../src/oops-payloads/tracer/README.md)**.

---

## Table of Contents

1. [Application Catalog](#1-application-catalog)
2. [Building & Running the Demo Titles](#2-building--running-the-demo-titles)
   - [GL-Cube (`src/gl-cube`)](#gl-cube-srcgl-cube)
3. [Using `tracer` for Passive Telemetry](#3-using-tracer-for-passive-telemetry)
   - [What `tracer` Does](#a-what-tracer-does)
   - [Attaching `tracer` to an Application](#b-attaching-tracer-to-an-application)
   - [Decoding & Feeding Traces into Orbistoun](#c-decoding--feeding-traces-into-orbistoun)
4. [Creating a New Application in `oops-apps`](#4-creating-a-new-application-in-oops-apps)

---

## 1. Application Catalog

`oops-apps` houses the apps and payloads built on `oops-sdk`:

| Application | Title ID | Description | Notes / Subsystems |
| :--- | :--- | :--- | :--- |
| **`gl1-cube`** | `GLCB00001` | 3D rotating cube demo (OpenGL 1.x via `oops-sdk` `gl/` on AGC); the pinned hardware oracle. | `OOPS_RENDERER = gl1`. Direct memory mapping, RDNA2 AGC universal queue, PM4 DCB submission, fence synchronisation. Interactive pad toggles + shared GPU HUD. |
| **`gl1-probe`** | `GLPB00001` | Breadth check: many small draws, each read back and decided, on host and console. | `OOPS_RENDERER = gl1`. A difference between the two runs is a hardware-path bug. |
| **`gl2-cube`** | `GLTC00001` | Programmable-pipeline (OpenGL 2.0 / GLSL) cube. | `OOPS_RENDERER = gl2`. |
| **`gl2-probe`** | `GLTP00001` | GL 2.0 shader-path checks. | `OOPS_RENDERER = gl2`. |
| **`seashell`** | `SCSH00001` | SeaShell unified homebrew shell (title launcher, settings, save/media manager). | Ships as a native eboot Big App (category 0, root). Display + software canvas, multi-port pad, filesystem discovery, PNG icon decode. |
| **`gallery`** | `GALR00001` | Capability showcase across SDK subsystems. | `oops_*_available()` reachability across display, draw, input, audio, net, and media decode. |
| **`pad-viz`** | `PADV00001` | Live DualSense/DualShock controller telemetry visualizer. | Batched low-latency input (`oops_input_poll_batch`); sticks, triggers, 6-axis IMU, touchpad. |
| **`net-tool`** | `NETT00001` | Network configuration and interface diagnostics. | SDK inet helpers, link status, UDP status responder. |
| **`porthole`** | `PORT00001` | Remote-play target payload: video out and controller input over TCP. | Plain-ELF payload; host half in Prosperous. POSIX sockets, sysmodule load, `klog`; encoder gated off (D003/D004). |
| **`tracer`** | `TRAC00001` | In-process passive hooking & telemetry engine for real titles. | Plain-ELF payload. Intercepts `sceAgcSubmitDcb` / `sceVideoOutSubmitFlip`; captures PM4 DCB packets and RDNA2 shader bytecode. |
| **`sandbox-daemon`** | `SNDA00001` | On-demand filesystem-namespace unsandboxing daemon. | Plain-ELF payload (root). Loopback IPC `127.0.0.1:9069`; FreeBSD `filedesc`/`ucred` kernel-memory updates. |
| **`pltauth-patch`** | `PLTA00001` | Kernel patcher for SceShellCore / platform-authentication entitlement checks. | Plain-ELF payload (system tool). |
| **`mesa-winsys-probe`** | `MESA00001` | OpenGL-through-Mesa bring-up app (winsys path). | Hosted (non-freestanding) link via `OOPS_RENDERER = mesa`; oops-mesa shim over upstream Mesa. |
| **`mesa-dri-probe`** | `DRIP00001` | The same stack through the Gallium DRI frontend; renders and hashes a frame. | Hosted; `OOPS_RENDERER = mesa`. |
| **`mesa-cube`** | `MCUB00001` | The example title: a textured, depth-tested cube through upstream Mesa, presenting every frame at 59.94 fps. | Hosted; `OOPS_RENDERER = mesa`. Interactive pad toggles + shared GPU HUD, same as `gl1-cube`. |
| **`sdl-probe`** | `SDLP00001` | Upstream SDL2 on the console through `oops-sdl`: init, window + GL context, event pump, controller. | `src/oops-frameworks/`. Links a renderer underneath; the framework is the point. |
| **`glut-demo`** | `GLUT00001` | An ordinary GLUT program built for the console by compiling it against the SDK's `<GL/glut.h>`. | `src/oops-frameworks/`. |
| **`cxx-throw`** | `CXTH00001` | C++ exception-handling probe (throw/catch across the runtime). | `src/oops-utilities/`. |
| **`injector`** | — | Standalone process payload injector. | Source under `src/injector/` (internals not covered here). |

---

## 2. Building & Running the Demo Titles

All apps follow the standard Makefile workflow.

### GL-Cube (`src/gl-cube`)

1. **Build Title Directory**:
   ```bash
   cd oops-apps/src/gl-cube
   make title
   ```
2. **Deploy to Console**:
   ```powershell
   pros.exe restore build/title/GLCB00001 /data/homebrew/GLCB00001
   pros.exe launch GLCB00001
   ```
3. **Run in Orbistoun Emulator**:
   ```powershell
   orbistoun.exe run build/title/GLCB00001
   ```

---

## 3. Using `tracer` for Passive Telemetry

### A. What `tracer` Does
While `obSCEne` actively probes known functions with synthetic parameters, `tracer` runs **passively inside real games**:
- Installs clean trampolines over key export stubs (`sceAgcSubmitDcb`, `sceVideoOutSubmitFlip`).
- Copies submitted PM4 draw packets and compute dispatches to a circular ring buffer in memory.
- Flushes captured telemetry over local sockets to your PC without interrupting game execution.

### B. Attaching `tracer` to an Application
`tracer` builds as a freestanding plain-ELF payload (`build/tracer.elf`):
```bash
cd oops-apps/src/tracer
make elf
```
It runs either injected into a target process (via `injector`) or as a standalone diagnostic payload. On start it installs its trampolines over the target export stubs and begins recording telemetry.

### C. Decoding & Feeding Traces into Orbistoun
Pull the binary trace capture off the console:
```powershell
pros.exe pull /data/trace-<TITLE_ID>.bin .
```
The on-disk trace format and its reader live in `tracer`'s own host-side decoder (`trace_decode.c` / `trace_format.h`, exercised by `make check`). The decoded PM4 draw streams and RDNA2 shader bytecode are what ground Orbistoun's packet decoders and shader recompiler (see [`src/oops-payloads/tracer/README.md`](../src/oops-payloads/tracer/README.md)).

---

## 4. Creating a New Application in `oops-apps`

1. Create a new directory under `oops-apps/src/<your-app-name>`.
2. Add your source file(s) (e.g. `<your-app-name>.c`) in that directory, referencing `oops-sdk` headers.
3. Create an `app.env` for the app's identity and release formats:
   ```properties
   APP_NAME=your-app-name
   TITLE_ID=YOUR00001
   TITLE_NAME="My Test App"
   FORMATS=elf eboot title
   ```
4. Create a `Makefile`:
   ```makefile
   OOPS_APPS_ROOT ?= $(abspath ../..)

   PAYLOAD_SRCS = your-app-name.c
   ENTRY_POINT := your_app_name_start

   include $(OOPS_APPS_ROOT)/common/app.mk
   ```
5. Run `make title` and deploy with `pros`!

### 4a. Choosing a renderer (`OOPS_RENDERER`)

An app that draws with OpenGL picks its renderer with **one line** in the `Makefile`, before the
`include`:

```makefile
OOPS_RENDERER = gl1     # fixed-function OpenGL 1.x, freestanding
# OOPS_RENDERER = gl2   # programmable OpenGL 2.0, freestanding
# OOPS_RENDERER = mesa  # OpenGL through upstream Mesa (hosted: its own C runtime)
```

That is the whole choice. `gl1`/`gl2` link oops-gl's implementation; `mesa` brings up the hosted
Mesa stack (a "hosted" title carries its own C library and is packaged slightly differently —
oops-mesa D002). You do **not** list the GL sources or set `USE_MESA` by hand; the flag does it.

Both renderers present the **same API** (`oops/gfx.h`), so your source does not change when you
switch backends:

```c
#include "oops/gfx.h"

oops_gfx_t *gfx = oops_gfx_create(&(oops_gfx_desc_t){ .width = 1920, .height = 1080,
                                                      .depth = true, .vsync = true });
/* ... draw with ordinary GL each frame ... */
oops_gfx_present(gfx);         /* flip */
/* ... at the end ... */
oops_gfx_destroy(gfx);
```

`oops_gfx_create` opens the display and makes a GL context current for you — you never call
`oops_display_open` yourself. `oops_gfx_display(gfx)` hands back the display if you need it (for
input, or a 2D overlay).

To draw text or panels over your 3D scene, use the GPU overlay (`oops/hud.h`) rather than writing
pixels by hand — it works on both renderers, including Mesa, whose scanout buffer the CPU cannot
touch:

```c
#include "oops/hud.h"

oops_hud_t *hud = oops_hud_create(w, h);   /* once, after the context is current */
/* ... each frame, after your scene and before present: */
oops_hud_begin(hud);
oops_hud_text(hud, 40, 40, 2, OOPS_COLOR_WHITE, "HELLO");
oops_hud_end(hud);
```

A title that builds **only** a host self-test (no payload) lists oops-gl in `HOST_TEST_SRCS`
itself and leaves `OOPS_RENDERER` unset.

