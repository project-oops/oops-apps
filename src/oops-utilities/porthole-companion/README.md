# porthole-companion

<p align="center">
  <img src="assets/icon0.png" alt="Porthole Companion" width="160">
</p>

A clean-room, freestanding native BigApp (`PORT00002`) for Prospero-generation consoles that
monitors, controls, and stages the Porthole Remote Play streaming daemon.

## About

`porthole-companion` provides an on-console graphical dashboard for Porthole with zero manual setup
required for end users:

- **Bundled out of the box** - ships with `porthole.elf` packaged inside the application directory
  (`/app0/payload/porthole.elf`). Users install only the BigApp package.
- **One-touch daemon dispatch** - streams the payload directly over TCP to `127.0.0.1:9021`
  (`elfldr`) from the menu.
- **Port reachability probes** - tests TCP ports 9021 (`elfldr` daemon loader required for payload
  dispatch), 9805 (video out), and 9806 (controller input and control plane) to give immediate visual
  confirmation of daemon and loader readiness.
- **Connection guidance** - queries the live network interface (`oops_net_ctl_get_info`) to
  display the console's IPv4 address and instructions for PC clients (`pros watch`, Moonlight, `mpv`).
- **On-demand keyframe injection** - transmits a 24-byte `PCTL` opcode 1 record to port 9806 to
  trigger immediate IDR keyframe emission.
- **In-app GitHub updates** - fetches the latest `porthole-prospero.elf` directly from GitHub
  releases over HTTPS (`oops/http.h`) and stages it to `/data/pldmgr/payloads/porthole/porthole.elf`.
  A staged update in `/data` takes precedence over the `/app0` copy without reinstalling the BigApp.

## Controls

| Button | Action |
|---|---|
| **D-Pad Up / Down** | Navigate action menu |
| **Cross (X)** | Execute selected action |
| **Triangle** | Download or update latest payload from GitHub |
| **L1 / R1** | Refresh network interface and daemon status |
| **Circle** | Exit to console home screen |

## Building

```bash
make check      # Host selftest (state machine, menu navigation, offscreen rendering)
make elf        # Target freestanding ELF binary
make title      # Conforming BigApp directory (PORT00002)
make package    # Title package bundled with porthole.elf in dist/
```

Packaging builds `src/oops-payloads/porthole` automatically and stages the resulting ELF into
`build/title/PORT00002/payload/porthole.elf` before assembling the distribution archive.

## Installation and deployment

Deploy the packaged title archive directly to the console:

```bash
pros restore <console-ip> dist/porthole-companion-title-prospero.zip
```

Or extract the directory contents to `/data/homebrew/PORT00002/`.

## Docs

- [Reference manual](docs/REFERENCE.md) - internal architecture, state machine, and port contracts.
- [Porthole daemon payload](../../oops-payloads/porthole/README.md) - streaming daemon wire protocol and build.
