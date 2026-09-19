# SeaShell — clean-room unified console shell and launcher

A freestanding console shell and homebrew launcher. It draws a complete console interface — a Games/Media
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
| **Games Carousel** | **implemented** | Horizontal title carousel, selection cards with Title ID, category badge (`NATIVE BIG APP 0`, `ELF`, `LEGACY`), favorite badge (`[*]`), version |
| **Game Hub Detail Strip** | **implemented** | Primary action (`[X] PLAY`), `[OPTIONS] OPTIONS` prompt, trophies progress bar (`28 / 42`), contextual activity cards |
| **Media Carousel** | **implemented** | Dedicated `MEDIA` mode with Media Player, Media Gallery, and Web Browser |
| **Title Options Menu** | **implemented** | `PLAY`, `ADD/REMOVE FAVORITE`, `CHECK FOR UPDATE`, `MANAGE GAME CONTENT`, `SAVED DATA`, `INFORMATION`, `DELETE`, `BACK` |
| **Title Information Screen** | **implemented** | Complete metadata: Title ID, category, version, format, location (`/user/app/`, `/mnt/usb0/`), target SDK, audio format, parental rating |
| **Game Library** | **implemented** | Filtered view with 4 toggleable categories: `ALL TITLES`, `NATIVE APPS`, `HOMEBREW & ELFS`, and `FAVORITES (PINNED)` |
| **Control Centre (Home button)**| **implemented** | Quick menu overlay: 13 bottom dock icons (`HOME`, `SWITCHER`, `NOTICES`, `BASE`, `MUSIC`, `CAPTURES`, `ACC`, `NET`, `SOUND`, `MIC`, `PADS`, `USER`, `POWER`) + upper activity and download cards |
| **Active Switcher Lifecycle** | **implemented** | Real-time running/suspended state tracking (`NOW PLAYING` vs `SUSPENDED`), with `RESUME GAME`, `SUSPEND GAME`, `CLOSE GAME`, and recent title shortcuts |
| **Game Base** | **implemented** | Friends online status list and voice party chat indicator |
| **Media Gallery / Captures** | **implemented** | Screenshots and video clips list with resolution, date, size, and screenshot capture action |
| **Saved Data Manager** | **implemented** | Save data management: backup to USB/host, restore, and delete |
| **Storage Visual Meter** | **implemented** | Color-coded disk usage meter bar (Games, Media, Saves, Other, Free) + drive breakdown (Internal SSD, M.2, USB, Host) |
| **Multi-Mount USB Scanning** | **implemented** | Automatic discovery of installed titles, `.pkg` packages, and `.elf` payloads across `/mnt/usb0` and `/mnt/usb1` |
| **Settings & Favorites Persistence** | **implemented** | Clean-room binary struct serialization saving active theme, mode, audio preferences, and pinned favorites to `/data/homebrew/SCSH00001/settings.bin` |
| **Settings Subsystems** | **implemented** | Full tree: Users, System (HDMI, Power Saving), Storage, Sound (3D audio, volume, mic), Screen & Video (4K, 120Hz, VRR), Accessories (Controller triggers/haptics) |
| **Developer & Debug Settings**| **implemented** | Package installer (`.pkg`), Payload runner (`.elf`), Big App 0 category override, pltauth bypass indicator, live kernel log viewer, filesystem browser |
| **Emulator Settings** | **implemented** | Save state slots (1-10), quick save/load, frame limiter (60 FPS / VSync), shader compilation stats, performance HUD |
| **Common Dialogs Subsystem** | **implemented** | Modal overlays: Confirmation prompt (`[CANCEL]` / `[OK]`), Progress bar modal (`45%`), Error dialog (`CE-108255-1`) |
| **Virtual Keyboard (IME)** | **implemented** | 4-row QWERTY on-screen keyboard with `[SPACE]`, `[BACKSPACE]`, `[CLEAR]`, and `[DONE]` for search and text input |
| **Toast Notifications** | **implemented** | Floating notification popup card in upper right corner with auto-dismiss frame timer |
| **Theme & Layout Engine** | **implemented** | Modular skins (`MODERN` carousel, `XMB` Cross Media Bar, `BLADES` Dashboard, `MEMCARD` Column Browser, `REVOLUTION` 4x3 Grid) + customizable cursor styles |
| **Input Responsiveness** | **implemented** | Fast 14-frame hold repeat, 3-frame stride, 4 ms idle sleep, multi-port pad polling, and dynamic carousel horizontal viewport scrolling |

---

## Controls (Console UI)

