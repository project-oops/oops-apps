# pltauth-patch

<p align="center">
  <img src="../../../common/assets/no-logo.svg" alt="No logo yet" width="200">
</p>

A SceShellCore entitlement patch that lets native Big App homebrew launch.

## About

On Prospero, a native Big App (category 0) is the only execution mode that gets direct HDMI
scanout and direct GPU access. Without a retail license ticket, the userland gatekeeper inside
SceShellCore refuses the entitlement check and terminates the process. pltauth-patch is what makes
that mode available to homebrew.

- **Applies a verified patch set** to SceShellCore's resident memory, so the gatekeeper stops
  terminating unlicensed native Big Apps.
- **Persists for the whole console uptime** — applied once, native Big App 0 titles launch and
  run uninterrupted.

## Screenshot

<p align="center">
  <img src="../../../common/assets/no-screenshot.svg" alt="No screenshot yet" width="600">
</p>

## Docs

- [oops-apps catalog & guide](../../../docs/USER_GUIDE.md)
