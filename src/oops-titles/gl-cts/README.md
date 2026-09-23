# gl-cts

The Khronos OpenGL Conformance Test Suite, running on the console.

## About

`upstream.lock` pins `opengl-cts-4.6.8.1`, peeled commit `067e8832315e`. **This is the only
thing in the collection that can say the word "conformance"**: everything else here measures
what this project wrote against what this project expected, and the worth of a CTS result is
that the tests are somebody else's and the answer is not ours to arrange.

- **Fetched, not vendored** — only `shim/`, `tools/`, the Makefile and the lock live here.
- **Sparse**: `framework/` only, 10 MB. `external/openglcts` is a further 177 MB of test modules
  and data and is not fetched yet, deliberately — the framework has to build and run first.
- **No patches.** If upstream needs changing to run here, that is a finding about the port.

## Where it is

**196 of 220 framework sources compile for the target.** Run `tools/survey.sh` for the current
number and the cause histogram; it compiles every source one at a time and groups the failures,
which is the only useful shape for a port this size.

Nothing links yet and nothing has run. Building is not conforming, and the distinction matters
more here than anywhere else in the collection.

### How it got there

Three findings, in the order they mattered:

| | files it unblocked |
|---|---|
| libc++'s headers must precede oops-sdk's `include/libc` | **116** |
| the generated GL/EGL wrappers are checked in, under `wrapper/` | 42 |
| `<fstream>`, and the five stdio names `basic_filebuf` reaches for | 3 |

The first is the one worth remembering. libc++ ships its own `<math.h>`, `<string.h>` and
friends — thin wrappers that pull in the C library's with `#include_next` and add the C++
overloads — and `<cmath>` checks that its wrapper was the one found. With the SDK's `include/libc`
first, the C header wins and **161 of 260 sources failed on that one line**. It is also why
`common/cxxrt.cpp` declares `std::set_terminate` by hand rather than including `<exception>`,
which was read at the time as a quirk of that file.

### The 24 that do not, and which of them matter

| blocked on | files | disposition |
|---|---|---|
| `signal.h`, `unistd.h`, `sys/stat.h`, `sys/wait.h`, `sys/socket.h`, `dlfcn.h`, `dirent.h`, `semaphore.h`, `xeXMLParser.hpp` | 13 | **Not needed.** These are `deutil`'s process, socket, directory and dynamic-library helpers and the `xexml` executor — the machinery for running tests *remotely* from a host. A title that runs its own cases on the console needs none of it |
| `sinh` and friends, `fenv.h`, `posix_memalign`, `struct timespec` | 7 | oops-sdk gaps, each small |
| `pthread.h` | 2 | `dethread`. The framework itself includes no `<thread>` or `<mutex>` — dEQP threads itself in C — so this is a real but bounded piece |
| `png.h` | 1 | `tcuImageIO`, for reference-image comparison. Wanted eventually |
| `DEQP_TARGET_NAME is not defined!` | 1 | **Ours.** `deDefs.h` wants to know what platform this is, and answering it is the first line of the port rather than an obstacle to it |

## What is not decided yet

**How the subset is chosen.** It must be at run time, through `--deqp-case`, never by compiling
a reduced binary: a suite we pruned at compile time is one we curated. That is `oops-mesa`'s
roadmap row 8 and this title does not get to reopen it — it means building the whole `glcts`,
which will be a much larger link than anything here so far.

**How the whole thing links.** 196 sources compiling is not an executable, and `glcts` is a much
larger link than anything attempted here.

## The platform layer

`shim/tcuOopsPlatform.cpp`, and it compiles. **It is deliberately the only thing this port
writes** — everything else built here is upstream's, which is the property that makes a CTS
result worth having.

It is smaller than any platform upstream ships, because there is nothing to negotiate: a
`tcu::Platform` exposing a `glu::Platform`, one `glu::ContextFactory`, and a
`glu::RenderContext` whose four methods are the GL version, the function table, the render
target, and a present. `oops_gfx_create` opens the display and makes a context current in one
call — what the X11 platform spends four files on. EGL and Vulkan are left to the base class,
which reports them unsupported, and that is the correct answer rather than a gap.

### The function loader is a table, and that is the interesting part

dEQP fills `glw::Functions` through a loader shaped like `dlsym`. This platform has no runtime
symbol table, and Mesa does not export `_glapi_get_proc_address` in this build — both checked. So
the lookup happens at link time: `tools/gen-gl-loader.py` reads the names out of upstream's own
`glwInitGL33.inl` and `glwInitExtGL.inl`, intersects them with what `nm` says Mesa's
`libglapi_bridge.a` defines, and emits the table.

The intersection is the point. **A null is a real answer**: `glu::ContextInfo` checks a pointer
before use and a test whose entry point is absent reports `NotSupported`. A loader that returned
something plausible would turn that into a fault inside the test.

### 454 of 856, and some of the gap is GL 3.3 core

That is what the intersection currently yields, and it is worth knowing before a run rather than
after one. Most of the 402 absent names are extensions Mesa does not implement, which is
expected and harmless. **Some are not.** `glGetQueryObjecti64v`, `glGetQueryObjectui64v` and
`glQueryCounter` are `GL_ARB_timer_query`, which is *core in 3.3* — and the archive has
`glGetQueryObjectiv` and `glGetQueryObjectuiv` but not the 64-bit forms.

So this is a question for oops-mesa's `libglapi_bridge` rather than for the CTS: the bridge is
built specially (`build_by_default : false` upstream, see mesa-demos' README) and appears not to
cover the whole 3.3 entry-point set. Re-run the generator after any change to it; the count and
the full absent list go to stderr.
