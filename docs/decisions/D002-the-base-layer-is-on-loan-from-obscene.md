# D002 - The base layer is on loan, and belongs in the SDK


**decided** · 2026-09-04

Porthole needs a freestanding runtime, the syscall interface and kernel read/write before it
can open a socket or map a buffer. Those arrived in `common/`, carried in with it from obSCEne
(`src/common/{freestd,syscall}` and `src/injector/krw`), because that is where they were
written.

That is not their home. Every target payload needs them - which is the definition of what
belongs in **oops-sdk**, not in an app and not duplicated across apps. `freestd` and `syscall`
are plainly SDK-shaped and should promote there, at which point an app includes `oops/…` for
them the way it already does for the display.

`krw` is the same in principle and has a complication in practice: oops-sdk already carries an
`escalate` module in the kernel-access space, and whether `krw` is the primitive that belongs
beneath it, a duplicate of part of it, or a separate thing, is a question that needs the
`escalate` code read to answer - and that code is deliberately walled off. So `krw` moves when
whoever owns `escalate` reconciles the two; until then it rides here with Porthole.

**It is a copy, and honestly a duplicate for now.** obSCEne keeps its originals - its own
injector, loader and process control are built on the same `freestd`/`syscall`/`krw`, so those
files cannot leave it - and this repository carries a copy. Two copies of one thing is exactly
the drift the collection refuses, and the resolution is not to pick one of these two homes but
to move the layer to the one that is neither: **oops-sdk**, where every target payload,
obSCEne's and this app's alike, consumes it from a single place. This note is here so the debt
is visible and counted until that move is made, rather than settling in as though it were the
arrangement.

---

**resolved** · 2026-09-04

The debt is closed. `freestd`, `syscall`, and `krw` have been promoted to `oops-sdk`:
- `include/oops/freestd.h` and `src/system/freestd.c`
- `include/oops/syscall.h` and `src/system/syscall.c`
- `include/oops/krw.h` and `src/system/krw.c`

`krw` is reconciled as a dedicated low-level primitive module alongside `escalate`, preserving
the payload-argument-driven kernel memory and process control contract. All consumer sources in
both obSCEne and oops-apps now include `oops/*` and compile the SDK sources via `oops-sdk.mk`.
`oops-apps/common/` and `obscene/src/common/` have been removed entirely, leaving zero duplicate
source copies across the repositories.
