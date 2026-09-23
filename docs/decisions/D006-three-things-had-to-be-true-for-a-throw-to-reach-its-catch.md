# D006 - Three things had to be true for a `throw` to reach its `catch`, and each one hid the next

**decided** · 2026-09-23 (measured: `cxx-throw` reports `passed=5 total=5` on CXTH00001, FW 12.40,
after four hardware probes)

[D005](D005-exceptions-come-from-our-own-libunwind-and-one-measurement-gates-it.md) settled that
a title brings its own libunwind, and the build linked cleanly on 2026-09-21. It did not throw.
Every `throw` reached `std::terminate` without entering any handler - **`catch (...)` included**,
which is the detail that made this take four runs to unpick, because a `catch (...)` that does
not catch reads as a broken personality routine and is nothing of the kind.

Three separate defects, in series. Each one fully explains the symptom, so fixing one moved the
failure rather than removing it, and each was invisible to the check that should have caught it.

## 1. The frame table was discarded

`selfish/link/native_eboot.ld` listed `*(.eh_frame .eh_frame_hdr)` in `/DISCARD/`, and a discard
beats the `KEEP` in `oops-deps/libcxx/link/eh-frame.ld`. `__eh_frame_start` and `__eh_frame_end`
came out equal: a zero-byte table. Fixed in `selfish#D105`.

**What let it through:** `make check` asserted the four `__eh_frame_*` symbols were *present and
non-zero*. They were. `start` and `end` simply held the same address. The check now asserts the
range is wider than zero and prints the width.

## 2. The frame table was in execute-only memory

With the table kept, the title died on `exception: 0xa0020328 (SYSTEM_XO_VIOLATION)` at
`fault address: 00000000004403fc (xotext:"eboot.bin"+0x403fc)` - `0x403fc` being
`__eh_frame_end` / `__eh_frame_hdr_start` exactly, the index's first byte, faulting on the first
load.

**This platform maps a title's text execute-only.** `eh-frame.ld` said `INSERT AFTER .text`,
which put both sections in the `FLAGS(5)` PT_LOAD. The unwinder found them and was not allowed to
read them. Now `INSERT AFTER .rodata`, which lands them beside `.gcc_except_table` in the
readable segment.

**What let it through:** nothing checked. `.gcc_except_table` was never affected - nothing names
it, so the default layout had always put it with `.rodata` - and that asymmetry is precisely what
disguised the whole problem. The LSDA was always readable and the frame table never was, so the
failure looked like handler-table logic rather than an unreadable page. There is now a check that
the frame table is not in the same segment as `.text`, and *is* in the same one as
`.gcc_except_table`.

## 3. The unwinder's own frames were not walkable

Readable, complete, 446 FDEs covering every frame the unwind walks - and still `terminate` on the
first throw, with `basic` named by the per-check log.

`_Unwind_RaiseException` had no FDE. Neither did `_Unwind_Resume`. Every libc++abi function had
one.

**Not throwing is not the same as not being unwound through.** `unwind_phase1`'s first act is a
DWARF step past `_Unwind_RaiseException`'s own frame - upstream's comment reads *"skip over first
which is `_Unwind_RaiseException`"*. With no FDE that step returns end-of-stack, phase 1 concludes
there are no frames to search, and no handler of any kind is consulted. `_Unwind_Resume` is the
same on the cleanup path.

The cause is a default, not a mistake in the source: clang for `x86_64-unknown-freebsd` with
these flags emits **no** asynchronous unwind tables, which is why a pure C title here has no
`.eh_frame` at all. libc++abi is compiled *with* exceptions and got tables anyway; libunwind is C
and `-fno-exceptions` C++ and got none. `oops-libunwind.mk` now passes
`-fasynchronous-unwind-tables`. The table went from 446 FDEs to 518.

## What this costs

`.eh_frame` is ~22 KiB on `cxx-throw`. It is charged only to titles that link
`oops-libunwind.mk`, which is `cxx-throw` and nothing else today: the four `local_tls.ld` titles
link no unwinder and keep discarding the section, which `selfish#D105` records with the
measurement (1.33 MB on `mesa-demos`).

## Two things that are still true and are not bugs

- **The loader still prints `WARNING: corrupted eh_frame_hdr or eh_frame in /app0/eboot.bin`**,
  on the run that passes 5/5. It was tempting to read that line as this problem reporting itself,
  and it is not - exceptions work with it present. Most likely it wants a `PT_GNU_EH_FRAME`
  segment, which this linker script does not emit. **Do not chase it on the strength of the
  wording**; it cost time here.
- **`.init_array` is not walked in a plain C++ title.** `common/cxxrt.cpp` installs a
  `set_terminate` handler from a namespace-scope object and it never ran - libc++abi's own
  default handler printed the message instead. oops-mesa titles call `oops_mesa_run_init_array`
  for exactly this reason; a title without it gets no static constructors. `cxx-throw` installs
  its handler from `cxx_throw_run` instead, which is the honest placement for a probe anyway.

## The general lesson, which is about checks rather than about unwinding

Every one of the three passed the check that existed. Present-and-non-zero is not non-empty;
non-empty is not readable; readable-and-complete is not walkable-from-the-frame-that-starts-the-walk.
`make check` now asserts all four properties separately, because they fail for different reasons
and each failure wants a different answer. The per-check log line in `probe.cpp` is part of the
same argument: the first three runs could not say *which* of five checks died, and three of them
throw the same exception type.
