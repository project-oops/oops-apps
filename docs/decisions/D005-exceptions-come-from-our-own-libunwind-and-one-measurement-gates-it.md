# D005 - Exceptions come from our own libunwind, and one measurement gates the build shape

**decided** · 2026-09-21 (built and linked the same day; the entry was drafted `assumed` waiting
on a bus answer, and then the baremetal route removed the dependency on it - see "How the open
question stopped being a blocker")

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

## The route, and the measurement that came back and confirmed it

`REQ-20260921T1830Z-b4d1` asked the obSCEne bus whether `dl_iterate_phdr`, `_dl_find_object` or
the `__register_frame*` family are callable in **any** library. The route below was chosen
before the answer, because it does not depend on one.

**The answer arrived the same day and there was no other route.** Target Run 26
(`obscene/reports/hardware/20260921-run26-eboot.obs.log`, FW 12.40) swept all eight symbols
across `libkernel`, `libSceLibcInternal` and `self`: **every one absent at `0x0`**. Not the
program-header iteration (`dl_iterate_phdr`, `_dl_find_object`) and not the explicit
registration API (`__register_frame`, `__register_frame_info` and their deregister forms)
either. obSCEne's own conclusion is the same as this entry's: a title throwing C++ exceptions
has to carry its frame information itself.

So what follows was the only option, not the cautious one of two.

`_LIBUNWIND_IS_BAREMETAL=1` replaces the dynamic-linker lookup entirely. Instead of asking the
platform where the frame table is, libunwind reads four symbols the *link* provides -
`__eh_frame_start`, `__eh_frame_end`, `__eh_frame_hdr_start`, `__eh_frame_hdr_end` - which
`link/eh-frame.ld` defines by bracketing the sections the compiler already emits. A run-time
question about the platform becomes a link-time fact about the title.

That is the better answer regardless of how the bus replies, and it is worth saying why: the
stock route's failure mode is silent. An unwinder that cannot find its tables computes an empty
range, treats the first frame as the end of the stack, and calls `std::terminate` - a long way
from the `throw`, and indistinguishable from a title bug. The linker-script route cannot do
that: the symbols are either in the ELF or they are not, and `make check` looks.

What the bus answer changes, now that it is in: **it removes the alternative rather than the
route.** There is no `__register_frame` to fall back to, so a title that loads code at run time
and wants to throw through it has no mechanism at all here - the frame table is fixed at link
time or it does not exist. Nothing queued needs that, and this is the sentence to come back to
if something does.

## What was built and verified

All of it is in the tree and in the gate:

- `oops-libunwind.mk` builds all 10 libunwind sources. The two with content define **all 14 of
  the `_Unwind_*` symbols** REQ-e3f7 found at `0x0`.
- `oops-libcxxabi.mk` builds 16 of 19 libc++abi sources, excluding three with stated reasons,
  and defines the other **5**: `__cxa_throw`, `__cxa_begin_catch`, `__cxa_end_catch`,
  `__cxa_allocate_exception`, `__gxx_personality_v0`. 14 + 5 is the whole set.
- `common/cxx.mk` gained `OOPS_CXX_EXCEPTIONS = 1`, which turns on `-fexceptions -frtti` and
  defines `OOPS_CXX_EXCEPTIONS` so `cxxrt.cpp` stops shadowing the four symbols libc++abi
  provides properly.
- `src/oops-utilities/cxx-throw` is the probe: five checks - basic round trip, derived caught as
  base, destructors run while unwinding, rethrow, and a multi-frame walk. It **passes 5/5
  against the build machine's own C++ runtime**, which is what says the checks themselves are
  sound rather than unpassable.
- Its target ELF builds and carries `__cxa_throw`, `__cxa_begin_catch`,
  `__cxa_allocate_exception`, `__gxx_personality_v0`, `_Unwind_RaiseException`, `_Unwind_Resume`,
  `__eh_frame_start` and `__eh_frame_end`, all confirmed by `nm`.

**`nm` and not the exit code, deliberately.** `app.mk` links with
`--unresolved-symbols=ignore-all`, so a payload that failed to link an unwinder still links and
every one of those symbols resolves to zero. The build succeeding is no evidence; the symbol
table is. `make check` fails on a missing one.

## What is still owed

**No console has run this.** The proof owed is the probe reporting `passed=5 total=5` over klog
on the hardware, and that run has not happened - so what is established is that a title *can
carry* a working unwinder, not that the platform lets one run. Those are different claims and
this entry does not blur them.

One known limit is already visible without hardware: `oops_malloc` returns 8-byte-aligned
memory (its 24-byte header sits on a 16-aligned block), and `__cxa_allocate_exception` wants 16.
So `aligned_alloc` refuses and libc++abi takes its small static fallback buffer on every throw.
That works and does not scale; the fix is a `max_align_t`-aligned heap, which is oops-sdk's and
is on the bus. `cxxrt.cpp` says so where the constant lives.
