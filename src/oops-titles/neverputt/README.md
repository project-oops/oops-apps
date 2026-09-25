# Neverputt

<p align="center">
  <img src="assets/logo.png" alt="Neverputt" width="200">
</p>

Miniature golf on the Neverball engine — [upstream](https://github.com/Neverball/neverball),
pinned at `neverball-1.6.0`.

## Screenshot

<p align="center">
  <img src="assets/demo.gif" alt="Neverputt running on Prospero, captured over JetKVM" width="600">
</p>

## It is the second binary of the Neverball tree

Neverputt is not a separate project. Upstream's Makefile builds two programs from one checkout,
`BALL_OBJS` and `PUTT_OBJS`, and Neverputt is the second: seven sources under `putt/` plus a
subset of the `share/` files Neverball already compiles, with its own `main` at
`putt/main.c:279`.

That is why this title was cheap to add and why it linked first time. Everything underneath it
already existed — the eight pinned dependencies, the POSIX shim, the `mapc` host tool, and the
fixed-function GL layer that Neverball spent a day proving out.

**`NP_SHARE` in the Makefile is upstream's own `PUTT_OBJS`, not Neverball's list with things
removed.** The three differences are real and each would have been a link error or a stowaway:

| | why |
|---|---|
| no `tilt_null` | upstream adds a tilt backend to `BALL_OBJS` only (Makefile:349–357); Neverputt has no tilt sensing |
| no `queue`, no `cmd` | those carry the client/server command stream Neverball's replay and simulation split needs; Neverputt simulates in one place |
| `hmd_null` without `hmd_common` | the common half belongs to the openhmd and libovr arms (Makefile:361–370) |

## Building

```
make          # fetch upstream, build the payload
make data     # compile the .map sources into .sol - minutes, needed once
make package  # the title directory, with the data in it
```

`make data` needs `git` for the fetch, which the `silkeh/clang:21` container does not carry — run
the fetch on the host and the rest in the container.

`make package`, not `make title`: `common/app.mk` builds a title directory out of the payload and
`sce_sys/`, and a port also has to ship the program's own content, which this game will not start
without.

## Its own upstream checkout

`upstream.lock` names the same repository and the same commit as `../neverball/upstream.lock`,
and fetches its own copy. A title that read a sibling title's working tree would depend on
whether somebody had run `make upstream-clean` next door and on which revision *that* lock
names — a coupling this collection does not have today. Disk is free; the fetch script verifies
the hash after checkout and after patches.

The two are expected to move together but are not required to.

## What it inherits, and what it does not

Neverball carries five patches. **Neverputt carries none yet**, deliberately: three of the five
are in `ball/` and do not apply, and the two in `share/` are bring-up diagnostics that should be
added when something is opaque rather than copied on the assumption they will be needed.

**Input was the open question and it is answered: it plays.** `putt/main.c:299` initialises
`SDL_INIT_JOYSTICK` and putting is an aim-and-power interaction rather than tilt-the-floor, which
is a different shape from anything the SDL backend had been exercised against. It needed nothing.

Two patches, both of which Neverball carries too:

- **`0001` — settings persist.** Upstream calls `config_save` once, at `putt/main.c:386`, after
  the main loop returns, and a console title is closed from the dashboard rather than quitted, so
  that line never ran. Saving when the options screen is left catches every setting at once and,
  unlike a close handler, does not have to be right about how long the kill timer allows.
- **`0002` — rumble on a bounce.** The bounce strength the sound is already mixed at goes to the
  pad as well, so the speaker and the motors agree by construction. The decay lives in
  `common/haptics.c`, shared with every port; the shim starts and stops it around `main`.

## Still to do

- **No controls reference card.** Neverball has one, on the `Controls` page its help screen
  already had. Neverputt has **no help screen at all** — `putt/st_all.c` has no `st_help` — so a
  card here is a new GUI state and a new title-screen entry rather than a patch to one function.
  The artwork and the convention are ready in [`common/assets/controls/`](../../../common/assets/controls/);
  only the screen is missing.
- **No motion tilt**, and it would mean more here than in Neverball. Upstream's tilt interface is
  Neverball's alone (`ball/main.c:270`); Neverputt has no equivalent hook, so aiming a putt by
  tilting the pad would be a real change to its input handling rather than a backend swap.
- **No rumble on the stroke itself**, only on bounces. Hitting the ball is the moment that most
  wants feedback, and `putt/game.c` has the stroke separate from the collision path.

## The icon

`assets/icon0.png` is upstream's own `data/icon/neverputt.png` composited on black at 512×512.
The source is 32×32, so it is soft — Neverball's icon is a proper render of its ball texture
rather than an upscale, and this one would be better as the same. Worth doing; not worth blocking
on.
