# oops-apps User and Operator Guide

Building, testing and running the demo titles, using `tracer` to record hardware telemetry, and
adding an application.

## Contents

1. [Finding the apps](#1-finding-the-apps)
2. [Building and running a title](#2-building-and-running-a-title)
3. [Using `tracer` for passive telemetry](#3-using-tracer-for-passive-telemetry)
   - [What `tracer` does](#a-what-tracer-does)
   - [Attaching `tracer` to an application](#b-attaching-tracer-to-an-application)
   - [Decoding and feeding traces into Orbistoun](#c-decoding-and-feeding-traces-into-orbistoun)
4. [Creating a new application in `oops-apps`](#4-creating-a-new-application-in-oops-apps)

## 1. Finding the apps

The apps live under `src/`, grouped by the part of the collection they exercise. Three ways to
see what is here:

- **The oops-apps index** - [project-oops.github.io/oops-apps](https://project-oops.github.io/oops-apps/) - screenshots, descriptions and a download of the latest build of each one.
- **`./bin/oops-apps list`** - every app the repository holds, as the build and release tooling sees it.
- **The source tree** - each app is a directory under `src/<group>/<app>/` with its own `README.md`, `app.env` and `Makefile`.

There is no catalog table in the docs: the index, the `list` command and the source tree are the
catalog.

## 2. Building and running a title

Every app follows the same Makefile workflow. Using `gl1-cube` as the example:

1. Build the title directory:
   ```bash
   cd src/oops-gl/gl1-cube
   make title
   ```
2. Deploy and launch on the hardware:
   ```powershell
   pros.exe restore build/title/GLCB00001 /data/homebrew/GLCB00001
   pros.exe launch GLCB00001
   ```
3. Or run it in the Orbistoun emulator:
   ```bash
   ./bin/orbistoun run GLCB00001
   ```

## 3. Using `tracer` for passive telemetry

### A. What `tracer` does

`obSCEne` probes known functions with synthetic parameters; `tracer` runs passively inside real
games:

- It installs trampolines over key export stubs (`sceAgcSubmitDcb`, `sceVideoOutSubmitFlip`).
- It copies submitted PM4 draw packets and compute dispatches to a circular ring buffer in memory.
- It flushes captured telemetry over local sockets to the host without interrupting the game.

### B. Attaching `tracer` to an application

`tracer` builds as a freestanding plain-ELF payload (`build/tracer.elf`):

```bash
cd src/oops-payloads/tracer
make elf
```

It runs either inside a target process (loaded by `injector`) or as a standalone diagnostic
payload. On start it installs its trampolines over the target export stubs and begins recording.

### C. Decoding and feeding traces into Orbistoun

Pull the binary trace capture off the hardware:

```powershell
pros.exe pull /data/trace-<TITLE_ID>.bin .
```

The on-disk trace format and its reader live in `tracer`'s host-side decoder (`trace_decode.c` /
`trace_format.h`, exercised by `make check`). The decoded PM4 draw streams and RDNA2 shader
bytecode ground Orbistoun's packet decoders and shader recompiler (see
[`src/oops-payloads/tracer/README.md`](../src/oops-payloads/tracer/README.md)).

## 4. Creating a new application in `oops-apps`

1. Create a directory under `oops-apps/src/<your-app-name>`.
2. Add the source file(s) (e.g. `<your-app-name>.c`) in that directory, using `oops-sdk` headers.
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
5. Run `make title` and deploy with `pros`.

### 4a. Choosing a renderer (`OOPS_RENDERER`)

An app that draws with OpenGL picks its renderer with one line in the `Makefile`, before the
`include`:

```makefile
OOPS_RENDERER = gl1     # fixed-function OpenGL 1.x, freestanding
# OOPS_RENDERER = gl2   # programmable OpenGL 2.0, freestanding
# OOPS_RENDERER = mesa  # OpenGL through upstream Mesa (hosted: its own C runtime)
```

`gl1`/`gl2` link oops-gl's implementation; `mesa` brings up the hosted Mesa stack (a hosted
title carries its own C library and is packaged differently - oops-mesa D002). The flag lists
the GL sources and sets `USE_MESA`; the Makefile does not.

Both renderers present the same API (`oops/gfx.h`), so the source does not change between
backends:

```c
#include "oops/gfx.h"

oops_gfx_t *gfx = oops_gfx_create(&(oops_gfx_desc_t){ .width = 1920, .height = 1080,
                                                      .depth = true, .vsync = true });
/* ... draw with ordinary GL each frame ... */
oops_gfx_present(gfx);         /* flip */
/* ... at the end ... */
oops_gfx_destroy(gfx);
```

`oops_gfx_create` opens the display and makes a GL context current; the app does not call
`oops_display_open`. `oops_gfx_display(gfx)` returns the display for input or a 2D overlay.

To draw text or panels over a 3D scene, use the GPU overlay (`oops/hud.h`) rather than writing
pixels by hand. It works on both renderers, including Mesa, whose scanout buffer the CPU cannot
touch:

```c
#include "oops/hud.h"

oops_hud_t *hud = oops_hud_create(w, h);   /* once, after the context is current */
/* ... each frame, after your scene and before present: */
oops_hud_begin(hud);
oops_hud_text(hud, 40, 40, 2, OOPS_COLOR_WHITE, "HELLO");
oops_hud_end(hud);
```

A title that builds only a host self-test (no payload) lists oops-gl in `HOST_TEST_SRCS` itself
and leaves `OOPS_RENDERER` unset.
