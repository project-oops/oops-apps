# Porting Bugdom

## Where it stands

**Nothing is built. The GL surface is measured, and it is covered completely:**

| | |
|---|---|
| GL entry points the game and Pomme name | **51** |
| of those, defined by `oops-gl` | **51** |
| real gaps | **0** |
| SDL | **SDL3** — and this collection vendors SDL2. See below; this is the whole cost. |
| language | 78 C, 38 C++ (the C++ is almost entirely `extern/Pomme`) |
| game data | **ships with upstream** — 66 MB under `Data/`, Bugdom is freeware |

Three names came back "missing" from the census and none is real: `glFunction` and `glTextureName`
are Bugdom's own identifiers, and `glTexImage` is a prefix of `glTexImage2D`, which is defined.

## It is immediate-mode GL 1.x

`glBegin` and `glVertex3f` throughout — 6 and 30 occurrences. That is the same renderer shape as
Neverball and Extreme Tux Racer, which both run, so `OOPS_RENDERER = gl1` is the target and there is
no shader question to answer.

## The cost is SDL3, and it is not a version bump

Upstream requires SDL3: `CMakeLists.txt` looks for `SDL3` and nothing else. `oops-deps/sdl2` is
SDL2 with its own `backend/` directory and a patch set, and none of that transfers — SDL3 moved the
internal driver interfaces the backend plugs into. So this is a second SDL port.

There is no shortcut in the other direction either. `sdl2-compat` lets an SDL2 *application* run on
an SDL3 library; it does not let an SDL3 application run on SDL2.

Two ways to spend that, and neither has been costed:

1. **Vendor SDL3 with its own backend.** Pays for Bugdom, Bugdom 2, `isle-portable` and everything
   else that has moved on, which is most new ports.
2. **Patch Bugdom back to SDL2.** Upstream supported SDL2 until relatively recently, so the diff
   may be small - but it is a patch against a moving target, and it buys one title.

## Pomme is portable, which was the other thing worth checking

`extern/Pomme` is jorio's reimplementation of the classic Macintosh Toolbox — the API these Pangea
games were written against. The worry with any such layer is that it wraps the host OS rather than
replacing it. It does not: **5 of its 69 sources mention any Apple API at all.** It is C++ on top of
SDL, so it inherits the SDL3 question above and adds a libc++ dependency.

## What has not been measured

- **Whether Pomme's C++ is within what `oops-deps/libcxx` provides.** It uses the standard library;
  how much, and whether any of it needs the parts this target does not have, is unread.
- **Submodules.** `extern/Pomme` is a submodule, so the fetch needs `UPSTREAM_SUBMODULES=1`.
  Ship of Harkinian's lock is the precedent, including the check that a declared submodule path is
  non-empty afterwards — `git submodule update` reports success for a module it decided to skip.
- **Audio.** Pomme has its own mixer (`SoundMixer/cmixer.cpp`), so this does not go through SDL's
  audio callback in the way ETR's does.
- **The icon.** `assets/icon0.png` does not exist yet.
