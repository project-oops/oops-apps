# porthole-companion - reference

Internal architecture, port contracts, state model, and payload bundling for the companion app.
The [README](../README.md) provides an overview.

## Architecture

`porthole-companion` uses a strict model and host separation:

1. **Model and UI renderer (`porthole_companion.c`)**:
   - Pure state operations: initialization, menu navigation, state transitions.
   - 2D Canvas rendering using `oops/draw.h` into an `oops_surface_t`.
   - Free of platform-specific syscalls or network operations; fully verified by host selftest (`make check`).
2. **Platform host (`porthole_companion_main.c`)**:
   - Opens the hardware display via `oops_display_open()` (AGC hardware presentation or software fallback).
   - Polls DualSense controllers via `oops_input_poll()`.
   - Queries IP address and link state via `oops_net_ctl_get_info()`.
   - Performs non-blocking socket probes across ports 9021, 9805, and 9806.
   - Performs streaming downloads over TLS via `oops_http_get_to_file_cb()`.
   - Streams ELF payload binaries to `127.0.0.1:9021`.

## State model and probe lifecycle

The dashboard tracks the state of each network port via `port_probe_state`:

| State | Indicator | Description |
|---|---|---|
| `PORT_PROBE_INACTIVE` | Gray (`STOPPED`) | Socket is closed or unreachable |
| `PORT_PROBE_WAITING` | Yellow (`CONNECTING`) | Non-blocking connect handshake in progress |
| `PORT_PROBE_ACTIVE` | Green (`LISTENING`) | Connection succeeded; daemon endpoint is responsive |
| `PORT_PROBE_ERROR` | Red (`ERROR`) | Connection attempt failed or timed out |

The host runs non-blocking probes at periodic intervals without stalling the 60 Hz display loop.

## Network contracts

- **Port 9021 (ELF loader)**:
  - Protocol: Raw ELF binary stream over TCP.
  - Probe: Non-blocking TCP connect test. Confirms whether `elfldr` is listening to accept payloads.
  - Action: "Start / Restart Daemon" resolves the local payload, connects to `127.0.0.1:9021`, and
    streams the binary in 16 KB chunks. If `elfldr` is stopped, it checks whether a local shell
    service (`:2323`) is listening to bootstrap `elfldr` automatically before alerting the user.
- **Port 9805 (Video stream out)**:
  - Protocol: TCP raw Annex-B H.264 stream.
  - Probe: Non-blocking TCP connect test. If connect returns 0 or completes during `select`/poll,
    the video listener is confirmed active.
- **Port 9806 (Controller input and control plane)**:
  - Protocol: Multiplexed 24-byte records (`PPAD` input state or `PCTL` control opcode).
  - Probe: Non-blocking TCP connect test.
  - Action: "Force IDR Keyframe" menu item opens a short-lived connection and sends a 24-byte `PCTL`
    opcode 1 record (`magic` = `'P','C','T','L'`, `opcode` = `1`) to request an immediate keyframe.

## Payload bundling and synchronization

### Build-time bundling

When packaging the companion BigApp (`make package`), `porthole-companion/Makefile` invokes
`make -C $(PORTHOLE_PAYLOAD_DIR) elf` via an explicit make prerequisite. The resulting payload
binary (`src/oops-payloads/porthole/build/porthole.elf`) is copied into
`build/title/PORT00002/payload/porthole.elf` and packaged directly into
`dist/porthole-companion-title-prospero.zip`.

### Header synchronization

Both daemon and companion share protocol constants and structures directly:
- `porthole-companion/Makefile` adds `-I$(PORTHOLE_PAYLOAD_DIR)` to compilation flags.
- `porthole_companion.h` and `porthole_companion_main.c` derive port numbers (`PORTHOLE_PORT_VIDEO`,
  `PORTHOLE_PORT_INPUT`) and control structures (`porthole_ctl`, `PCTL` magic/opcodes) directly
  from `<porthole.h>`.

### Runtime payload resolution

1. The companion checks for `/data/pldmgr/payloads/porthole/porthole.elf` first.
2. If absent, it falls back to the immutable bundled copy in `/app0/payload/porthole.elf`.
3. In-app downloads via **Triangle** save to `/data/pldmgr/payloads/porthole/porthole.elf`, allowing
   updates to take effect immediately without reinstalling the BigApp title.
