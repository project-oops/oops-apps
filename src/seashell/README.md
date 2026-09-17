# SeaShell — clean-room reimplementation of the PS5 shell (Prospero UX)

A freestanding homebrew shell and launcher. It draws a complete console interface — a Games/Media
carousel, game hubs with activity cards and trophy progress, an authentic 13-dock Control Centre,
deep settings trees, a storage visual meter, and common system dialogs (virtual keyboard, progress,
confirmation, and error popups).

Two audiences, one codebase:
1. **The front-end shell for Orbistoun**: drives emulator processes, save states, captures, and
   hardware configuration.
2. **A native shell on real hardware**: ships as a native eboot Big App (category 0, root privilege), installed to `/user/app/SCSH00001`; a plain-ELF build is also produced for the `elfldr` / Orbistoun path.

That is possible because of the strict split below, which isolates UI state and software-canvas
rendering from machine-specific execution.

---

## The split: the model does everything that does not touch a machine

| | lives in | tested |
|---|---|---|
| navigation, screens, cursors, what each screen contains | `home.c` | with no display at all |
| drawing | `home.c` | into a plain memory surface (1280x720) |
| launching a title, installing a package, running a payload, power | **the host** | with a recorder |
| the display, the pad, the loop | `home_main.c` | on hardware and in emulator |

The model never launches anything directly. It hands the host an **action and an argument** and the
host decides what that means:
- **Orbistoun** maps the dispatch into emulator syscalls, process lifecycles, and save states.
- **Real hardware** maps the dispatch into on-console services and kernel calls.
- **Self-tests** map the dispatch into an action recorder to assert exact parameter passing.

**A host that declines is recorded, not swallowed.** `perform` returns whether it handled the
action, and the screen displays `NOT AVAILABLE HERE` when unhandled.

---

## Screen hierarchy & feature map

| feature / screen | status | details |
|---|---|---|
| **Top Navigation Bar** | **implemented** | `GAMES` and `MEDIA` tabs with accent underline; `SEARCH`, `SETTINGS`, `PROFILE`, and clock in system tray |
| **Interactive Universal Search** | **implemented** | Dynamic substring search matching title names and Title IDs; live results list with 1-click launch action |
| **Games Carousel** | **implemented** | Horizontal title carousel, selection cards with Title ID, category badge (`PS5 BIG APP 0`, `ELF`, `PS4`), favorite badge (`[*]`), version |
| **Game Hub Detail Strip** | **implemented** | Primary action (`[X] PLAY`), `[OPTIONS] OPTIONS` prompt, trophies progress bar (`28 / 42`), contextual activity cards |
| **Media Carousel** | **implemented** | Dedicated `MEDIA` mode with Media Player, Media Gallery, and Web Browser |
| **Title Options Menu** | **implemented** | `PLAY`, `ADD/REMOVE FAVORITE`, `CHECK FOR UPDATE`, `MANAGE GAME CONTENT`, `SAVED DATA`, `INFORMATION`, `DELETE`, `BACK` |
| **Title Information Screen** | **implemented** | Complete metadata: Title ID, category, version, format, location (`/user/app/`, `/mnt/usb0/`), target SDK (`FW 12.40`), audio format, parental rating |
| **Game Library** | **implemented** | Filtered view with 4 toggleable categories: `ALL TITLES`, `PS5 NATIVE APPS`, `HOMEBREW & ELFS`, and `FAVORITES (PINNED)` |
| **Control Centre (PS button)**| **implemented** | Quick menu overlay: 13 bottom dock icons (`HOME`, `SWITCHER`, `NOTICES`, `BASE`, `MUSIC`, `CAPTURES`, `ACC`, `NET`, `SOUND`, `MIC`, `PADS`, `USER`, `POWER`) + upper activity and download cards |
| **Active Switcher Lifecycle** | **implemented** | Real-time running/suspended state tracking (`NOW PLAYING` vs `SUSPENDED`), with `RESUME GAME`, `SUSPEND GAME`, `CLOSE GAME`, and recent title shortcuts |
| **Game Base** | **implemented** | Friends online status list and voice party chat indicator |
| **Media Gallery / Captures** | **implemented** | Screenshots and video clips list with resolution, date, size, and screenshot capture action |
| **Saved Data Manager** | **implemented** | PS5 save data management: backup to USB/host, restore, and delete |
| **Storage Visual Meter** | **implemented** | Color-coded disk usage meter bar (Games, Media, Saves, Other, Free) + drive breakdown (Console SSD, M.2, USB, Host) |
| **Multi-Mount USB Scanning** | **implemented** | Automatic discovery of installed titles, `.pkg` packages, and `.elf` payloads across `/mnt/usb0` and `/mnt/usb1` |
| **Settings & Favorites Persistence** | **implemented** | Clean-room binary struct serialization saving active theme, mode, audio preferences, and pinned favorites to `/data/homebrew/SCSH00001/settings.bin` |
| **Settings Subsystems** | **implemented** | Full tree: Users, System (FW 12.40, HDMI, Power Saving), Storage, Sound (3D audio, volume, mic), Screen & Video (4K, 120Hz, VRR), Accessories (DualSense triggers/haptics) |
| **Developer & Debug Settings**| **implemented** | Package installer (`.pkg`), Payload runner (`.elf`), Big App 0 category override, pltauth bypass indicator, live kernel log viewer, filesystem browser |
| **Emulator Settings** | **implemented** | Save state slots (1-10), quick save/load, frame limiter (60 FPS / VSync), shader compilation stats, performance HUD |
| **Common Dialogs Subsystem** | **implemented** | Modal overlays: Confirmation prompt (`[CANCEL]` / `[OK]`), Progress bar modal (`45%`), Error dialog (`CE-108255-1`) |
| **Virtual Keyboard (IME)** | **implemented** | 4-row QWERTY on-screen keyboard with `[SPACE]`, `[BACKSPACE]`, `[CLEAR]`, and `[DONE]` for search and text input |
| **Toast Notifications** | **implemented** | Floating notification popup card in upper right corner with auto-dismiss frame timer |
| **Theme & Layout Engine** | **implemented** | 4 distinct layouts (`TILES` Prospero carousel, `XMB` Cross Media Bar, `LIST` PS1/PS2 Browser, `BLADES` 360 Dashboard) + 3 cursor styles (`BOX`, `UNDERLINE`, `BAR`) |
| **Input Responsiveness** | **implemented** | Fast 14-frame hold repeat, 3-frame stride, 4 ms idle sleep, multi-port pad polling, and dynamic carousel horizontal viewport scrolling |

