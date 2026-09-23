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

**208 of 220 framework sources compile for the target.** Run `tools/survey.sh` for the current
number and the cause histogram; it compiles every source one at a time and groups the failures,
which is the only useful shape for a port this size.

Nothing links yet and nothing has run. Building is not conforming, and the distinction matters
more here than anywhere else in the collection.

### This is a hosted title, and getting that wrong cost a day's reading

`OOPS_RENDERER = mesa` makes this a **hosted** title, and `common/app.mk` says what that means
at its `USE_MESA` block: the target C library comes from **the Mesa sysroot**, and oops-sdk's
freestanding libc headers *collide* with it — the sysroot's `__clock_t` is `int` where oops-sdk's
`clock_t` is `int64_t`. So `app.mk` empties `OOPS_SDK_LIBC_INCLUDE` for these titles.

The first pass of this port used oops-sdk's freestanding libc and reached 196 of 220, against a
configuration this title will never be built in. The sysroot is a full FreeBSD header set —
`unistd.h`, `signal.h`, `pthread.h`, `dirent.h`, `sys/stat.h`, a real `libm.a` — so most of what
looked like a long list of missing SDK work was the wrong question rather than work.

**The same trap caught libc++.** `oops-deps/libcxx/include/` held shims standing in for a C
library that does not exist freestanding — `locale.h`, `runetype.h`, `nl_types.h`. On a hosted
title's include path those *shadow FreeBSD's real ones*, and 128 of 220 sources failed with
errors naming neither directory. They now live in `include/freestanding/`, which a hosted build
does not put on the path.

### How it got there

| | files it unblocked |
|---|---|
| build against the Mesa sysroot, not oops-sdk's freestanding libc | **128** |
| libc++'s headers must precede the C library's | 116 |
| the generated GL/EGL wrappers are checked in, under `wrapper/` | 42 |

The middle one still holds and is worth remembering independently: libc++ ships its own
`<math.h>`, `<string.h>` and friends — wrappers that `#include_next` the C library's and add the
C++ overloads — and `<cmath>` *checks* that its wrapper was the one found.

### The 12 that do not

All single causes now, and all in `delibs` or at the edges: `u_int` and `cpusetid_t` (the staged
sysroot is a subset of FreeBSD's headers), `malloc_usable_size`, `execinfo.h`, `netinet6/in6.h`,
`fenv.h`, two files wanting a newer `_POSIX_C_SOURCE`, `png.h` for reference-image comparison,
and `xeXMLParser.hpp` for the executor.

Several are in files a self-contained run does not need — `deSocket.c`, `deProcess.c` and the
`xexml` executor exist to drive tests *remotely from a host*. The rest are small.

## What is not decided yet

**How the subset is chosen.** It must be at run time, through `--deqp-case`, never by compiling
a reduced binary: a suite we pruned at compile time is one we curated. That is `oops-mesa`'s
roadmap row 8 and this title does not get to reopen it — it means building the whole `glcts`,
which will be a much larger link than anything here so far.

**The link, and it is blocked on one thing that does not exist yet.** 208 objects is not an
executable. A hosted C++ title needs **libc++, libc++abi and libunwind built against the Mesa
sysroot**, and the only build of them is the freestanding one — against oops-sdk's libc, which
disagrees with the sysroot about `FILE` and `clock_t`. Linking the two together would produce a
binary that resolves every symbol and reads the wrong bytes.

That build should be *smaller* than the freestanding one, not larger: the sysroot has real
`xlocale.h`, `locale.h` and `runetype.h`, and `librune.a` already supplies `_DefaultRuneLocale`,
so upstream's own FreeBSD locale backend applies and none of `include/freestanding/` is needed.

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
