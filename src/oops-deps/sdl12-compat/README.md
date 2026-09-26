# sdl12-compat

SDL 1.2's API over SDL2, pinned, for a title written against SDL 1.2. Extreme Tux Racer is the
consumer.

SDL 1.2 is not implemented here either - see `oops-sdk#D010`. This is the same arrangement as
[`../sdl2`](../sdl2/): an origin, our patches, and nothing of upstream's committed.

## Layout

```
oops-deps/sdl12-compat/
  upstream.lock   where it comes from and exactly which revision
  upstream/       the fetched tree. Never edited, never committed (.gitignore'd)
  patches/        one file, every hunk where upstream asks
  build/          the archives, and the symbol map that makes them work
  oops-sdl12.mk   what a title includes
```

There is no `backend/`. sdl12-compat has no platform layer of its own - it is SDL 1.2's API
expressed in SDL2 calls, and the platform underneath is `../sdl2`'s.

## Symbol renaming

sdl12-compat is built to be a drop-in shared library. It defines SDL 1.2's `SDL_Init` and
reaches SDL2's `SDL_Init` through `dlopen`, so the two never meet in one symbol table. Linked
statically into a payload they collide, from `SDL_Init` down.

sdl12-compat's `SDL_Init` keeps its name, because that is the name the title calls, so SDL2's
copy is renamed. Upstream already chose the new name: `src/SDL20_include_wrapper.h` includes
SDL2's headers with every declaration renamed to `IGNORE_THIS_VERSION_OF_SDL_*`, so the C code
can see both APIs at once, and then `#undef`s the renames so the SDL 1.2 names are free.
Renaming SDL2's linker symbols the same way makes the declarations sdl12-compat already has
resolve to the definitions it wants.

One `objcopy --redefine-syms` pass over the archive does it, from a map `nm` generates, so the
list is read from the build rather than kept by hand. The renamed archive exports no bare `SDL_`
name.

## The patch

| Hunk | What upstream says there | What we do |
|---|---|---|
| Platform arm for loading SDL2 | `#error Please define your platform.` | no library to load; `Loaded_SDL20` carries its own address as a sentinel |
| Symbol binding | - | bind to `IGNORE_THIS_VERSION_OF_SDL_*` directly instead of `dlsym` per name |
| `SDL12COMPAT_getenv_unsafe` | *"a simple `return NULL;` for platforms without an environment table"* | exactly that |
| `SDL12COMPAT_setenv_unsafe` | *"a simple `return;` for platforms without an environment table"* | exactly that |
| `OS_GetExeName` | `#warning Please implement this for your platform.` | empty - see below |

The binding hunk is the only one not invited by upstream, and it is three lines: token pasting
happens before macro expansion, so the same `SDL20_syms.h` table that generates `dlsym` calls
generates direct assignments instead.

`OS_GetExeName` returns empty. That name matches a title against upstream's quirks table, and a
payload is loaded rather than exec'd under a name - there is no `getprogname` to ask. Empty
means no quirk matches. A title that needs one sets it through SDL's hints.

The link needs `SDL_LoadObject`, `SDL_LoadFunction` and `SDL_UnloadObject`, which SDL declares in
its public header whatever the config says, so `../sdl2` compiles `src/loadso/dummy/`.

## Using it

A title includes `oops-sdl12.mk` instead of `oops-sdl.mk`, never both - this file pulls in the
SDL2 it needs and hands back the renamed copy. A title that linked both would get two SDL2s and
the collision above.

```make
OOPS_SDL12 ?= $(abspath ../../oops-deps/sdl12-compat)
include $(OOPS_SDL12)/oops-sdl12.mk

EXTRA_TARGET_CFLAGS  += $(OOPS_SDL12_INCLUDE)
EXTRA_TARGET_LDFLAGS += $(OOPS_SDL12_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(OOPS_SDL12_LIB) $(OOPS_SDL12_SDL2_LIB)
```

Building needs `nm` and `objcopy`; the rule fails with a message naming them rather than
producing an archive that would link wrong.
