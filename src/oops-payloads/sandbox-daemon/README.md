# sandbox-daemon

<p align="center">
  <img src="../../../common/assets/no-logo.svg" alt="No logo yet" width="200">
</p>

An on-demand filesystem-namespace and unsandboxing daemon for Prospero.

## About

sandbox-daemon runs as a background service and, on request, grants a Big App the directory and
credential changes it needs to reach global storage.

- **Event-driven.** It sleeps in the kernel's socket wait queue until a client connects - no
  polling, no CPU, no disk I/O while idle.
- **Nothing hardcoded.** It reads the running kernel to resolve the values it needs rather than
  baking in a firmware-specific address.
- **A small, fixed protocol** - a 4-byte request, a one-word reply - over a loopback socket.

## Screenshot

<p align="center">
  <img src="../../../common/assets/no-screenshot.svg" alt="No screenshot yet" width="600">
</p>

## Docs

- [Reference](docs/REFERENCE.md) - the role, the event loop, and the protocol.
- [oops-apps catalog and guide](../../../docs/USER_GUIDE.md)
