# Porthole — reference

The wire protocol, build, and current milestone status. The [README](../README.md) is the
overview; the overarching design spec is
[`prosperous/docs/VIDEO.md`](../../../../prosperous/docs/VIDEO.md) ("Part three: Porthole").

## Protocol architecture

Porthole operates across two dedicated TCP sockets with fixed roles and no handshake negotiation.
The serving loop waits on neither: the listeners are non-blocking and the input receive takes only
what has arrived, so video is served with no input client connected and keeps flowing while a pad
is at rest.

### Video stream (port 9805 — video out)
- **Format**: raw Annex-B H.264, start-code delimited (`00 00 00 01`), continuous Access Units.
- **Zero container overhead**: no MP4/MKV/RTP wrapper — raw NAL units directly onto the wire.
- **Host consumption**: `pros-core::watch` reads port 9805, counts bytes/units/keyframes, and pipes
  every byte unchanged into the player named in `player.txt` (default
  `mpv --demuxer=h264 --profile=low-latency --untimed --no-cache -`).

### Controller input (port 9806 — input in)
- **Format**: fixed 24-byte `PPAD` record, absolute state, sequence-numbered.
- **Timing**: 60–250 Hz with zero parsing overhead.
- **Sequencing**: a record no newer than the last applied to its slot is superseded and reported
  `PORTHOLE_STALE`, so stale sticks never replay. A new connection on 9806 is a new sender.

```
Offset  Size  Field       Description
0x00    4     magic       "PPAD" ASCII identifier
0x04    2     version     Protocol version (currently 1)
0x06    1     slot        Controller index (0..3)
0x07    1     reserved0   Must be 0x00 (checked)
0x08    4     buttons     32-bit bitmask (Ghostpad button layout)
0x0C    4     sticks      LX, LY, RX, RY (unsigned 8-bit, 128 = center)
0x10    2     triggers    L2, R2 analog pressure (0..255)
0x12    2     reserved1   Must be 0x0000
0x14    4     sequence    Per-slot monotonically increasing sequence number
```

## Freestanding discipline (obscene#D008)

Zero libc (`malloc`/`free`/`printf` never called), allocation-free (static or caller-provided
buffers only), no float or variadics. Dynamic import tables do not auto-bind in unsigned payload
mode, so Porthole loads its encoder module via `sceSysmoduleLoadModule` and walks the live kernel
export table to resolve entry points (obscene#D277 / obscene#D300).

## Build & verification

```bash
make check                          # host selftest — wire-contract validation
make elf                            # target plain-ELF payload (build/porthole.elf)
make elf PORTHOLE_ENCODER_SESSION=1 # opt in to the hardware encoder session
```

The host selftest asserts `sizeof(porthole_pad) == 24`, checks the struct offsets against the wire
spec, decodes valid `PPAD` packets and rejects malformed ones. The encoder session is compiled out
by default (its four struct-taking calls pass layouts not yet confirmed against the platform, D003),
and the selftest asserts that default, so a build that turned it on by accident fails `make check`.

Deploying is [Prosperous](../../../../prosperous/)'s job:

```bash
pros register <address>
pros check
pros send build/porthole.elf --seconds 5
pros logs --seconds 30
```

Porthole's log lines carry a `[PORTHOLE]` prefix.

## Milestones

- **M0 — Go/No-Go**: answered on hardware. The encoder sysmodule load succeeds and the module loads.
- **M1 — Dynamic load & symbol self-resolution**: done — sysmodule load, live export-table walk,
  self-resolved encoder function pointers, plain-ELF payload build.
- **M2 — Encoder session init**: **blocked on the payload route.** The encoder sysmodule load is
  refused for unsigned payloads and no encoder entry point resolves by any route, so there is
  nothing to open a session against; the gate now suppresses the load itself. See D004.
- **M3–M4 — First encoded AU, streaming loop**: pending M2.
- **M5 — Controller input loop**: the socket, record checks and per-slot sequencing are done;
  **applying a decoded record is blocked on the payload route** (the pad symbols don't resolve),
  and the startup log says plainly that records are received but not applied. See D004.
- **M6 — Resident remote-play daemon**: the end-to-end goal.
