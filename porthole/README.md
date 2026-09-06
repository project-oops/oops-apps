# Porthole - DIY Remote-Play Payload for the Prospero generation

Porthole is a freestanding, zero-dependency remote-play payload for a jailbroken Prospero-generation console. It provides our own video stream out and our own controller state in, communicating directly with PC host tooling (`pros-core::watch` in [Prosperous](../../prosperous)) without speaking the vendor's Remote Play protocol.

Because the console is jailbroken and runs our code, the protocol is a design decision rather than a reverse-engineering problem: no PIN pairing, no per-session AES-GCM crypto handshakes, and no AGPL Chiaki client dependencies.

The overarching design specification lives in [`prosperous/docs/VIDEO.md`](../../prosperous/docs/VIDEO.md) ("Part three: Porthole"). This directory contains the on-console payload implementation, built and tested as an independent subtree.

---

## Features & Protocol Architecture

Porthole operates across two dedicated TCP sockets with fixed roles and no handshake negotiation:

```
           +-------------------------------------------------------------+
           |                  Prospero Target Payload                    |
           |                                                             |
           |   [Hardware Encoder]           [Pad State Injector]         |
           |    (libSceVencCore)               (Ghostpad API)            |
           +--------------+------------------------------^---------------+
                          |                              |
             Port 9805    |                              |   Port 9806
            (Video Out)   |                              |  (Input In)
                          v                              |
           +--------------+------------------------------+---------------+
           |                     PC Host Client                          |
           |                 (prosperous / mpv)                          |
           +-------------------------------------------------------------+
```

### 1. Video Stream (Port 9805 - Video Out)
* **Format**: Raw Annex-B H.264 stream, start-code delimited (`00 00 00 01`), continuous Access Units (AUs).
* **Zero Container Overhead**: No MP4, MKV, RTP, or custom container wrapping. Porthole emits raw NAL units directly onto the wire.
* **Host Consumption**: `pros-core::watch` reads chunks from port 9805, meters bitrate and frame rate, and pipes stdout directly into `mpv --demuxer=lavf --demuxer-lavf-format=h264 -`.

### 2. Controller Input (Port 9806 - Input In)
* **Format**: Fixed 24-byte `PPAD` binary record, absolute state, sequence-numbered.
* **Timing**: Streamed at 60 Hz to 250 Hz with zero parsing overhead.
* **Pad Layout**: Direct Ghostpad button-bit mapping and stick ranges, confirmed against real hardware. A trigger actuation sets both its digital bit and its analog pressure byte.

```
Offset  Size  Field        Description
0x00    4     magic        "PPAD" ASCII identifier
0x04    2     version      Protocol version (currently 1)
0x06    1     slot         Controller index (0..3)
0x07    1     reserved0    Must be 0x00 (checked)
0x08    4     buttons      32-bit bitmask (Ghostpad button layout)
0x0C    4     sticks       LX, LY, RX, RY (unsigned 8-bit, 128 = center)
0x10    2     triggers     L2, R2 analog pressure (0..255)
0x12    2     reserved1    Must be 0x0000 (reserved for gyro/touchpad)
0x14    4     sequence     Per-slot monotonically increasing sequence number
```

### 3. Freestanding C Discipline (D008)
* **Zero Libc**: No `malloc`, `free`, `printf`, or standard C runtime calls.
* **Allocation-Free**: Static buffers and caller-provided memory only; zero runtime heap allocation.
* **No Float / Variadics**: Pure integer and bitwise manipulation.
* **Dynamic Kernel Self-Resolution**: Dynamic import tables do not auto-bind in unsigned payload mode (`elfldr`). Porthole loads `VENC` via `sceSysmoduleLoadModule(0x00A0)` and walks the live kernel export table (`kproc + 0x3E8`) to resolve `libSceVencCore` entry points (D277/D300).

---

## File Structure

| File | Role | Execution Target |
|---|---|---|
| [`porthole.h`](porthole.h) | Authoritative wire contract, `porthole_pad` 24-byte record layout, status enum, and encoder API function pointers. | Both (host & target) |
| [`porthole.c`](porthole.c) | Core logic: freestanding `porthole_pad_decode`, dynamic encoder sysmodule load (`0x00A0`), kernel export table traversal (D277/D300), and capture loops. | Both (target active, host stubs) |
| [`porthole_main.c`](porthole_main.c) | Standalone payload entry point (`porthole_start`), syscall trampoline bootstrapping, progress logging via `klog`. | Target (FreeBSD ELF64) |
| [`porthole_selftest.c`](porthole_selftest.c) | Host-side verification: compile-time `_Static_assert` on struct offsets/sizes, round-trip record decoding, fuzz/rejection of bad records, refusal on host. | Host |
| [`Makefile`](Makefile) | Independent build system providing `check` (host selftest), `skeleton` (freestanding target object), and `elf` (plain ELF payload). | Host & WSL |

---

## Build & Verification

Porthole builds independently from the top-level repository or through the root `./bin/obscene` CLI:

### 1. Host Selftest (Wire Contract Validation)
Verifies that `sizeof(porthole_pad) == 24`, struct offsets match the wire specification, valid `PPAD` packets decode accurately, invalid packets (wrong magic, bad slot, non-zero reserved bytes) are rejected, and host returns `PORTHOLE_NO_ENCODER`:

```bash
# Inside porthole/
make check

# Or from repository root
./bin/obscene porthole-check
```

### 2. Build Target Plain-ELF Payload (`porthole.elf`)
Cross-compiles freestanding C for `x86_64-unknown-freebsd` with `-nostdlib`, `-fPIC`, and 16 KB page alignment (`-Wl,-z,max-page-size=0x4000`), generating an ELF executable accepted by `elfldr`:

```bash
# Inside porthole/
make elf

# Or from repository root
./bin/obscene porthole-build
```

The output binary is staged to `porthole/build/porthole.elf` and copied to `build/porthole.elf`.

### 3. Deploy and Run on the Console
Sends `porthole.elf` to the target console via `elfldr` (port 9021/9020) and streams system execution logs back to `reports/hardware/porthole-klog.txt`:

```bash
# From repository root
./bin/obscene porthole

# Dry run (build only, stop before transmitting)
./bin/obscene porthole --build-only
```

---

## Roadmap & Milestones

* **M0 - Go/No-Go Answered** : Completed on hardware (2026-09-01). Section `106-encoder` proved `sceSysmoduleLoadModule(0x00A0)` succeeds (`rc 0x0`) and loads `libSceVencCore` at handle `0x14`.
* **M1 - Dynamic Load & Symbol Self-Resolution** : Completed (2026-09-03). Implemented dynamic loading of sysmodule `0x00A0`, live export table walk across `kproc + 0x3E8` (D277/D300), self-resolution of `sceVencCore*` function pointers, and plain-ELF payload build (`build/porthole.elf`).
* **M2 - Encoder Session Initialization**: Confirm `QueryMemorySize` and `CreateEncoder` parameter structures from public toolchain sources; safely open an encoder session.
* **M3 - First Encoded Access Unit**: Feed a test surface into `SetInputFrame`, invoke `GetAuData`, and retrieve a valid Annex-B H.264 Access Unit.
* **M4 - Video Streaming Loop**: Open listening socket on port 9805, stream encoded AUs continuously, verify playback via `pros-core::watch` and `mpv`.
* **M5 - Controller Input Loop**: Open listening socket on port 9806, receive and validate 24-byte `PPAD` records, inject decoded inputs into target pad state using Ghostpad mapping.
* **M6 - Resident Remote-Play Daemon**: Full background payload operational end-to-end on target hardware.
