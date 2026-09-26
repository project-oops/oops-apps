# FreeType

Font rasterisation, pinned at `VER-2-13-3`. Extreme Tux Racer uses it directly and ships
TrueType fonts in its data, and Neverball reaches the same library through SDL2_ttf, which is why
it lives here rather than in either title.

Same arrangement as every dependency here: an origin, our build glue, and nothing of upstream's
committed. `patches/` is empty; FreeType is plain C that asks its platform for very little. It
compiles for `x86_64-unknown-freebsd -ffreestanding` against `oops-sdk`'s libc and no host
headers.

## Source list

FreeType builds every font format it knows: Type 1, CFF, CID, PCF, BDF, PFR, Windows FNT, plus
validators and a glyph cache. `oops-freetype.mk` compiles the core, the `sfnt`/`truetype` driver
pair, both rasterisers and the autohinter - upstream's own minimal TrueType set.

The list is trimmed for surface rather than size: a format no title opens is still a parser
reading untrusted bytes, and still something to carry across a bump.

## What it needs from the SDK

| Wanted | Answer |
|---|---|
| `<setjmp.h>` | SDK header. Declarations over the platform's own - `setjmp` cannot be written in C, and the platform's FreeBSD-derived libc already has it. `jmp_buf` is FreeBSD's amd64 layout because the platform writes it. |
| `getenv` | SDK function answering NULL. A payload has no environment block, so every name is unset. |
| `FP_NAN` and its siblings | SDK macros, shared with libc++ |

`ftstdlib.h` includes `<setjmp.h>` unconditionally for the `ft_jmp_buf` type, so every
translation unit needs the header, while the functions are called only from the gzip and LZW
decompressors. A TrueType build compiles against the header and never references the symbols.
A title that turns on compressed font support depends on `setjmp`/`longjmp` binding on the
hardware, and the SDK header says so.
