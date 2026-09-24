# Porting Armagetron Advanced

Pinned at `v0.2.9.3.0` (`036daaf3`), the revision `upstream.lock` names.

## Where it stands

**A scaffold. Nothing here has been measured.** The lock is verified and the layout is in place;
every claim below is inherited from the survey in [`../README.md`](../README.md) and is waiting
for somebody to read the source.

That distinction matters more than it usually would, because **the survey's method has a known
blind spot**. It classifies a title by grepping for `glBegin`/`glVertexPointer` against
`glCreateShader`/`glUseProgram` and reading the shading-language version - which answers "what is
in the tree" when the question is "what executes". SuperTux was sorted into the GL 2.0 slot on
that basis and turned out to run a fixed-function backend that loads none of the shaders it
ships. Armagetron's row has not been re-checked against that lesson.

## What the survey says

| | |
|---|---|
| Renderer | 23 `glBegin` call sites, no shader calls - so `gl1/` |
| Language | C++, 189 files |
| Ordering | **last**, not second, and the reason is below |

## Why it is last rather than second

Armagetron is the collection's most demanding C++ title, and the cost is not the graphics.

- **Exceptions and RTTI.** The plan the collection works to is that a freestanding C++ target
  gets `-fno-exceptions -fno-rtti` and runtime stubs for `new`, `delete` and static-init guards.
  Armagetron is the title that forced a correction to that plan: it is a program that *uses*
  both, so it needs a runtime with them working rather than switched off.
- **Boost.** Part of it, which is a dependency outside every lock file in `src/oops-deps/`.

`../README.md` puts it plainly: that is "a larger project wearing the same name". Extreme Tux
Racer is nominated to pay the C++ runtime cost first precisely because it needs neither.

**oops-sdk's own position is better than that summary suggests**, and is worth checking before
the ordering is taken as settled: C++ exceptions are measured working on this target, 5 of 5 on
firmware 12.40. Whether that covers what Armagetron needs is exactly the sort of thing that
should be measured rather than assumed in either direction.

## The first job

Read `upstream/src/` and answer three questions, in this order:

1. **Which renderer actually runs.** Not which GL calls exist - which are reached. That is the
   SuperTux lesson and it costs an hour.
2. **What the C++ runtime is actually asked for.** `catch` sites, `dynamic_cast`, `typeid`, and
   what throws across what boundary. The plan's claim is that this title needs more than the
   others; that claim has never been measured.
3. **How much of boost.** A header-only subset is a different problem from a compiled one.

Until (1) is done, this title's slot in `../README.md` is provisional.
