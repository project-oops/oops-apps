# gl2-probe — reference

The check taxonomy and design notes for the OpenGL 2.0 conformance probe. The
[README](../README.md) is the overview; this is what the suite actually measures.

Run the checks with `make check`, or `./bin/oops-apps check gl2-probe` from the repository root.

## What it measures

A probe exists to compare two implementations of the same specification. `gl1-probe`'s pair is
the software rasteriser and the console; this one has the software reference and, when there is
one, the console's GL 2.0 back end. The suite in `gl2_probe.c` is written as a shared suite with
a thin runner for exactly that reason — the day there is a hardware path, a `gl2_probe_main.c`
goes beside `gl1_probe_main.c` and these checks run on it unchanged.

The checks fall into groups, and the split is deliberate:

| Group | What it holds | Why |
|---|---|---|
| The object model | 11 checks: **version gating**, the shared name space, compile and link status, deferred deletion, uniform and attribute locations, the limits | Proves the API is wired. These answer the same on any implementation — they read variables — so they are the minimum rather than the bulk |
| The vertex stage | 6 checks: a program draws, the `mvp` uniform transforms, attribute arrays and current values, the fixed-function built-ins, a vertex-only program | Each ends in a pixel |
| Varyings | 3 checks: interpolation, **perspective correctness**, several at once | The perspective one is the whole reason the group exists |
| The fragment stage | 8 checks: `gl_FragCoord`, `discard`, `gl_FragDepth`, `gl_FrontFacing`, a fragment-only program, two texture lookups, derivatives | |
| The language | 10 checks: matrix arithmetic, swizzles, control flow, user functions, the built-in library, `mod`, the relational built-ins, short-circuit evaluation, constructors, and **GLSL 1.20** | A compiler that got one of these wrong would pass every check above it |
| What still applies | 5 checks: the depth test, blending, culling, the scissor and the colour mask, and that a draw is deterministic | A programmable pipeline does not replace the per-fragment operations, and an implementation that ran a shader *round* them would look right until something was blended |
| The rest of GL 2.0 | 3 checks: separate stencil state, a blend equation per channel group, `glDrawBuffers` | The three parts of the version that are not about shaders at all, and the ones a reader forgets are in it |

## Checks worth naming

**`varying-perspective`.** A varying is linear in clip space, not in screen space. The quad it
draws has `w = 1` on its left edge and `w = 3` on its right, so the value at the screen midpoint
is 0.25 and not 0.5. An affine interpolator answers 128 where this expects 64, and *every other
check in the suite passes either way* — a quad drawn flat to the viewer cannot tell them apart,
and that is the shape most test scenes have.

**`mod-and-int-divide`.** GLSL's `mod` is a floored modulus and C's `fmod` is truncated:
`mod(-1.0, 4.0)` is 3 in a shader and -1 in C. A shader tiling a texture by `mod(uv, 1.0)` wraps
correctly with one and mirrors at the origin with the other, which looks like an art problem.

**`short-circuit`.** `&&` and `||` must not evaluate their right operand when the left decides
the answer, which a shader relies on to guard a divide. The right operand here writes through an
`out` parameter, so an implementation that evaluated it anyway leaves a mark — a pure function
would have been no test at all, because there would be nothing to see either way.

**`version-gating`** is the only check that takes something away. A context has the entry points
its version defines and no others, so this narrows the suite's own context to 1.5 mid-run,
requires the whole programmable surface to be refused — `GL_INVALID_OPERATION` from a call,
`GL_INVALID_ENUM` from a query, and the query's destination left alone — and then widens it back,
because a claim is a property of the context rather than a one-way switch. It also requires
GL 1.x to be *untouched* by the narrowing, which is the half that says the gate is on the right
boundary.

**`glsl-120`** checks that the two dialects are actually two. `2 * 0.25` is 0.5 under
`#version 120` and a compile error under `#version 110`, because 1.20 converts `int` to `float`
implicitly and 1.10 converts nothing — and it measures the result as a colour, because a front
end that accepted the conversion and an interpreter that then truncated the answer to 0 would
both pass a check that only compiled. It also requires the 1.10 half still to *fail*.

**`separate-stencil`** is a whole technique rather than a rule. It draws the shadow-volume idiom:
two quads of opposite winding in one draw, incrementing where a front face passes and
decrementing where a back one does, so the stencil returns to its clear value everywhere both
were drawn. An implementation carrying one stencil state for both faces increments twice and
leaves 2, and the check paints only where the buffer came back to 0.

## What the suite found

Two things, both on its first run and both real:

- **`gl_FrontFacing` was always true.** The fragment stage was taking its facing from
  `prim_polygon_back`, which is two-sided *lighting*'s flag and is set only while GL_LIGHTING and
  GL_LIGHT_MODEL_TWO_SIDE are both on. Every GL 2.0 program has lighting off, so every fragment
  was told it faced forward. It now comes from the triangle's own winding, which is the same sign
  culling reads.
- **A sampler read the wrong target.** The interpreter was handing the texture bridge the front
  end's own type enumerant where the GL one was expected. It compiled perfectly and matched no
  case, so every `texture2D` returned opaque black. Caught by `gl2-cube` before this suite
  existed, and `texture-sampler` is the check that keeps it caught.

## Why there is no payload

There is no GL 2.0 back end for the console. The draw path refuses a draw with a program bound
and logs once, rather than running the fixed-function instruments in its place and putting a
picture on screen that no part of the program asked for — so a payload built today would report
every drawing check as failed, which measures nothing that is not already written down.

`FORMATS` is `check-only` until that changes. The day it does, this directory gains
`gl2_probe_main.c` and an `elf` target, and the table in `gl2_probe.c` is untouched.
