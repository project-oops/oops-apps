# SeaShell

<p align="center">
  <img src="icon0.png" alt="SeaShell" width="200">
</p>

A clean-room, freestanding console shell and homebrew launcher.

## About

SeaShell draws a complete console interface — a Games/Media carousel, game hubs with activity
cards and trophy progress, a 13-dock Control Centre, deep settings trees, a storage meter, and the
common system dialogs (virtual keyboard, progress, confirmation, error) — and launches what you
pick.

- **Two audiences, one codebase.** It is both the front-end shell for the
  [Orbistoun](../../../../orbistoun/) emulator and a native shell on real hardware, shipping as a
  category-0 Big App.
- **A strict model/host split.** Everything that doesn't touch a machine — navigation, screens,
  drawing — lives in the model and is tested with no display at all; launching, installing and
  power are the host's, behind a recorded dispatch.
- **Five layout engines** over the same state: MODERN, XMB, BLADES, MEMCARD and REVOLUTION.
- **A real launcher** — universal search, favourites, a running/suspended switcher, and
  multi-mount USB scanning for titles, `.pkg` packages and `.elf` payloads.

## Screenshot

<p align="center">
  <img src="../../../common/assets/no-screenshot.svg" alt="No screenshot yet" width="600">
</p>

## Docs

- **[Architecture](docs/ARCHITECTURE.md)** — the model/host split, persistence, and build outputs.
- **[Screens & features](docs/FEATURES.md)** — the full screen map and launcher capabilities.
- **[Skins](docs/SKINS.md)** — the five layout engines and the responsiveness tuning.
- **[Controls](docs/CONTROLS.md)** — the console UI input map.
- [oops-apps catalog & guide](../../../docs/USER_GUIDE.md)
