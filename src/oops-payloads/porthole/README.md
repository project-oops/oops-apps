# Porthole

<p align="center">
  <img src="../../../common/assets/no-logo.svg" alt="No logo yet" width="200">
</p>

A freestanding, zero-dependency remote-play daemon payload for Prospero-generation hardware.

## About

Porthole captures video frames directly from the hardware display pipeline and serves raw H.264
Access Units over TCP port 9805. It receives controller inputs and stream control commands over
TCP port 9806. Host tooling (`pros watch` in [Prosperous](../../../../prosperous/), Moonlight via
`pros moonlight`, or `mpv`) connects directly to these ports without proprietary Remote Play
protocols, encryption handshakes, or PIN pairing.

- **Dual-socket wire protocol** - fixed TCP sockets: port 9805 streams raw start-code-delimited
  H.264 Access Units directly to video players; port 9806 multiplexes 24-byte `PPAD` controller
  records and 24-byte `PCTL` control records (such as on-demand IDR keyframe requests).
- **Freestanding C runtime** - no libc, no heap allocations, no floating point operations, and
  no variadics (`-nostdlib -ffreestanding`). Symbol resolution uses dynamic export-table lookup.
- **Uncoupled serving loop** - video frames flow continuously whether an input client is connected
  or absent. Controller inputs apply as they arrive.
- **On-console management** - [Porthole Companion](../../oops-utilities/porthole-companion/README.md)
  (`PORT00002`) provides a native BigApp dashboard to monitor port reachability, inspect console
  network configuration, and stage or dispatch the payload via `elfldr` (:9021) or root shell (`shsrv` :2323).

## Building

```bash
make check      # Host selftest - wire-contract and state validation
make elf        # Target freestanding plain-ELF payload (build/porthole.elf)
```

## Running

### On-console via Porthole Companion

Install and run [Porthole Companion](../../oops-utilities/porthole-companion/README.md) (`PORT00002`).
The companion bundles `porthole.elf` within its application directory and dispatches it directly
via `elfldr` (:9021) or the local root shell (:2323).

### Standalone dispatch

With `elfldr` running on the target console (port 9021):

```bash
pros send build/porthole.elf --seconds 5
```

### Host client connection

Stream video to `mpv` through Prosperous:

```bash
pros watch <console-ip>
```

Or connect low-latency players directly:

```bash
mpv --demuxer=h264 --profile=low-latency --untimed --no-cache tcp://<console-ip>:9805
```

## Docs

- **[Reference](docs/REFERENCE.md)** - wire protocol specification, socket contracts, and record layouts.
- [Design spec: prosperous/docs/VIDEO.md](../../../../prosperous/docs/VIDEO.md) - architecture and video pipeline.
- [Porthole Companion](../../oops-utilities/porthole-companion/README.md) - native on-console management title.
