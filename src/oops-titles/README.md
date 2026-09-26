# Titles

Programs ported to run on the hardware through `oops-sdk`. A title builds and boots without
the user supplying assets or ROMs. An emulator is the one exception: it builds, boots and runs
without proprietary code, and only its content comes from the user.

## What a title is made of

A title is an origin, our patches, our shim and its metadata. None of the program's own source
or assets is committed here.

```
oops-titles/<title>/
  upstream.lock     the origin: where it comes from and exactly which revision
  upstream/         the fetched tree, never edited and never committed
  patches/          our changes to it, numbered, applied to a clean checkout
  shim/             our code: the entry point, and what oops-sdk does not provide
  sce_sys/          icon0.png, pic0.png, logo.png, param.json
  app.env           the application identifier, name and version
  Makefile          through oops-apps/common/app.mk, like every other app here
```

`common/upstream.mk` fetches `upstream/` at the revision `upstream.lock` names on the first
`make`, applies every `patches/*.patch` in sorted order, and stamps the result; the patch set is
part of the stamp, so adding or removing a patch re-fetches. `make upstream-clean` removes the
tree. A lock file rather than a submodule keeps a version bump to one line in one repository and
lets the fetch be shaped (`--depth 1 --filter=blob:none` for large asset repositories).

The identity guard (`~/.oops-identity/scan.sh`) reads what is staged. Upstream is never staged,
so the scan covers exactly our patches, shim and metadata.

## Shims and patches

This is the policy for every title; titles carry no README of their own for it.

- **A shim is minimal and belongs to one title.** `shim/include/` goes on the include path
  ahead of upstream's own headers, so a missing header is answered without touching upstream;
  a translation unit in `shim/` supplies a missing function. What a second port would want is
  an SDK entry point or lives in [`common/`](../../common/).
- **A patch is minimal and belongs to one title.** It changes upstream only where upstream's
  own code must behave differently here: a hard-coded `main`, a path only this program builds,
  a menu the pad cannot reach. A missing header or function is a shim, never a patch. A patch
  that adapts the platform rather than the program is the platform's job, because a patch is
  carried and rebased indefinitely.
- A title whose `patches/` grows faster than its `shim/` is pointing at something missing from
  the SDK.

[`../../AGENTS.md`](../../AGENTS.md) has the table of where each kind of fix goes.

## Identifiers and builds

Application identifiers are four letters naming the title and `00001` (`GLCB00001`,
`GLUT00001`). The `Makefile` goes through `common/app.mk`, so a title gets the same
undefined-symbol check as every payload here: a program calling a C library function this SDK
lacks fails the link instead of faulting on the hardware. CI builds each title and publishes
it to the release.

## Queued titles

A title with a pinned upstream and no code of ours has no directory. These are the pins and
what their source says about rendering.

| Title | Upstream | Revision | From its source |
|---|---|---|---|
| Bugdom 2 | `github.com/jorio/Bugdom2` | `v4.0.0` (`4050d6f9`) | Bugdom's engine and `extern/Pomme`; immediate-mode GL 1.x; game data ships upstream |
| SuperTux 3D | `github.com/edstoner/supertux_3d` | `5d409c08` | GDScript and assets for Godot 3.x; no C or C++; porting it is porting Godot's `platform/` layer |
| Armagetron Advanced | `github.com/ArmagetronAd/armagetronad` | `v0.2.9.3.0` (`036daaf3`) | GL 1.x (`glBegin` and vertex arrays); C++ with exceptions, RTTI and boost |
| RetroArch | `github.com/libretro/RetroArch` | `v1.22.2` (`69a4f0ea`) | separate `gl1`, `gl2` and `gl3` video drivers; C |
| SuperTuxKart | `github.com/supertuxkart/stk-code` | `1.5` (`1fb491f5`) | OpenGL 3.3 or GLES 3.0 per its README; assets in the separate `stk-assets` repository |

## Porting families

The N64 and GameCube ports in circulation fall into a few families, and the family decides
whether a port is in reach:

| Family | Renderer | Here |
|---|---|---|
| `libultraship` (Ship of Harkinian, 2ship2harkinian, SpaghettiKart, Starship, PaperBoat, Ghostship) | its own Fast3D over GL 2.1-era GLSL under a `#version 130` directive; no direct GL calls in the games | in reach: [ship-of-harkinian](ship-of-harkinian/), [spaghetti-kart](spaghetti-kart/) |
| `sm64-port`, `perfect_dark` | GL 2.0; assets extracted from a ROM at build time | [sm64](sm64/) |
| N64Recomp (Banjo, Zelda 64, Harvest Moon 64) | RT64, which is Vulkan and D3D12 | out of reach |
| Dusklight (Twilight Princess) | Aurora over WebGPU and Dawn | out of reach |
| raw decompilations (`zeldaret/oot`, `n64decomp/sm64`, ...) | none: they build a ROM or DOL | not ports |

The Harbour Masters ports build with no ROM anywhere and convert the player's own copy on the
device, which is the shape the rule at the top of this file allows.