---

## Controls (Authentic PS5 UI)

| input | action |
|---|---|
| **PS Button** (`bit 16`) | Toggle Control Centre overlay (open / close) |
| **L1 / R1** | Switch between `GAMES` and `MEDIA` modes |
| **D-pad / Sticks** | Navigate; press fires immediately, then repeats after initial hold delay |
| **Cross** ($\times$) | Select / activate highlighted item or button |
| **Circle** ($\bigcirc$) | Back / cancel / dismiss dialog |
| **Square** ($\square$) | Quick jump to Game Library |
| **Triangle** ($\triangle$) | Quick jump to Universal Search (opens on-screen keyboard) |
| **Options** | Context menu on highlighted title / options |
| **Up** (from carousel) | Move focus to Top Bar (Tabs, Search, Settings, Profile) |
| **Down** (from top bar) | Return focus to carousel |
| **L1 + R1 + Options** | Clean exit to loader |

---

## Building

```bash
make check      # Headless test of model, seams, dialogs, IME keyboard, and renderer
make skeleton   # Compiles freestanding for the target (object only)
make elf        # Builds freestanding payload ELF (build/seashell.elf)
make eboot      # Wraps ELF into fake-signed Prospero executable (dist/seashell-eboot-prospero.bin) via selfish
make title      # Packages complete native PS5 Big App bundle (dist/seashell-title-prospero.zip) via selfish
make dist       # Stages all shippable formats under dist/
```

Or using the repository CLI runner:

```bash
./bin/oops-apps check seashell
./bin/oops-apps dist seashell
```

---

## Architecture: Dynamic Filesystem Discovery & Sandbox Escape

On physical hardware, SeaShell operates as a native Big App (category 0). By default, the console kernel confines Big Apps to their private `/app0` sandbox mount, which denies access to other installed titles on the internal SSD.

### 1. Zero-Polling Event-Driven Handshake
SeaShell elevates its filesystem namespace dynamically at startup without relying on third-party daemons (etaHEN, Lapy JB daemon), cheats, or background polling loops:
1. **Single Connect at Boot**: SeaShell invokes `oops_system_escape_sandbox()` once on startup.
2. **Loopback IPC**: Connects via TCP to `127.0.0.1:9069`, where the resident first-party `sandbox-daemon` is suspended in a blocking `accept()` call (0% CPU, 0 disk I/O).
3. **Namespace Elevation**: `sandbox-daemon` repoints SeaShell's `fd_rdir` (`0x10`), `fd_jdir` (`0x18`), and `fd_cdir` (`0x08`) to the dynamically resolved `rootvnode`, elevates its credentials to root UID `0` and `cr_sceauthid = 0x4801000000000013`, and acknowledges with status `0`.
4. **Immediate Close**: SeaShell closes the socket and proceeds with global storage access.

