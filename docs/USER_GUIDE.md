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
| **`gl-cube`** | `GLCB00001` | 3D rotating cube demo (OpenGL via `oops-sdk` `gl/` on AGC). | Direct memory mapping, RDNA2 AGC universal queue, PM4 DCB submission, fence synchronisation. |
| **`seashell`** | `SCSH00001` | SeaShell unified homebrew shell (title launcher, settings, save/media manager). | Ships as a native eboot Big App (category 0, root). Display + software canvas, multi-port pad, filesystem discovery, PNG icon decode. |
| **`gallery`** | `GALR00001` | Capability showcase across SDK subsystems. | `oops_*_available()` reachability across display, draw, input, audio, net, and media decode. |
| **`pad-viz`** | `PADV00001` | Live DualSense/DualShock controller telemetry visualizer. | Batched low-latency input (`oops_input_poll_batch`); sticks, triggers, 6-axis IMU, touchpad. |
| **`net-tool`** | `NETT00001` | Network configuration and interface diagnostics. | SDK inet helpers, link status, UDP status responder. |
| **`porthole`** | `PORT00001` | Remote-play target payload: video out and controller input over TCP. | Plain-ELF payload; host half in Prosperous. POSIX sockets, sysmodule load, `klog`; encoder gated off (D003/D004). |
| **`tracer`** | `TRAC00001` | In-process passive hooking & telemetry engine for real titles. | Plain-ELF payload. Intercepts `sceAgcSubmitDcb` / `sceVideoOutSubmitFlip`; captures PM4 DCB packets and RDNA2 shader bytecode. |
| **`sandbox-daemon`** | `SNDA00001` | On-demand filesystem-namespace unsandboxing daemon. | Plain-ELF payload (root). Loopback IPC `127.0.0.1:9069`; FreeBSD `filedesc`/`ucred` kernel-memory updates. |
| **`pltauth-patch`** | `PLTA00001` | Kernel patcher for SceShellCore / platform-authentication entitlement checks. | Plain-ELF payload (system tool). |
| **`mesa-probe`** | `PPSA90010` | OpenGL-through-Mesa bring-up app. | Hosted (non-freestanding) link via `USE_MESA`; oops-mesa shim over upstream Mesa. |
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

