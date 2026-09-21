# SeaShell — skins

SeaShell decouples UI navigation from rendering through a generic, declarative skin system
(`home_skin_t` in `skin.h`). The same model state drives every layout below.

## MODERN — modern carousel
Horizontal sliding carousel with dynamic viewport scrolling tracking `title_cursor`. The selected
title expands with an elevated frame, a glowing badge, and a lower game-hub detail strip
(trophies, play button, contextual activity cards).

## XMB — cross media bar
Horizontal category ribbon (`SETTINGS`, `PHOTO`, `MUSIC`, `VIDEO`, `GAMES`, `NETWORK`) with
dynamic sine-wave ribbon meshes and an ambient floating-particle system. Horizontal category
rotation crossed with vertical item-list scrolling.

## BLADES — curved blade dashboard
Overlapping curved blade tabs (`NETWORK`, `GAMES`, `MEDIA`, `SYSTEM`), a left hero card with a
large artwork box, a right companion blade preview, and a colour-coded action-button legend.

## MEMCARD — framed column list browser
Classic dual-column framed card grid with vintage inset borders and index numbering
(`#01`, `#02`, …), plus a right-hand storage & title-info pane with full path metadata.

## REVOLUTION — 4×3 channel grid
A 4×3 grid of twelve rounded TV channels, a bottom console swoop with a 3D spherical System
button, an SD-card slot, a digital clock/date, and a Message Board envelope button. Full 2D grid
D-pad navigation.

## Capacity & responsiveness

- **Horizontal viewport windowing** — the modern carousel centres the active item
  (`view_start = title_cursor - 2`, clamped) so titles stay in view across the whole list.
- **Responsive hold & repeat** — 14-frame initial delay (~230 ms), 3-frame stride (~50 ms),
  4 ms idle sleep (`HOME_IDLE_SLEEP_US`).
- **Multi-port polling** — ports 0..3 scanned every frame with no artificial throttling.
- **Expanded capacities** — up to 64 titles (`HOME_MAX_TITLES`) and 64 menu items
  (`HOME_MAX_ITEMS`).