| input | action |
|---|---|
| **Home Button** (`bit 16`) | Toggle Control Centre overlay (open / close) |
| **L1 / R1** | Switch between category modes or tabs |
| **D-pad / Sticks** | Navigate; press fires immediately, then repeats after initial hold delay |
| **Cross / Enter** | Select / activate current item |
| **Circle / Back** | Return to previous screen or cancel dialog |
| **Triangle** | Open Universal Search |
| **Options / Menu** | Open Title Options context menu |
| **Square** | Open Game Library |

---

## Modular Skin Architecture

SeaShell decouples UI navigation and rendering through a generic, declarative skin system (`home_skin_t` defined in `skin.h`):

1. **`MODERN` (Modern Carousel)**:
   - Horizontal sliding carousel with dynamic viewport scrolling tracking `title_cursor`.
   - Selected title expands with elevated frame, glowing badge, and lower game hub detail strip (trophies, play button, contextual activity cards).
2. **`XMB` (Cross Media Bar)**:
   - Horizontal category ribbon (`SETTINGS`, `PHOTO`, `MUSIC`, `VIDEO`, `GAMES`, `NETWORK`).
   - Dynamic sine wave ribbon meshes and ambient floating particle system.
   - Horizontal category rotation $\times$ vertical item list scrolling.
3. **`BLADES` (Curved Blade Dashboard)**:
   - Overlapping curved blade tabs (`NETWORK`, `GAMES`, `MEDIA`, `SYSTEM`).
   - Left hero card with large artwork box, right companion blade preview, and color-coded action button legend.
4. **`MEMCARD` (Framed Column List Browser)**:
   - Classic dual-column framed card grid with vintage inset borders and index numbering (`#01`, `#02`, ...).
   - Right-hand storage & title info pane with full path metadata.
5. **`REVOLUTION` (4x3 Channel Grid)**:
   - 4x3 grid of 12 rounded TV channels (Optical Disc, Avatar, Photo, Shop, Forecast, News, Games).
   - Bottom console swoop with 3D spherical System button, SD Card slot, digital clock/date, and Message Board envelope button.
   - Full 2D grid D-pad navigation.

---

## Input Responsiveness & Capacity Scaling

1. **Horizontal Viewport Windowing**:
   - The modern carousel calculates `view_start = title_cursor - 2` (clamped to bounds), dynamically centering the active item so titles 0..63 remain seamlessly in view.
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

Inspired by modern console shells and homebrew launchers:

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
- **Visual Status Badging**: Pinned titles display a distinct `[*]` gold accent badge across all layout engines.
- **Instant Filtering**: Pinned titles can be isolated immediately in the Library view.

### 4. Game Library Category Filtering
- **Dynamic Category Cycling**: Pressing the top filter button in Game Library cycles seamlessly across 4 views:
  1. `ALL TITLES`: Complete unified collection.
  2. `NATIVE APPS`: Official big applications and retail titles.
  3. `HOMEBREW & ELFS`: Native homebrew payloads and tools.
  4. `FAVORITES (PINNED)`: Only user-pinned favorite titles.

### 5. Multi-Mount USB Storage Scanning
- **Dual Port Support**: Scans `/mnt/usb0` and `/mnt/usb1` alongside internal SSD storage (`/user/app`, `/data/homebrew`).
- **Automated Detection**: Discovers installed titles, external `.pkg` packages, and standalone `.elf` payloads.
- **Seamless Icon Resolution**: Automatically resolves `icon0.png` from `/mnt/usb0/<ID>/sce_sys/icon0.png` or root directory payloads.

### 6. Settings & Favorites Persistence
- **Clean-Room Binary Serialization**: Saves state to `/data/homebrew/SCSH00001/settings.bin` (with fallback to `/data/seashell_settings.bin`).
- **Preserved Preferences**: Persists active theme, mode (`GAMES` vs `MEDIA`), audio state, and pinned favorite Title IDs across sessions.
- **Fail-Safe I/O**: Employs freestanding `oops_fs_read_all()` and `oops_fs_write_all()`, validating magic header (`0x53435348u`) and format version before applying.

---

## Status

**Compiled and verified across all formats.** Built under strict compiler warning flags
(`-std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion -Wstrict-prototypes -Wmissing-prototypes`).
Headless host selftest passes cleanly. Shipped outputs under `dist/` follow the project-wide four-axis conventions (`OOPS/docs/CONVENTIONS.md` section 2):
- `dist/seashell-prospero.elf`: freestanding ELF for Orbistoun and `elfldr`.
- `dist/seashell-eboot-prospero.bin`: fake-signed executable container (`fSELF`).
- `dist/seashell-title-prospero.zip`: full native Big App category 0 bundle (`SCSH00001/` with `eboot.bin`, `sce_sys/param.json`, `icon0.png`, `keystone`) ready to deploy to `/user/app/SCSH00001` or auto-mount via ShadowMountPlus.
