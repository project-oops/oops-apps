# SeaShell - screens and features

The full screen hierarchy and launcher capabilities. The [README](../README.md) is the overview;
[ARCHITECTURE](ARCHITECTURE.md) explains how these are built and tested.

## Screen hierarchy & feature map

| feature / screen | details |
|---|---|
| **Top Navigation Bar** | `GAMES` and `MEDIA` tabs with accent underline; `SEARCH`, `SETTINGS`, `PROFILE`, and clock in system tray |
| **Interactive Universal Search** | Dynamic substring search matching title names and Title IDs; live results list with 1-click launch action |
| **Games Carousel** | Horizontal title carousel, selection cards with Title ID, category badge (`NATIVE BIG APP 0`, `ELF`, `LEGACY`), favorite badge (`[*]`), version |
| **Game Hub Detail Strip** | Primary action (`[SELECT] PLAY`), `[OPTIONS]` prompt, trophies progress bar (`28 / 42`), contextual activity cards |
| **Media Carousel** | Dedicated `MEDIA` mode with Media Player, Media Gallery, and Web Browser |
| **Title Options Menu** | `PLAY`, `ADD/REMOVE FAVORITE`, `CHECK FOR UPDATE`, `MANAGE GAME CONTENT`, `SAVED DATA`, `INFORMATION`, `DELETE`, `BACK` |
| **Title Information Screen** | Complete metadata: Title ID, category, version, format, location, target SDK, audio format, parental rating |
| **Game Library** | Filtered view with 4 toggleable categories: `ALL TITLES`, `NATIVE APPS`, `HOMEBREW & ELFS`, `FAVORITES (PINNED)` |
| **Control Centre (Home button)** | Quick menu overlay: bottom dock icons + upper activity and download cards |
| **Active Switcher Lifecycle** | Real-time running/suspended tracking (`NOW PLAYING` vs `SUSPENDED`), with `RESUME`, `SUSPEND`, `CLOSE`, and recent-title shortcuts |
| **Game Base** | Friends online status list and voice party chat indicator |
| **Media Gallery / Captures** | Screenshots and video clips with resolution, date, size, and a capture action |
| **Saved Data Manager** | Backup to USB/host, restore, and delete |
| **Storage Visual Meter** | Colour-coded disk usage meter (Games, Media, Saves, Other, Free) + drive breakdown |
| **Multi-Mount USB Scanning** | Automatic discovery of installed titles, `.pkg` packages and `.elf` payloads across `/mnt/usb0` and `/mnt/usb1` |
| **Settings & Favorites Persistence** | Binary struct serialization of active theme, mode, audio preferences, and pinned favorites |
| **Settings Subsystems** | Users, System (HDMI, Power Saving), Storage, Sound, Screen & Video (4K, 120Hz, VRR), Accessories |
| **Developer & Debug Settings** | Package installer, payload runner, Big App 0 category override, pltauth bypass indicator, live kernel log viewer, filesystem browser |
| **Emulator Settings** | Save-state slots (1-10), quick save/load, frame limiter, shader compilation stats, performance HUD |
| **Common Dialogs Subsystem** | Confirmation prompt, progress-bar modal, error dialog |
| **Virtual Keyboard (IME)** | 4-row QWERTY on-screen keyboard for search and text input |
| **Toast Notifications** | Floating notification card with auto-dismiss timer |
| **Theme & Layout Engine** | Modular skins (see [SKINS](SKINS.md)) + customizable cursor styles |
| **Input Responsiveness** | 14-frame hold repeat, 3-frame stride, 4 ms idle sleep, multi-port pad polling, dynamic carousel scrolling |

## Launcher capabilities

### Interactive universal search
Instant case-insensitive substring matching against both title names and Title IDs; the on-screen
keyboard (IME) opens from `△` or the `SEARCH` tab; matching results carry a direct launch action
for 1-click execution.

### Active title lifecycle & dynamic switcher
State-aware execution tracks running titles (`NOW PLAYING` vs `SUSPENDED`). The Control Centre
switcher card shows a state badge and contextual controls (`RESUME`, `SUSPEND`, `CLOSE`) plus a
quick-switch recent list.

### Title pinning & favorites
Toggle favorite status from the Title Options menu; pinned titles show a `[*]` badge across every
layout engine and can be isolated in the Library.

### Game library category filtering
Cycles across `ALL TITLES`, `NATIVE APPS`, `HOMEBREW & ELFS`, and `FAVORITES (PINNED)`.

### Multi-mount USB storage scanning
Scans `/mnt/usb0` and `/mnt/usb1` alongside internal storage, discovering installed titles,
external `.pkg` packages and standalone `.elf` payloads, and resolving each `icon0.png`.