### 2. Title Discovery & Metadata Extraction
Once elevated, SeaShell queries console storage:
- **Directory Enumeration**: Probes `/user/appmeta`, `/data/homebrew`, and `/user/app` using modern FreeBSD 12 `SYS_getdirentries` (554) with a 64-bit `basep` pointer and `ino64` layout.
- **Candidate Probing Fallback**: Validates installed candidates directly on disk via `oops_fs_exists()` checking `/user/appmeta/<ID>/param.json`, `/user/appmeta/<ID>/icon0.png`, and executable paths.
- **Metadata Extraction**: Parses genuine `titleName` and master/content versions directly from `param.json`. Zero invented titles.
- **Icon Loading**: Reads and decodes genuine `icon0.png` images from disk into 96x96 RGB tile surfaces for the home carousel.

### 3. Periodic Background Storage Polling
To automatically reflect newly installed packages, games, or staged homebrew without requiring a shell restart, SeaShell performs periodic background storage scans:
- **Interval**: Scans every 600 frames (~10 seconds at 60 Hz).
- **Telemetry**: Emits a `[SCSH00001:HOME]` status line to `klog` on every cycle (e.g. `periodic storage poll: 17 titles, no changes`).
- **Silent Steady-State**: If no new titles are detected, the scan completes silently with zero UI modification, zero frame hitches, and no toast popups.
- **Dynamic Discovery**: When a new title is found on disk, SeaShell parses its `param.json`, decodes its `icon0.png`, appends it to the active carousel model, and displays a temporary notification toast (`LIBRARY UPDATED`).

---

## Layout Engines & Theme Customization

SeaShell supports 4 distinct rendering engines driven directly by the selected skin in Settings (`Skins & Themes`):

1. **`HOME_LAYOUT_TILES` (Default / Modern PS5 Prospero)**:
   - Horizontal sliding carousel with dynamic viewport scrolling tracking `title_cursor`.
   - Selected title expands with elevated frame, glowing badge, and lower game hub detail strip (trophies, play button, contextual activity cards).
2. **`HOME_LAYOUT_XMB` (Amber / Authentic Cross Media Bar)**:
   - Authentic PSP/PS3 horizontal category ribbon (`SETTINGS`, `PHOTO`, `MUSIC`, `VIDEO`, `GAMES`, `NETWORK`).
   - Vertical title cascade drops directly below the active category column, with secondary items dimmed and selected item highlighted.
   - Right-side showcase card displaying title version, category ID, and quick action hints.
3. **`HOME_LAYOUT_LIST` (Retro / PS1 Memory Card & PS2 System Browser)**:
   - Classic dual-column framed card grid with vintage inset borders and index numbering (`#01`, `#02`, ...).
   - Right-hand storage & title info pane with full path metadata and target SDK firmware levels.
4. **`HOME_LAYOUT_BLADES` (Blades / Xbox 360 Dashboard)**:
   - Vertical tab blade tabs (`MARKETPLACE`, `GAMES`, `MEDIA`, `SYSTEM`) pinned across the top bar.
   - Prominent left hero game card with large artwork box and status tags.
   - Vertical companion blade stack on the right displaying adjacent titles.

### Dynamic Cursor Styling
- **`HOME_CURSOR_BOX`**: Bounded outline frame highlighting the perimeter of the focused item.
- **`HOME_CURSOR_UNDERLINE`**: PS2-style glowing horizontal underline accent along the bottom edge of focused cards.
- **`HOME_CURSOR_BAR`**: Full-bleed solid accent bar with an indicator marker.

---

## Input Responsiveness & Capacity Scaling

1. **Horizontal Viewport Windowing**:
   - In previous iterations, navigating past title index 6 moved the cursor off-screen while the carousel stayed fixed at indices 0..6, creating an apparent freeze.
   - The modern carousel now calculates `view_start = title_cursor - 2` (clamped to bounds), dynamically centering the active item so titles 0..63 remain seamlessly in view.
2. **Responsive Hold & Repeat**:
   - Initial repeat delay shortened from 24 frames (~400 ms) to **14 frames** (~230 ms).
   - Continuous repeat stride shortened from 4 frames (~66 ms) to **3 frames** (~50 ms).
   - Idle loop sleep reduced from 16 ms to **4 ms** (`HOME_IDLE_SLEEP_US`), cutting wake-up jitter by 75%.
