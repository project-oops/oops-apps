# D005 - Exceptions come from our own libunwind, and one measurement gates the build shape

**assumed** · 2026-09-21 (the route is settled by measurement; the *configuration* of it waits
on a hardware answer that is filed and not yet returned)

`common/cxx.mk` is `-fno-exceptions -fno-rtti`, and its own header says why: the C++ titles in
the queue were counted and Extreme Tux Racer has 0 `throw` and 0 `dynamic_cast`. That remains
right for them. This entry is about the titles it is not right for - Armagetron's 68 throws and
167 dynamic casts, and the GL 3.3 CTS, whose every failure path is a throw
(`oops-mesa` worklog 067).

## The platform does not have an unwinder we can use

obSCEne swept all 19 Itanium unwind and C++ ABI symbols across `libkernel`,
`libSceLibcInternal` and `self` and found **every one absent at `0x0`**
(`REQ-20260921T0953Z-e3f7`, sweep `20260921-run17`, rows at
`obscene/reports/hardware/20260921-run17-eboot.obs.log:2170-2286`). The platform runs an
MSVC-lineage SEH unwinder, which is not the ABI clang emits for this target.

So there is no borrowing this. A title that throws brings its own unwinder or does not throw,
and `-fno-exceptions` is the honest expression of the second option rather than a preference.

## The route: libunwind from the pin already here, not a new dependency

`src/oops-deps/libcxx` already pins llvm-project at `llvmorg-21.1.8`
(`2078da43e25a4623cab2d0d60decddf709aaea28`). libunwind and libc++abi are in that same commit,
so they cost a wider `UPSTREAM_SPARSE` and nothing else - no second origin, no second pin, no
second provenance question. That is why the sparse list gained `libunwind` (and `libc`, for a
header `charconv.cpp` needs across llvm-project's own subtree boundary).

It is also why this waited for clang 21 (`oops-mesa#D013`): libc++ 21's headers want a compiler
at least as new as themselves, and the authoritative container's clang is built from
`2078da43e25a` - the same commit. The compiler and the sources it compiles are one revision.

## What was measured, and the one thing that is not settled

libunwind was compiled for `x86_64-unknown-freebsd` against oops-sdk's freestanding libc, under
clang 21:

- **9 of its 10 sources compile.** The two with real content - `UnwindLevel1.c` and
  `UnwindLevel1-gcc-ext.c` - **define all 14 of the `_Unwind_*` symbols the platform lacks**, as
  global text. The platform's gap is fillable.
- **`libunwind.cpp` does not compile**, and it fails in one place: `AddressSpace.hpp`, at
  `dlfcn.h` and then, with `_LIBUNWIND_USE_DLADDR=0`, at `link.h`. Those are `dladdr` and
  `dl_iterate_phdr` - **how the unwinder finds `.eh_frame_hdr` at run time**, and nothing else in
  the library reaches for them.
- That file holds every `__unw_*`, and the compiled half references fourteen of them. So the
  objects are not linkable without it. **The unwinder is one file short, and that file's only
  obstacle is dynamic-linker introspection.**

Two more findings worth keeping, because both cost time:

- **`-nostdlibinc` is not optional**, exactly as `src/oops-deps/libcxx/oops-libcxx.mk` already
  says for libc++. Without it the build machine's `/usr/include` is found for a FreeBSD
  freestanding target and glibc's `features-time64.h` fails on `bits/wordsize.h`.
- **libunwind needs `<inttypes.h>`**, which oops-sdk's freestanding libc does not have. It uses
  only the `PRI*` macros, in logging paths. That is a small header this dependency should carry
  beside the other gap-fillers it already has, in the same way it carries `__config_site` -
  rather than a change to oops-sdk, which is a different repository's call.

## Why this is `assumed` and what settles it

`REQ-20260921T1830Z-b4d1` is filed on the obSCEne bus: are `dl_iterate_phdr`, `_dl_find_object`
or the `__register_frame*` family callable in **any** library, on the legs `unwind-abi` used?
`dl_iterate_phdr` is already measured absent in `libSceLibcInternal` - but only there, and
`libkernel` and `self` were never asked.

The answer chooses between two different builds:

- **Some route answers** - `AddressSpace.hpp` configures stock and the unwinder is ordinary.
- **None answers** - the title must register its own `.eh_frame` at start-up through
  `__register_frame`, and libunwind is built with the frame APIs compiled in instead.

Choosing between those by guessing is how a runtime that half-supports exceptions gets shipped,
which is the one outcome `common/cxx.mk` was written to avoid. So the variant is not added to
`cxx.mk` until the answer is in: an `-fexceptions` mode that links an unwinder which cannot find
its tables would fail on the first `throw`, a long way from its cause, and would look like a
title bug.

**Nothing here is claimed to work on hardware.** No console has run any of it. The proof owed is
a title that throws across a shared-object boundary and catches, on the hardware, and that run
has not happened.
