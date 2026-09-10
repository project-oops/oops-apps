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

The serving loop waits on neither socket. The listeners are non-blocking and the input receive takes only what has already arrived, so video is served with no input client connected, and keeps flowing while a pad is at rest and the host is quiet.

### 1. Video Stream (Port 9805 - Video Out)
* **Format**: Raw Annex-B H.264 stream, start-code delimited (`00 00 00 01`), continuous Access Units (AUs).
* **Zero Container Overhead**: No MP4, MKV, RTP, or custom container wrapping. Porthole emits raw NAL units directly onto the wire.
* **Host Consumption**: `pros-core::watch` reads from port 9805, counts bytes, units and keyframes and measures the rate as they pass, and writes every byte unchanged into the standard input of the player named in `player.txt` - by default `mpv --demuxer=h264 --profile=low-latency --untimed --no-cache -`, the same line Prosperous writes as its example.

### 2. Controller Input (Port 9806 - Input In)
* **Format**: Fixed 24-byte `PPAD` binary record, absolute state, sequence-numbered.
* **Timing**: Streamed at 60 Hz to 250 Hz with zero parsing overhead.
* **Pad Layout**: Direct Ghostpad button-bit mapping and stick ranges, confirmed against real hardware. A trigger actuation sets both its digital bit and its analog pressure byte.
* **Sequencing**: a record no newer than the last applied to its slot is superseded and reported as `PORTHOLE_STALE`, so stale sticks never replay. A new connection on 9806 is a new sender: its count starts over, and the previous sender's high mark is forgotten for every slot.

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

Porthole builds on its own, or through the repository's `./bin/oops-apps` CLI, which reaches each app through its Makefile. Deploying is [Prosperous](../../prosperous)'s job, as the host half of this path.

### 1. Host Selftest (Wire Contract Validation)
Verifies that `sizeof(porthole_pad) == 24`, struct offsets match the wire specification, valid `PPAD` packets decode accurately, invalid packets (wrong magic, bad slot, non-zero reserved bytes) are rejected, and host returns `PORTHOLE_NO_ENCODER`:

```bash
# Inside porthole/
make check

# Or from the repository root
./bin/oops-apps check porthole
```

### 2. Build Target Plain-ELF Payload (`porthole.elf`)
Cross-compiles freestanding C for `x86_64-unknown-freebsd` with `-nostdlib`, `-fPIC`, and 16 KB page alignment (`-Wl,-z,max-page-size=0x4000`), generating an ELF executable accepted by `elfldr`:

```bash
# Inside porthole/
make elf

# Or from the repository root, which also runs the selftest and the skeleton
./bin/oops-apps build porthole
```

The payload is `build/porthole.elf`. `make dist` (or `./bin/oops-apps dist porthole`) stages a copy under `dist/`, which is what CI publishes.

By default the payload is built **without** the hardware encoder session. The four struct-taking encoder calls (`QueryMemorySize`, `CreateEncoder`, `SetInputFrame`, `GetAuData`) pass parameter layouts not yet confirmed against the platform, so they are compiled in only on request, and a default build serves the template stream instead (D003). Once the layouts are confirmed:

```bash
make elf PORTHOLE_ENCODER_SESSION=1
```

The host selftest asserts that the default is the gate, so a build that turned it on by accident fails `make check`.

### 3. Deploy and Run on the Console
Deploying uses `pros`, the Prosperous command line. It speaks to the services the jailbreak already runs and invents nothing: `elfldr` on 9021 takes a payload and runs it, and `klogsrv` on 3232 serves the system log that Porthole writes its progress into.

```bash
# Once: remember the console under a name
pros register <address>

# Confirm the loader and the log service are answering
pros check

# Send the payload; elfldr runs it. --seconds is how long to listen for what it prints
pros send build/porthole.elf --seconds 5

# Then read Porthole's own progress out of the system log
pros logs --seconds 30
```

Porthole's lines in that log carry a `[PORTHOLE]` prefix. A default build reports the resolved encoder symbols, then that the encoder session is gated off, then that the dual-socket server is starting. Two lines mean a socket constant did not survive contact with the platform, and are worth reading before anything else: `listeners could not be made non-blocking`, and `input connection closed, receive returned:` followed by the code.

Once it is serving, the stream panel in the Prosperous window is Porthole's own controls: *watch* connects to 9805 and pipes the stream into the player named in `player.txt`, and the input line connects to 9806. The payload runs until its process ends - nothing on the network stops it - so decide beforehand whether that is a kill from `pros sh` or a reboot.

---

## Roadmap & Milestones

* **M0 - Go/No-Go Answered** : Completed on hardware (2026-09-01). Section `106-encoder` proved `sceSysmoduleLoadModule(0x00A0)` succeeds (`rc 0x0`) and loads `libSceVencCore` at handle `0x14`.
* **M1 - Dynamic Load & Symbol Self-Resolution** : Completed (2026-09-03). Implemented dynamic loading of sysmodule `0x00A0`, live export table walk across `kproc + 0x3E8` (D277/D300), self-resolution of `sceVencCore*` function pointers, and plain-ELF payload build (`build/porthole.elf`).
* **M2 - Encoder Session Initialization**: **Blocked on the payload route, measured 2026-09-09.** The VENC sysmodule load is refused for unsigned payloads (`0xa0020101`, a privilege refusal raised as a signal) and no `sceVencCore` entry point resolves by any route, so there is nothing to open a session against. The gate (`PORTHOLE_ENCODER_SESSION`) now also suppresses the load itself, because tripping the refusal risks killing the payload before it opens a socket. See D004.
* **M3 - First Encoded Access Unit**: Feed a test surface into `SetInputFrame`, invoke `GetAuData`, and retrieve a valid Annex-B H.264 Access Unit.
* **M4 - Video Streaming Loop**: Open listening socket on port 9805, stream encoded AUs continuously, verify playback via `pros-core::watch` and `mpv`.
* **M5 - Controller Input Loop**: The socket, the record checks and the per-slot sequencing are done. **Injection is blocked on the payload route, measured 2026-09-09**: none of the eight `scePad*` symbols resolves by any route and the libScePad sysmodule load answers `0x805a1000`, so a decoded record has nothing to be handed to. Records are received, checked and tracked, and the startup log says plainly that they are not injected. Whether any other leg reaches the virtual-device API is an open question. See D004.
* **M6 - Resident Remote-Play Daemon**: Full background payload operational end-to-end on target hardware.