3. **Multi-Port Polling**:
   - Pad polling scans ports 0..3 every frame without artificial multi-frame throttling.
4. **Expanded Capacities**:
   - Title capacity raised from 24 to **64 titles** (`HOME_MAX_TITLES`).
   - General UI menu items raised from 18 to **64 items** (`HOME_MAX_ITEMS`).

---

## Enhanced Launcher & Shell Capabilities

Inspired by modern console shells and homebrew launchers (PS5 OS, VitaShell, ItemzFlow, Aurora, multiMAN):

### 1. Interactive Universal Search
- **ASCII Substring Matching**: Instant case-insensitive filtering against both title names (`title_name`) and Title IDs (`title_id`).
- **Integrated On-Screen Keyboard (IME)**: Pressing $\triangle$ or activating `SEARCH` in the top bar launches the 4-row virtual keyboard.
- **Direct Launch Action**: Matching results are populated directly into the Search screen menu with `HOME_ACTION_LAUNCH_TITLE` actions, allowing instant 1-click execution.

### 2. Active Title Lifecycle & Dynamic Switcher
- **State-Aware Execution**: SeaShell actively tracks running title states (`NOW PLAYING` vs `SUSPENDED (IN BACKGROUND)`).
- **Control Centre Switcher Card**:
  - When a title is active: displays state badge, title name, Title ID, and contextual controls (`RESUME GAME`, `SUSPEND GAME`, `CLOSE GAME`).
  - Quick-switch recent list: provides rapid switching back to previously launched titles.

### 3. Title Pinning & Favorites
- **Pin Any Title**: Toggle favorite status anytime from the Title Options menu (`ADD TO FAVORITES` / `REMOVE FROM FAVORITES`).
- **Visual Status Badging**: Pinned titles display a distinct `[*]` gold accent badge across all 4 layout engines (`TILES`, `XMB`, `LIST`, `BLADES`).
- **Instant Filtering**: Pinned titles can be isolated immediately in the Library view.

### 4. Game Library Category Filtering
- **Dynamic Category Cycling**: Pressing the top filter button in Game Library cycles seamlessly across 4 views:
  1. `ALL TITLES`: Complete unified collection.
  2. `PS5 NATIVE APPS`: Official big applications and retail titles.
  3. `HOMEBREW & ELFS`: Native homebrew payloads and tools.
  4. `FAVORITES (PINNED)`: Only user-pinned favorite titles.

### 5. Multi-Mount USB Storage Scanning
- **Dual Port Support**: Scans `/mnt/usb0` and `/mnt/usb1` alongside internal SSD storage (`/user/app`, `/data/homebrew`).
- **Automated Detection**: Discovers installed titles, external `.pkg` packages, and standalone `.elf` payloads.
- **Seamless Icon Resolution**: Automatically resolves `icon0.png` from `/mnt/usb0/<ID>/sce_sys/icon0.png` or root directory payloads.

### 6. Settings & Favorites Persistence
- **Clean-Room Binary Serialization**: Saves state to `/data/homebrew/SCSH00001/settings.bin` (with fallback to `/data/seashell_settings.bin`).
- **Preserved Preferences**: Persists active theme (Prospero, Dark, Amber, Retro, Cyberpunk, Blades), mode (`GAMES` vs `MEDIA`), audio state, and pinned favorite Title IDs across sessions.
- **Fail-Safe I/O**: Employs freestanding `oops_fs_read_all()` and `oops_fs_write_all()`, validating magic header (`0x53435348u`) and format version before applying.

---

## Status

**Compiled and verified across all formats.** Built under strict compiler warning flags
(`-std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion -Wstrict-prototypes -Wmissing-prototypes`).
Headless host selftest passes cleanly. Shipped outputs under `dist/` follow the project-wide four-axis conventions (`OOPS/docs/CONVENTIONS.md` section 2):
- `dist/seashell-prospero.elf`: freestanding ELF for Orbistoun and `elfldr`.
- `dist/seashell-eboot-prospero.bin`: fake-signed Prospero executable container (`fSELF`).
- `dist/seashell-title-prospero.zip`: full native PS5 Big App category 0 bundle (`SCSH00001/` with `eboot.bin`, `sce_sys/param.json`, `icon0.png`, `keystone`) ready to deploy to `/user/app/SCSH00001` or auto-mount via ShadowMountPlus.


