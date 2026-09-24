# oops-apps User & Operator Guide

Welcome to the **oops-apps** catalog and operator guide.

This guide covers **building, testing and running the demo titles**, using the **`tracer`** tool to record hardware telemetry, and **adding an application of your own**.

---

## Table of Contents

1. [Finding the apps](#1-finding-the-apps)
2. [Building & running a title](#2-building--running-a-title)
3. [Using `tracer` for Passive Telemetry](#3-using-tracer-for-passive-telemetry)
   - [What `tracer` Does](#a-what-tracer-does)
   - [Attaching `tracer` to an Application](#b-attaching-tracer-to-an-application)
   - [Decoding & Feeding Traces into Orbistoun](#c-decoding--feeding-traces-into-orbistoun)
4. [Creating a New Application in `oops-apps`](#4-creating-a-new-application-in-oops-apps)

---

## 1. Finding the apps

The apps live under `src/`, grouped by the part of the collection they exercise. Three always-current ways to see what is here:

- **The oops-apps index** — [project-oops.github.io/oops-apps](https://project-oops.github.io/oops-apps/) — screenshots, descriptions and a download of the latest build of each one.
- **`./bin/oops-apps list`** — every app the repository holds, as the build and release tooling sees it.
- **The source tree** — each app is a directory under `src/<group>/<app>/` with its own `README.md`, `app.env` and `Makefile`.

There is deliberately no catalog table in the docs: the index, the `list` command and the source tree never fall behind as apps are added; a hand-kept table does.

---

## 2. Building & running a title

Every app follows the same Makefile workflow. Using `gl1-cube` as the example:

1. **Build the title directory**:
   ```bash
   cd src/oops-gl/gl1-cube
   make title
   ```
2. **Deploy and launch on hardware**:
   ```powershell
   pros.exe restore build/title/GLCB00001 /data/homebrew/GLCB00001
   pros.exe launch GLCB00001
   ```
3. **Or run it in the Orbistoun emulator**:
   ```bash
   ./bin/orbistoun run GLCB00001
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
cd src/oops-payloads/tracer
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

