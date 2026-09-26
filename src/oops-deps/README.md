# Dependencies

Third-party libraries a ported title needs, each **pinned to a commit and never committed here**.
Same shape as a title in `../oops-titles/`: an origin, our patches, our build glue, and nothing
of upstream's tracked. The pinned revision is in each directory's `upstream.lock`.

These are not written here. `oops-sdk#D010` settled that for SDL and the reasoning holds for all
of them: a library upstream maintains is pinned and bridged, not reimplemented. Our side of each
is a `.mk` file, sometimes a config header its build system would have generated, and sometimes
a patch.

## Contents

| | Ours | For |
|---|---|---|
| [`sdl2`](sdl2/) | backend files and a patch | SDL2 titles |
| [`sdl12-compat`](sdl12-compat/) | a patch | an SDL 1.2 title |
| [`sdl2-image`](sdl2-image/) | a `.mk` | ETR, SuperTux (PNG and JPEG) |
| [`sdl2-mixer`](sdl2-mixer/) | a `.mk` | ETR |
| [`sdl2-ttf`](sdl2-ttf/) | a `.mk` | Neverball |
| [`freetype`](freetype/) | a `.mk` | ETR, and SDL2_ttf beneath |
| [`libcxx`](libcxx/) | `__config_site` and headers | any C++ title |
| [`libpng`](libpng/) | one changed line in upstream's config | Neverball |
| [`zlib`](zlib/) | a `.mk` | libpng |
| [`libvorbis`](libvorbis/) | a `.mk` | Neverball |
| [`libogg`](libogg/) | a generated header | libvorbis |
| [`libjpeg-turbo`](libjpeg-turbo/) | config headers | Neverball, and SDL2_image beneath |
| [`openal-soft`](openal-soft/) | config headers, a thread-local patch | SuperTux, SuperTuxKart |
| [`glm`](glm/) | a `.mk` | SuperTux |

The full set is the directory listing. All of them compile freestanding for
`x86_64-unknown-freebsd` against `oops-sdk`'s libc, with `-nostdlibinc` so the build machine's
headers cannot leak in.

## Rules for a dependency

**Peel the tag.** `git ls-remote refs/tags/X` gives the tag object for an annotated tag, and the
commit is behind `^{}`. A tag-object hash in a lock checks out a different tree.

**A duplicate is usually the right answer.** The collection carries two PNG decoders and two
Vorbis decoders: `oops_png_decode` and `stb_vorbis` answer "decode this file", which is all
SDL_mixer and a payload with a texture need; libpng and libvorbisfile serve programs written
against their APIs.

**Trim for surface, not size.** MP3, FLAC, Opus, MIDI and MOD are off in the mixer; the JPEG
encoder and arithmetic coder are off; FreeType builds TrueType only. A decoder no title has a file
for is still a parser of untrusted input, and still something to carry across a bump.

**Read upstream's own build for the file list.** `lib/*.c` in libvorbis sweeps in two standalone
tuning programs with their own `main()`; `libvorbis_la_SOURCES` in its `Makefile.am` names the
library files.

## What the dependencies need from oops-sdk

Each of these lives in the SDK rather than being shimmed per dependency:

`errno.h` over the platform's `__error` · `setjmp.h` over the platform's · `getenv`/`setenv`/
`unsetenv` · `strtoll`/`strtoull`/`strtold` · `alloca` · the `FP_*` classification macros and
`fpclassify` · `rint`/`nearbyint`/`lrint` and their relatives · and `extern "C"` on every libc
header, without which a C++ caller mangles every name.

`gmtime` and the calendar functions are absent: `time()` on this target is not a wall clock, and
`oops-sdk`'s `<libc/time.h>` prefers a failed link to a program that prints 1970. libpng's tIME
chunk support is off for the same reason.
