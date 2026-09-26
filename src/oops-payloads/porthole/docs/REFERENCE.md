# Porthole - reference

Wire protocol specification, socket contracts, record layouts, and freestanding architecture.
The [README](../README.md) provides an overview. The host design specification is
[`prosperous/docs/VIDEO.md`](../../../../prosperous/docs/VIDEO.md).

## Protocol architecture

Porthole operates across two dedicated TCP sockets with fixed roles and no handshake negotiation.
Listeners are non-blocking: video is served regardless of input client connection state, and pad
inputs are processed as they arrive.

### Video stream (port 9805 - video out)

- **Format**: raw Annex-B H.264, start-code delimited (`00 00 00 01`), continuous Access Units.
- **Framing**: zero container overhead (no MP4, MKV, or RTP wrapper); raw NAL units stream
  directly over TCP.
- **Consumption**: `pros watch` pipes incoming bytes unchanged into a low-latency player:
  `mpv --demuxer=h264 --profile=low-latency --untimed --no-cache -`.

### Controller input and control plane (port 9806 - input and control in)

Port 9806 accepts 24-byte binary records, multiplexed by a 4-byte ASCII magic header.

#### Pad input record (`PPAD`)

A fixed 24-byte record carrying absolute controller state:

```
Offset  Size  Field       Description
0x00    4     magic       "PPAD" (0x50504144)
0x04    2     version     Protocol version (1)
0x06    1     slot        Controller index (0..3)
0x07    1     reserved0   Must be 0x00
0x08    4     buttons     32-bit bitmask (Ghostpad button layout)
0x0C    4     sticks      LX, LY, RX, RY (unsigned 8-bit, 128 = center)
0x10    2     triggers    L2, R2 analog pressure (0..255)
0x12    2     reserved1   Must be 0x0000
0x14    4     sequence    Per-slot monotonic sequence counter
```

Sequencing: a record with a sequence counter less than or equal to the last applied sequence for
its slot is rejected as stale (`PORTHOLE_STALE`), preventing out-of-order stick replay.

#### Control record (`PCTL`)

A fixed 24-byte record for out-of-band stream commands:

```
Offset  Size  Field         Description
0x00    4     magic         "PCTL" (0x5043544C)
0x04    2     version       Protocol version (1)
0x06    2     opcode        Command opcode (1 = request keyframe, 2 = set mode)
0x08    2     width         Target width in pixels (e.g. 1920)
0x0A    2     height        Target height in pixels (e.g. 1080)
0x0C    2     fps           Target frame rate (e.g. 60)
0x0E    2     codec         Codec identifier (1 = H.264, 2 = HEVC)
0x10    4     bitrate_kbps  Target bitrate in kilobits per second
0x14    4     sequence      Monotonic control sequence counter
```

Supported opcodes:
- `opcode 1` (`PORTHOLE_CTL_OP_KEYFRAME`): requests immediate IDR keyframe emission on the next
  encoded frame. Repeat requests collapse into a single keyframe.
- `opcode 2` (`PORTHOLE_CTL_OP_SET_MODE`): reconfigures video stream dimensions, target frame
  rate, codec, and bitrate. Clamps unsupported parameters to valid ranges and requests an immediate
  IDR keyframe to begin the new stream parameters.

## Freestanding architecture

Porthole compiles under `-nostdlib -ffreestanding`:
- Zero libc dependencies (`malloc`, `free`, `printf` are not called).
- Static or caller-provided buffers only; no dynamic heap allocations.
- No floating-point operations or variadic functions.
- Symbol resolution walks the live kernel export table.

## Building and verification

```bash
make check                          # Host selftest - wire-contract and record decoding validation
make elf                            # Target freestanding plain-ELF payload (build/porthole.elf)
make elf PORTHOLE_ENCODER_SESSION=1 # Enable hardware encoder session integration
```

The host selftest verifies that `sizeof(porthole_pad) == 24` and `sizeof(porthole_ctl) == 24`,
validates struct offsets, tests valid record decoding, and checks rejection of malformed or
stale packets.

## Deployment

Porthole can be deployed in two ways:

1. **Porthole Companion (`PORT00002`)**: The native BigApp title bundles `porthole.elf` within its
   package (`/app0/payload/porthole.elf`) and streams it to `127.0.0.1:9021` (`elfldr`) or
   dispatches it via `shsrv` (`:2323`).
2. **Direct dispatch via Prosperous**:
   ```bash
   pros send build/porthole.elf --seconds 5
   ```
