# wipeout

Native, high-performance port of the classic PlayStation anti-gravity racing game *WipEout* for Prospero / Trinity, built on `oops-sdk`.

The engine uses Dominic Szablewski's clean C99 rewrite ([`phoboslab/wipeout-rewrite`](https://github.com/phoboslab/wipeout-rewrite)) hooked directly into the OOPS platform layer.

---

## Architecture: Zero-Fork Platform Shim

To prevent fork rot, this repository does not fork or maintain modified copies of the upstream engine:
- **Engine Logic**: Unmodified upstream C99 game logic (`wipeout-rewrite`).
- **Platform Layer (`platform_oops.c`)**: Direct implementation of the WipEout platform abstraction:
  - **Display**: 1080p / 720p presentation via `oops_display_open` and `oops_display_flip`, accelerated by the RDNA2 AGC hardware compute tiler.
  - **Input**: DualSense controller polling via `oops_input_poll` with analog stick deadzones and airbrake trigger mapping.
  - **Audio**: 44.1 kHz stereo PCM streaming via `oops_audio_open` and `oops_audio_write`.
  - **Timing**: High-resolution monotonic timing via `oops_time_monotonic`.

---

## Controls (DualSense)

| Action | DualSense Binding |
|---|---|
| **Steer** | Left Stick (X-axis) / D-Pad |
| **Pitch** | Left Stick (Y-axis) |
| **Thrust** | **Cross** ($\times$) |
| **Fire Weapon** | **Square** ($\square$) |
| **Left Airbrake** | **L2 Trigger** |
| **Right Airbrake** | **R2 Trigger** |
| **Rear View** | **Circle** ($\bigcirc$) |
| **Change Camera** | **Triangle** ($\triangle$) |
| **Pause** | **Options** |
| **Clean Exit to Loader** | **L1 + R1 + Options** |

---

## Game Assets

To run *WipEout*, the original game data files from the PlayStation release are required:
- Place the extracted assets into `/data/wipeout/` on the console's internal storage or USB drive.
- Required files include track geometries, craft models, textures, and sound files.

---

## Building

```bash
# Run headless host self-test (checks platform glue, input bounds, audio chunk sizing)
make check

# Build freestanding target ELF payload
make elf

# Stage release artifacts per FORMATS (dist/wipeout-eboot-prospero.bin and dist/wipeout-title-prospero.zip)
make dist
```

