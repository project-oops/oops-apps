# oops-apps User & Operator Guide

Welcome to the **oops-apps** catalog and operator guide.

This guide provides instructions for **building, testing, and running our homebrew demo titles**, as well as using the **`tracer`** tool to passively record hardware telemetry and graphics command streams.

If you are an AI coding agent or graphics systems architect seeking the internal AGC shader pipelines or hook trampoline disassembly, see the **[Technical Reference](README.md)** and **[src/tracer/README.md](../src/tracer/README.md)**.

---

## Table of Contents

1. [Application Catalog](#1-application-catalog)
2. [Building & Running the Demo Titles](#2-building--running-the-demo-titles)
   - [GL-Cube (`src/gl-cube`)](#gl-cube-srcgl-cube)
   - [WipEout Model Viewer (`src/wipeout`)](#wipeout-model-viewer-srcwipeout)
   - [Home Launcher (`src/home`)](#home-launcher-srchome)
3. [Using `tracer` for Passive Telemetry](#3-using-tracer-for-passive-telemetry)
   - [What `tracer` Does](#a-what-tracer-does)
   - [Attaching `tracer` to an Application](#b-attaching-tracer-to-an-application)
   - [Decoding & Feeding Traces into Orbistoun](#c-decoding--feeding-traces-into-orbistoun)
4. [Creating a New Application in `oops-apps`](#4-creating-a-new-application-in-oops-apps)

---

## 1. Application Catalog

`oops-apps` houses all native test titles built on `oops-sdk`:

| Application | Title ID | Description | Hardware Subsystems Tested |
| :--- | :--- | :--- | :--- |
| **`gl-cube`** | `GLCB00001` | Stage 2 3D rotating textured cube. | Direct memory mapping, RDNA2 AGC universal queue, PM4 DCB submission, fence synchronisation. |
| **`wipeout`** | `WIPE00001` | High-poly ship and track model viewer. | DualSense analog stick camera control, vertex/index buffer streams, multi-pass rendering. |
| **`home`** | `HOME00001` | Lightweight home-screen title launcher. | Filesystem directory scans (`/data/homebrew`), icon loading, process launch supervisor. |
| **`tracer`** | N/A | Dynamic in-process hooking & telemetry module. | Intercepts `sceAgcSubmitDcb`, logs GPU command lists and real-world shader constants. |

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

### WipEout Model Viewer (`src/wipeout`)

1. **Build Title Directory**:
   ```bash
   cd oops-apps/src/wipeout
   make title
   ```
2. **Launch & Control**:
   - `Left Stick`: Rotate camera.
   - `Right Stick`: Zoom & pan.
   - `L1 / R1`: Cycle ship models.

---

## 3. Using `tracer` for Passive Telemetry

### A. What `tracer` Does
While `obSCEne` actively probes known functions with synthetic parameters, `tracer` runs **passively inside real games**:
- Installs clean trampolines over key export stubs (`sceAgcSubmitDcb`, `sceVideoOutSubmitFlip`).
- Copies submitted PM4 draw packets and compute dispatches to a circular ring buffer in memory.
- Flushes captured telemetry over local sockets to your PC without interrupting game execution.

### B. Attaching `tracer` to an Application
`tracer` is compiled as a preload module (`tracer.prx`):
```bash
cd oops-apps/src/tracer
make prx
```
Stage `tracer.prx` into the title's `sce_module/` folder alongside `libc.prx`. The dynamic linker will automatically initialize telemetry on launch.

### C. Decoding & Feeding Traces into Orbistoun
Decode binary trace captures into human-readable PM4 packets:
```powershell
obscene-tool.exe trace --input raw_trace.bin --output decoded_trace.json
```
Copy `decoded_trace.json` to `orbistoun/compat/fixtures/` to create automated regression tests in the emulator!

---

## 4. Creating a New Application in `oops-apps`

1. Create a new directory under `oops-apps/src/<your-app-name>`.
2. Create `src/main.c` referencing `oops-sdk` headers.
3. Create `Makefile` with:
   ```makefile
   APP_NAME   := your-app-name
   TITLE_ID   := TEST00001
   TITLE_NAME := "My Test App"
   SRCS       := src/main.c

   include ../common/app.mk
   ```
4. Run `make title` and deploy with `pros`!

