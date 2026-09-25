# Porting Q3Rally

## Where it stands

**Nothing is built. One thing is measured, and it is the one that usually decides a port:**

| | |
|---|---|
| GL entry points `renderergl1` calls | **72** |
| of those, defined by `oops-gl` | **67** |
| real gaps | **1** — `glGetStringi` |
| SDL | **SDL2**, which this collection already vendors |
| language | C, no C++ anywhere in the engine |
| game data | **ships with upstream** — 629 MB under `baseq3r/`, no Quake 3 purchase |

## The census, and the mistake it nearly recorded

A first pass grepped the sources for `gl[A-Z]*` and found 23 names, 11 of them "missing". Every one
of those 11 was ioquake3's own — `glConfig`, `glState`, `glIndex`, `glMatrix`, `glStateBits`,
`glWrapClampMode`, and a typo of their own making, `glDrawElemet`.

**ioquake3 does not call GL directly.** Every entry point goes through a function-pointer table, so
the code says `qglBegin`, not `glBegin`. Censusing `qgl[A-Z]*` instead and stripping the `q` gives
the real surface: 72 names.

That is the same error shape as Ship of Harkinian's vertex-array question — a grep measuring
something adjacent to the thing you wanted. Worth re-reading before the next census.

## The one real gap

`glGetStringi` is GL 3.0's indexed string query, used to enumerate extensions when the context is
core profile. ioquake3 reaches for it only after deciding the context is core; on a compatibility
context it uses `glGetString(GL_EXTENSIONS)` instead. So the likely answer is that `oops-gl` never
advertises a core profile and this is never called - but that is a prediction, and the way to settle
it is to compile `renderergl1` and see, not to argue about it here.

`glLockArraysEXT` and `glUnlockArraysEXT` are **not** counted as gaps. They are
`GL_EXT_compiled_vertex_array`, an optional hint, and ioquake3 calls them only if the driver
advertises the extension. `oops-gl` will not.

## What has not been measured

- **The QVM.** `game`, `cgame` and `ui` are compiled to ioquake3 bytecode and run by a virtual
  machine. There are two ways to run them and both look open: the pure interpreter needs no
  executable memory at all, and the x86 JIT (`VM_CompileX86`) would need a mapping this SDK already
  has an interface for, `oops/jit.h`. A third option upstream supports is building the three
  modules as native code linked straight in, which is likely the cleanest on a console.
- **What upstream bundles that we already vendor.** `engine/code` carries its own SDL2, zlib,
  libogg, libvorbis, libtheora, opus, opusfile, jpeg, curl and OpenAL. Some of those duplicate
  `oops-deps`; a duplicate `inflate` in one link is a symbol that resolves to whichever the linker
  reached first, which is the trap `oops-stormlib.mk` records. Decide per library which copy wins
  before writing a source list.
- **Networking and `curl`.** A single-player build should not need either. Which parts of the engine
  fail to compile without them is unknown.
- **The icon.** `assets/icon0.png` does not exist yet; `../README.md` has the recipe and the
  512x512 requirement.

## Why this one first

Of the candidates surveyed with it, this is the only one already on SDL2. Bugdom and Bugdom 2 have a
cleaner GL story - 100% covered, not 99% - and are blocked behind porting SDL3, which is a second
SDL backend rather than a version bump. Being pure C also keeps it clear of the libc++ questions the
C++ titles carry.
