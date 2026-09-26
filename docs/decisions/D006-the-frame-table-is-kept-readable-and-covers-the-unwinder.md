# D006 - The frame table is kept, readable, and covers the unwinder

**Status:** decided
**Date:** 2026-09-26

A title linking `oops-libunwind.mk` keeps `.eh_frame` and `.eh_frame_hdr` (`selfish#D105`),
places them after `.rodata` so they land in the readable segment beside `.gcc_except_table`
(`link/eh-frame.ld`), and builds libunwind with `-fasynchronous-unwind-tables` so
`_Unwind_RaiseException` and `_Unwind_Resume` have frame entries. `cxx-throw`'s `make check`
asserts each property separately: the range is non-empty, it is outside the text segment,
and it shares a segment with `.gcc_except_table`.

**Why:** the platform maps a title's text execute-only, so a table placed after `.text` faults
on its first read. Phase 1 of an unwind first steps past `_Unwind_RaiseException`'s own frame;
without an entry for it the walk ends at once and no handler, `catch (...)` included, is
consulted. clang emits no asynchronous tables for C and `-fno-exceptions` C++ on this target.

**Rejected:**
- `INSERT AFTER .text`: the table sits in execute-only memory.
- Relying on default table generation: libunwind's own frames get none.
- One presence check on the `__eh_frame_*` symbols: present and non-zero, the range was empty.
