# D005 - Exceptions come from our own libunwind

**Status:** decided
**Date:** 2026-09-26

A title that throws links libunwind and libc++abi built from the llvm-project pin in
`src/oops-deps/libcxx` (`oops-libunwind.mk`, `oops-libcxxabi.mk`) and sets
`OOPS_CXX_EXCEPTIONS = 1` in `common/cxx.mk`. libunwind is built with
`_LIBUNWIND_IS_BAREMETAL=1`: it finds the frame table through the `__eh_frame_*` symbols
`link/eh-frame.ld` defines, not by asking the dynamic linker. Titles that do not throw stay
`-fno-exceptions -fno-rtti`.

**Why:** the platform exports none of the Itanium unwind or C++ ABI symbols, nor
`dl_iterate_phdr`, `_dl_find_object` or the `__register_frame` family; its unwinder is
SEH-lineage, not the ABI clang emits for this target. libunwind and libc++abi are in the same
llvm-project commit as libc++ and the pinned clang, so they add no origin and no pin. The
link-time table is checked by `make check` (the symbols exist or they do not), where a failed
run-time lookup ends in `std::terminate` far from the throw.

**Rejected:**
- The platform's unwinder: absent from every library a title can reach.
- Run-time frame registration: `__register_frame` is not exported, so code loaded at run time
  cannot be unwound through.
- `-fno-exceptions` everywhere: the GL CTS reports every failure by throwing.
