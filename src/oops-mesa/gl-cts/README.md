# gl-cts

The Khronos OpenGL Conformance Test Suite, built for the hardware.

## About

`upstream.lock` pins the suite's revision. **This is the only thing in the collection that can
use the word "conformance"**: everything else here measures what this project wrote against what
this project expected, and the worth of a CTS result is that the tests are somebody else's.

- **Fetched, not vendored** - only `shim/`, `tools/`, the Makefile and the lock live here.
- **No patches.** If upstream needs changing to run here, that is a finding about the port.
- **The subset is chosen at run time**, through `--deqp-case`, never by compiling a reduced
  binary: a suite pruned at compile time is a curated one.

`build/gl-cts.elf` carries the dEQP framework, the test sources under
`external/openglcts/modules`, dEQP's `modules/glshared` and `framework/randomshaders`, the
platform layer and the GL entry-point table. `make imports` resolves its imports and `make title`
packages it. `shim/gl_cts_registry.cpp` lists the test packages that register and names the ones
it leaves out.

## A hosted title

`OOPS_RENDERER = mesa` makes this a **hosted** title, and `common/app.mk` says what that means at
its `USE_MESA` block: the target C library comes from the Mesa sysroot, and oops-sdk's
freestanding libc headers collide with it - the sysroot's `__clock_t` is `int` where oops-sdk's
`clock_t` is `int64_t`. So `app.mk` empties `OOPS_SDK_LIBC_INCLUDE` for these titles. The sysroot
is a full FreeBSD header set - `unistd.h`, `signal.h`, `pthread.h`, `dirent.h`, `sys/stat.h`, a
real `libm.a`.

`oops-deps/libcxx/include/freestanding/` holds shims standing in for a C library that does not
exist freestanding - `locale.h`, `runetype.h`, `nl_types.h`. A hosted build does not put that
directory on the path, because there they would shadow FreeBSD's real headers.

libc++'s headers precede the C library's. libc++ ships its own `<math.h>`, `<string.h>` and
relatives - wrappers that `#include_next` the C library's and add the C++ overloads - and
`<cmath>` checks that its wrapper was the one found.

The generated GL/EGL wrappers are checked in, under `wrapper/`.

## Undefined symbols

A hosted title is linked `--unresolved-symbols=ignore-all`, because its C library is resolved at
load, so a successful link does not mean every symbol is answered; `nm -u` is the check. Every
undefined name is a libc or `sce*` import the loader answers, and none is a C++ symbol. C++ names
no platform library can resolve are defined in the shims:

| symbol | answered by |
|---|---|
| `tcu::ImageIO::loadPNG`, `loadPKM` | `shim/gl_cts_gaps.cpp` - throws `NotSupportedError`; `tcuImageIO.cpp` is excluded because libpng is not ported |
| `std::mutex::lock`, `unlock`, `~mutex` | `shim/gl_cts_gaps.cpp` - no-ops. `_LIBCPP_HAS_THREADS` is 0, so libc++'s `mutex.cpp` is not built |
| the `eglu::` entry points | `shim/gl_cts_gaps.cpp` - throws. `glcConfigListEGL.cpp` references them and cannot reach them |
| the `glc::spirvUtils` entry points | `shim/gl_cts_spirv_stub.cpp` - throws. glslang and spirv-tools are not ported |

The `eglu` entry points are unreachable: `getDefaultEglConfigList` opens with
`dynamic_cast<tcu::EglPlatform&>(platform)`, this platform is not one, and
`glcConfigListEGL.cpp:165` turns the `bad_cast` into a `tcu::Exception` that
`getDefaultConfigList` catches. They are still defined, because unreachable and absent are the
same thing to this linker, and `make imports` cannot place a C++ name that belongs to no platform
library.

Nothing in the test modules uses `std::` threading; the `std::mutex` references are the
framework's.

## The platform layer

`shim/tcuOopsPlatform.cpp` is the only code this port writes; everything else built here is
upstream's, which is what makes a CTS result worth having.

It is a `tcu::Platform` exposing a `glu::Platform`, one `glu::ContextFactory`, and a
`glu::RenderContext` whose four methods are the GL version, the function table, the render
target, and a present. `oops_gfx_create` opens the display and makes a context current in one
call. EGL and Vulkan are left to the base class, which reports them unsupported.

### The function loader

dEQP fills `glw::Functions` through a loader shaped like `dlsym`. This platform has no runtime
symbol table, and Mesa does not export `_glapi_get_proc_address` in this build, so the lookup
happens at link time: `tools/gen-gl-loader.py` reads the names out of upstream's own
`glwInitGL33.inl` and `glwInitExtGL.inl`, intersects them with what `nm` says Mesa's
`libglapi_bridge.a` defines, and emits the table.

**A null is a real answer**: `glu::ContextInfo` checks a pointer before use, and a test whose
entry point is absent reports `NotSupported`. A loader that returned something plausible would
turn that into a fault inside the test. Re-run the generator after any change to
`libglapi_bridge`; the count and the full absent list go to stderr.
